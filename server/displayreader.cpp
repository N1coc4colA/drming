#include "displayreader.h"

#include <sys/mman.h>

#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>

#include <cstring>

#include "sse.h"

#ifndef Q_MOC_RUN
#include <ctrack.hpp>
#endif


namespace Drm {

QString drmDeviceForConnector(const QString &connectorName)
{
    CTRACK;

    static QHash<QString, QString> cache;
    if (const auto it = cache.constFind(connectorName); it != cache.constEnd()) {
        return *it;
    }

    const QDir drmDir(QStringLiteral("/dev/dri"));
    const auto cards = drmDir.entryInfoList({QStringLiteral("card[0-9]*")},
                                            QDir::Files | QDir::Readable | QDir::System,
                                            QDir::Name);

    for (const QFileInfo &cardInfo : cards) {
        const QByteArray cardPathBytes = QFile::encodeName(cardInfo.absoluteFilePath());
        const int fd = open(cardPathBytes.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) [[unlikely]] {
            continue;
        }

        const DrmResourcesPtr resources(drmModeGetResources(fd));
        if (!resources) {
            close(fd);
            continue;
        }

        for (int i = 0; i < resources->count_connectors; ++i) {
            const DrmConnectorPtr connector(drmModeGetConnector(fd, resources->connectors[i]));
            if (!connector) {
                continue;
            }

            const QString sysfsName = QStringLiteral("%1-%2-%3")
                                          .arg(cardInfo.fileName(),
                                               QString::fromLatin1(drmModeGetConnectorTypeName(connector->connector_type)),
                                               QString::number(connector->connector_type_id));

            if (connectorName == sysfsName) {
                close(fd);
                cache.insert(connectorName, cardInfo.absoluteFilePath());
                return cardInfo.absoluteFilePath();
            }
        }

        close(fd);
    }

    return {};
}

/*
 * Maps a DRM handle into this process. When writable is true, mmap is issued
 * with PROT_READ|PROT_WRITE so callers may composite in-place; otherwise the
 * mapping is read-only. The buffer is owned by the caller; pair it with a
 * munmap() of the returned pointer.
 */
bool mapFramebuffer(void *&dataOut, const int drmFd, const uint32_t handle, const size_t size, const bool writable)
{
    CTRACK;

    dataOut = nullptr;

    if (handle == 0 || drmFd < 0 || size == 0) [[unlikely]] {
        return false;
    }

    const int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;

    drm_mode_map_dumb mapReq{};
    mapReq.handle = handle;

    if (drmIoctl(drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapReq) == 0) {
        void *p = mmap(nullptr, size, prot, MAP_SHARED, drmFd, static_cast<__off_t>(mapReq.offset));
        if (p != MAP_FAILED) {
            dataOut = p;
            return true;
        }
    }

    int primeFd = -1;
    if (drmPrimeHandleToFD(drmFd, handle, DRM_CLOEXEC | DRM_RDWR, &primeFd) == 0) {
        void *p = mmap(nullptr, size, prot, MAP_SHARED, primeFd, 0);
        if (p != MAP_FAILED) {
            dataOut = p;
            close(primeFd);
            return true;
        }

        close(primeFd);
    }

    return false;
}

} // namespace Drm


DisplayReader::DisplayReader(QString connectorName, const bool writableMapping)
    : m_connectorName(std::move(connectorName))
    , m_writableMapping(writableMapping)
{}

DisplayReader::~DisplayReader()
{
    teardown();
}

void DisplayReader::teardown()
{
    for (auto &c : m_primaryBuffers) {
        if (c.data && c.size) {
            munmap(c.data, c.size);
        }
    }
    m_primaryBuffers.clear();

    for (auto &c : m_cursorBuffers) {
        if (c.data && c.size) {
            munmap(c.data, c.size);
        }
    }
    m_cursorBuffers.clear();

    m_cursorPlanes.clear();

    if (m_drmFd >= 0) {
        close(m_drmFd);
        m_drmFd = -1;
    }

    m_crtcId = 0;
    m_crtc.reset();
    m_connectorPath.clear();
}

bool DisplayReader::ensureInitialised()
{
    CTRACK;

    if (m_drmFd >= 0) {
        return true;
    }

    if (m_connectorPath.isEmpty()) {
        m_connectorPath = Drm::drmDeviceForConnector(m_connectorName);
        if (m_connectorPath.isEmpty()) [[unlikely]] {
            return false;
        }
    }

    const QByteArray drmPathBytes = QFile::encodeName(m_connectorPath);
    m_drmFd = open(drmPathBytes.constData(), O_RDWR | O_CLOEXEC);
    if (m_drmFd < 0) [[unlikely]] {
        m_connectorPath.clear();
        return false;
    }

    // These caps must be set before plane enumeration, otherwise the kernel
    // hides cursor and overlay planes from us. They are one-shot per fd.
    drmSetClientCap(m_drmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmSetClientCap(m_drmFd, DRM_CLIENT_CAP_ATOMIC, 1);

    const Drm::DrmResourcesPtr resources(drmModeGetResources(m_drmFd));
    if (!resources) {
        goto err;
    }

    {
        Drm::DrmConnectorPtr connector{};
        for (int i = 0; i < resources->count_connectors; ++i) {
            connector.reset(drmModeGetConnector(m_drmFd, resources->connectors[i]));
            if (!connector) {
                continue;
            }

            const auto sysfsName = QStringLiteral("%1-%2-%3").arg(
                QFileInfo(m_connectorPath).fileName(),
                QString::fromLatin1(drmModeGetConnectorTypeName(connector->connector_type)),
                QString::number(connector->connector_type_id)
            );

            if (m_connectorName != sysfsName) {
                connector.reset();
                continue;
            }

            break;
        }

        if (!connector) {
            goto err;
        }

        Drm::DrmEncoderPtr encoder;
        if (connector->encoder_id) [[unlikely]] {
            encoder.reset(drmModeGetEncoder(m_drmFd, connector->encoder_id));
        }
        if (!encoder) [[unlikely]] {
            for (int i = 0; i < connector->count_encoders; ++i) {
                encoder.reset(drmModeGetEncoder(m_drmFd, connector->encoders[i]));
                if (encoder) {
                    break;
                }
            }
        }

        if (!encoder || !encoder->crtc_id) [[unlikely]] {
            goto err;
        }

        m_crtcId = encoder->crtc_id;
    }

    discoverCursorPlanes();
    return true;

err:
    teardown();
    return false;
}

void DisplayReader::discoverCursorPlanes()
{
    CTRACK;

    const Drm::DrmPlaneResPtr planeRes(drmModeGetPlaneResources(m_drmFd));
    if (!planeRes) {
        return;
    }

    for (uint32_t i = 0; i < planeRes->count_planes; ++i) {
        const Drm::DrmPlanePtr plane(drmModeGetPlane(m_drmFd, planeRes->planes[i]));
        if (!plane) {
            continue;
        }

        const Drm::DrmObjectPropsPtr props(drmModeObjectGetProperties(m_drmFd, plane->plane_id, DRM_MODE_OBJECT_PLANE));
        if (!props) {
            continue;
        }

        CursorPlaneInfo info;
        info.plane_id = plane->plane_id;

        bool isCursor = false;
        for (uint32_t j = 0; j < props->count_props; ++j) {
            const Drm::DrmPropertyPtr prop(drmModeGetProperty(m_drmFd, props->props[j]));
            if (!prop) {
                continue;
            }

            const QLatin1String name(prop->name);
            if (name == QLatin1String("type")) {
                info.typePropId = prop->prop_id;
                isCursor = props->prop_values[j] == DRM_PLANE_TYPE_CURSOR;
            } else if (name == QLatin1String("CRTC_X")) {
                info.crtcXPropId = prop->prop_id;
            } else if (name == QLatin1String("CRTC_Y")) {
                info.crtcYPropId = prop->prop_id;
            }
        }

        if (isCursor && info.typePropId && info.crtcXPropId && info.crtcYPropId) {
            m_cursorPlanes.push_back(info);
        }
    }
}

bool DisplayReader::getVkmsFrameBuffer(VkmsFrameBuffer &fb)
{
    CTRACK;

    fb = {};

    if (!ensureInitialised()) {
        return false;
    }

    m_crtc.reset(drmModeGetCrtc(m_drmFd, m_crtcId));
    if (!m_crtc || !m_crtc->buffer_id) [[unlikely]] {
        return false;
    }

    const uint32_t bufferId = m_crtc->buffer_id;

    auto it = m_primaryBuffers.find(bufferId);
    if (it == m_primaryBuffers.end()) {
        const Drm::DrmFB2Ptr mfb(drmModeGetFB2(m_drmFd, bufferId));
        if (!mfb || mfb->handles[0] == 0) [[unlikely]] {
            return false;
        }

        CachedPrimary cp;
        cp.width    = mfb->width;
        cp.height   = mfb->height;
        cp.stride   = mfb->pitches[0];
        cp.format   = mfb->pixel_format;
        cp.bpp      = (cp.format == DRM_FORMAT_RGB565) ? 16 : 32;
        cp.size     = static_cast<size_t>(cp.stride) * cp.height;
        cp.writable = m_writableMapping;

        if (!Drm::mapFramebuffer(cp.data, m_drmFd, mfb->handles[0], cp.size, cp.writable)) [[unlikely]] {
            return false;
        }

        if (m_primaryBuffers.size() >= kMaxCachedPrimaries) {
            auto evict = m_primaryBuffers.begin();
            if (evict->data && evict->size) {
                munmap(evict->data, evict->size);
            }
            m_primaryBuffers.erase(evict);
        }

        it = m_primaryBuffers.insert(bufferId, cp);
    }

    const auto &cp = *it;

    fb.fd        = m_drmFd;      // borrowed
    fb.data      = cp.data;      // borrowed
    fb.size      = cp.size;
    fb.buffer_fd = -1;           // not owned by the view
    fb.width     = cp.width;
    fb.height    = cp.height;
    fb.stride    = cp.stride;
    fb.format    = cp.format;
    fb.bpp       = cp.bpp;
    fb.fb_id     = bufferId;
    fb.crtc_id   = m_crtcId;

    return true;
}

void DisplayReader::releaseVkmsFrameBuffer(VkmsFrameBuffer &fb)
{
    // fd and data are borrowed from the DisplayReader cache, do not close/unmap.
    fb = {};
}

bool DisplayReader::getCursorFrameBuffer(CursorFrameBuffer &cursor, const VkmsFrameBuffer &primary)
{
    CTRACK;

    cursor = {};

    if (m_drmFd < 0 || primary.fd < 0 || primary.crtc_id == 0) [[unlikely]] {
        return false;
    }

    for (const auto &info : m_cursorPlanes) {
        const Drm::DrmPlanePtr plane(drmModeGetPlane(m_drmFd, info.plane_id));
        if (!plane) {
            continue;
        }

        if (plane->crtc_id != primary.crtc_id || plane->fb_id == 0) {
            continue;
        }

        const Drm::DrmObjectPropsPtr props(
            drmModeObjectGetProperties(m_drmFd, info.plane_id, DRM_MODE_OBJECT_PLANE));
        if (!props) {
            continue;
        }

        // Property *IDs* are cached, so this is a pure integer scan.
        uint64_t type = 0, crtcX = 0, crtcY = 0;
        for (uint32_t j = 0; j < props->count_props; ++j) {
            const auto propId = props->props[j];
            if (propId == info.typePropId) {
                type = props->prop_values[j];
            } else if (propId == info.crtcXPropId) {
                crtcX = props->prop_values[j];
            } else if (propId == info.crtcYPropId) {
                crtcY = props->prop_values[j];
            }
        }

        if (type != DRM_PLANE_TYPE_CURSOR) {
            continue;
        }

        const uint32_t bufferId = plane->fb_id;

        auto it = m_cursorBuffers.find(bufferId);
        if (it == m_cursorBuffers.end()) {
            const Drm::DrmFB2Ptr mfb(drmModeGetFB2(m_drmFd, bufferId));
            if (!mfb || mfb->handles[0] == 0) {
                continue;
            }

            CachedCursor cc;
            cc.width = mfb->width;
            cc.height = mfb->height;
            cc.stride = mfb->pitches[0];
            cc.format = mfb->pixel_format;
            cc.size = static_cast<size_t>(cc.stride) * cc.height;

            // Cursor buffers are never composited in-place, so always read-only.
            if (!Drm::mapFramebuffer(cc.data, m_drmFd, mfb->handles[0], cc.size, /*writable=*/false)) {
                continue;
            }

            if (m_cursorBuffers.size() >= kMaxCachedCursors) {
                auto evict = m_cursorBuffers.begin();
                if (evict->data && evict->size) {
                    munmap(evict->data, evict->size);
                }

                m_cursorBuffers.erase(evict);
            }

            it = m_cursorBuffers.insert(bufferId, cc);
        }

        const auto &cc = *it;

        cursor.data      = cc.data;   // borrowed
        cursor.size      = cc.size;
        cursor.buffer_fd = -1;        // owned by cache
        cursor.fd        = m_drmFd;   // borrowed
        cursor.width     = cc.width;
        cursor.height    = cc.height;
        cursor.stride    = cc.stride;
        cursor.format    = cc.format;
        // plane->x/y is the src offset within the cursor buffer (typically 0,0)
        cursor.crtc_x = static_cast<int32_t>(crtcX) + static_cast<int32_t>(plane->x);
        cursor.crtc_y = static_cast<int32_t>(crtcY) + static_cast<int32_t>(plane->y);
        return true;
    }

    return false;
}

void DisplayReader::releaseCursorFrameBuffer(CursorFrameBuffer &cursor)
{
    // data and fd are borrowed from the DisplayReader cache — do not close/unmap.
    cursor = {};
}

void DisplayReader::compositeWithCursor(const QImage &primary, const CursorFrameBuffer &cursor, const DrmFormat::FormatDescriptor &fmtDesc, QImage &output)
{
    CTRACK;

    if (!cursor.data || cursor.width == 0 || cursor.height == 0 || cursor.stride == 0) [[unlikely]] {
        return;
    }

    // Copy primary into the owned output buffer so QPainter can write without detaching the RO DRM mapping.
    if (&output != &primary) {
        if (output.size() != primary.size() || output.format() != primary.format()) [[unlikely]] {
            output = QImage(primary.size(), primary.format());
        }

        if (output.bytesPerLine() == primary.bytesPerLine()) {
            // Most likely case: same stride, contiguous buffer, only one memcpy needed.
            std::memcpy(output.bits(), primary.constBits(), output.sizeInBytes());
        } else {
            // Unlikely: fall back to row-wise.
            const int rowBytes = std::min(output.bytesPerLine(), primary.bytesPerLine());
            for (int y = 0; y < primary.height(); ++y) {
                std::memcpy(output.scanLine(y), primary.constScanLine(y), rowBytes);
            }
        }
    }

    const QImage cursorImg = imageFromFrameBuffer(
        static_cast<const uint8_t *>(cursor.data),
        cursor.width,
        cursor.height,
        cursor.stride,
        fmtDesc
    );
    if (cursorImg.isNull()) [[unlikely]] {
        return;
    }

    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(cursor.crtc_x, cursor.crtc_y, cursorImg);
    painter.end();
}

void DisplayReader::compositeWithCursorInPlace(QImage &primary, const CursorFrameBuffer &cursor, const DrmFormat::FormatDescriptor &fmtDesc)
{
    CTRACK;

    if (!cursor.data || cursor.width == 0 || cursor.height == 0 || cursor.stride == 0) [[unlikely]] {
        return;
    }

    const QImage cursorImg = imageFromFrameBuffer(
        static_cast<const uint8_t *>(cursor.data),
        cursor.width,
        cursor.height,
        cursor.stride,
        fmtDesc
    );
    if (cursorImg.isNull()) [[unlikely]] {
        return;
    }

    // `primary` was created from a writable (PROT_READ|PROT_WRITE) mapping and
    // has a native format (no swizzle), so QPainter writes land directly in
    // the mapped DRM buffer with no detach.
    QPainter painter(&primary);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(cursor.crtc_x, cursor.crtc_y, cursorImg);
    painter.end();
}

QImage DisplayReader::imageFromFrameBuffer(const uint8_t *data,
                                           const uint32_t width,
                                           const uint32_t height,
                                           const uint32_t stride,
                                           const DrmFormat::FormatDescriptor &fmtDesc,
                                           const bool writable)
{
    // Swizzles are one-way transforms; they must not run against the cached
    // DRM mapping or the next frame would be swizzled twice. Force a
    // copy-on-write wrap for any format that needs a conversion, regardless
    // of what the caller asked for.
    const bool effectiveWritable = writable && (fmtDesc.convert == nullptr);

    QImage output{};
    if (effectiveWritable) {
        output = QImage(
            const_cast<uint8_t *>(data),
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(stride),
            fmtDesc.qtFormat
        );
    } else {
        output = QImage(
            data,
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(stride),
            fmtDesc.qtFormat
        );
    }

    if (fmtDesc.convert) [[likely]] {
        fmtDesc.convert(output);
    }

    return output;
}
