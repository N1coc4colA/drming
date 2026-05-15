#include "displayreader.h"

#include <sys/mman.h>

#include <drm_fourcc.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <QDir>
#include <QFileInfo>
#include <QPainter>

namespace Drm {

struct DrmResourcesDeleter
{
    void operator()(drmModeRes *p) const noexcept
    {
        if (p) {
            drmModeFreeResources(p);
        }
    }
};

struct DrmConnectorDeleter
{
    void operator()(drmModeConnector *p) const noexcept
    {
        if (p) {
            drmModeFreeConnector(p);
        }
    }
};

struct DrmEncoderDeleter
{
    void operator()(drmModeEncoder *p) const noexcept
    {
        if (p) {
            drmModeFreeEncoder(p);
        }
    }
};

struct DrmCrtcDeleter
{
    void operator()(drmModeCrtc *p) const noexcept
    {
        if (p) {
            drmModeFreeCrtc(p);
        }
    }
};

struct DrmFB2Deleter
{
    void operator()(drmModeFB2 *p) const noexcept
    {
        if (p) {
            drmModeFreeFB2(p);
        }
    }
};

struct DrmPlaneResDeleter
{
    void operator()(drmModePlaneRes *p) const noexcept
    {
        if (p) {
            drmModeFreePlaneResources(p);
        }
    }
};

struct DrmPlaneDeleter
{
    void operator()(drmModePlane *p) const noexcept
    {
        if (p) {
            drmModeFreePlane(p);
        }
    }
};

struct DrmObjectPropsDeleter
{
    void operator()(drmModeObjectProperties *p) const noexcept
    {
        if (p) {
            drmModeFreeObjectProperties(p);
        }
    }
};

struct DrmPropertyDeleter
{
    void operator()(drmModePropertyRes *p) const noexcept
    {
        if (p) {
            drmModeFreeProperty(p);
        }
    }
};

using DrmResourcesPtr = std::unique_ptr<drmModeRes, DrmResourcesDeleter>;
using DrmConnectorPtr = std::unique_ptr<drmModeConnector, DrmConnectorDeleter>;
using DrmEncoderPtr = std::unique_ptr<drmModeEncoder, DrmEncoderDeleter>;
using DrmCrtcPtr = std::unique_ptr<drmModeCrtc, DrmCrtcDeleter>;
using DrmFB2Ptr = std::unique_ptr<drmModeFB2, DrmFB2Deleter>;
using DrmPlaneResPtr = std::unique_ptr<drmModePlaneRes, DrmPlaneResDeleter>;
using DrmPlanePtr = std::unique_ptr<drmModePlane, DrmPlaneDeleter>;
using DrmObjectPropsPtr = std::unique_ptr<drmModeObjectProperties, DrmObjectPropsDeleter>;
using DrmPropertyPtr = std::unique_ptr<drmModePropertyRes, DrmPropertyDeleter>;

// Returns the /dev/dri/cardN path that owns the given connector sysfs name
// e.g. "card0-Virtual-1" → "/dev/dri/card0"
QString drmDeviceForConnector(const QString &connectorName)
{
    const QDir drmDir(QStringLiteral("/dev/dri"));
    const auto cards = drmDir.entryInfoList({QStringLiteral("card[0-9]*")}, QDir::Files | QDir::Readable | QDir::System, QDir::Name);

    for (const QFileInfo &cardInfo : cards) {
        const QByteArray cardPathBytes = QFile::encodeName(cardInfo.absoluteFilePath());
        const int fd = ::open(cardPathBytes.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }

        DrmResourcesPtr resources(drmModeGetResources(fd));
        if (!resources) {
            ::close(fd);
            continue;
        }

        for (int i = 0; i < resources->count_connectors; ++i) {
            DrmConnectorPtr connector(drmModeGetConnector(fd, resources->connectors[i]));
            if (!connector) {
                continue;
            }

            const QString sysfsName = QStringLiteral("%1-%2-%3")
                                          .arg(cardInfo.fileName(),
                                               QString::fromLatin1(drmModeGetConnectorTypeName(connector->connector_type)),
                                               QString::number(connector->connector_type_id));

            if (connectorName == sysfsName) {
                ::close(fd);
                return cardInfo.absoluteFilePath();
            }
        }

        ::close(fd);
    }

    return {};
}

bool mapFramebuffer(VkmsFrameBuffer &fb, uint32_t handle)
{
    if (handle == 0 || fb.fd < 0 || fb.size == 0) {
        return false;
    }

    drm_mode_map_dumb mapReq{};
    mapReq.handle = handle;

    if (::drmIoctl(fb.fd, DRM_IOCTL_MODE_MAP_DUMB, &mapReq) == 0) {
        fb.data = ::mmap(nullptr, fb.size, PROT_READ, MAP_SHARED, fb.fd, static_cast<off_t>(mapReq.offset));
        if (fb.data != MAP_FAILED) {
            return true;
        }

        fb.data = nullptr;
    }

    int primeFd = -1;
    if (::drmPrimeHandleToFD(fb.fd, handle, DRM_CLOEXEC | DRM_RDWR, &primeFd) == 0) {
        fb.buffer_fd = primeFd;
        fb.data = ::mmap(nullptr, fb.size, PROT_READ, MAP_SHARED, fb.buffer_fd, 0);
        if (fb.data != MAP_FAILED) {
            return true;
        }

        fb.data = nullptr;
        ::close(fb.buffer_fd);
        fb.buffer_fd = -1;
    }

    return false;
}

bool mapCursorFramebuffer(CursorFrameBuffer &cursor, int drmFd, uint32_t handle)
{
    if (handle == 0 || drmFd < 0 || cursor.size == 0) {
        return false;
    }

    // Try dumb buffer mapping first
    drm_mode_map_dumb mapReq{};
    mapReq.handle = handle;

    if (::drmIoctl(drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapReq) == 0) {
        cursor.data = ::mmap(nullptr, cursor.size, PROT_READ, MAP_SHARED, drmFd, static_cast<off_t>(mapReq.offset));
        if (cursor.data != MAP_FAILED) {
            return true;
        }

        cursor.data = nullptr;
    }

    // Fall back to PRIME
    int primeFd = -1;
    if (::drmPrimeHandleToFD(drmFd, handle, DRM_CLOEXEC | DRM_RDWR, &primeFd) == 0) {
        cursor.buffer_fd = primeFd;
        cursor.data = ::mmap(nullptr, cursor.size, PROT_READ, MAP_SHARED, cursor.buffer_fd, 0);
        if (cursor.data != MAP_FAILED) {
            return true;
        }

        cursor.data = nullptr;
        ::close(cursor.buffer_fd);
        cursor.buffer_fd = -1;
    }

    return false;
}

} // namespace Drm

QImage::Format qtImageFormatFromDrm(const uint32_t drmFormat)
{
    switch (drmFormat) {
    case DRM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    case DRM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32;
    case DRM_FORMAT_RGB565:
        return QImage::Format_RGB16;
    default:
        return QImage::Format_Invalid;
    }
}

DisplayReader::DisplayReader(const QString &connectorName)
    : m_connectorName(connectorName)
{}

bool DisplayReader::getVkmsFrameBuffer(VkmsFrameBuffer &fb)
{
    using namespace Drm;

    fb = {};

    const QString drmDevicePath = drmDeviceForConnector(m_connectorName);
    if (drmDevicePath.isEmpty()) {
        return false;
    }

    const QByteArray drmPathBytes = QFile::encodeName(drmDevicePath);
    fb.fd = ::open(drmPathBytes.constData(), O_RDWR | O_CLOEXEC);
    if (fb.fd < 0) {
        return false;
    }

    DrmConnectorPtr connector;
    DrmEncoderPtr encoder;
    DrmCrtcPtr crtc;
    DrmFB2Ptr mfb;

    DrmResourcesPtr resources(drmModeGetResources(fb.fd));
    if (!resources) {
        goto err;
    }

    for (int i = 0; i < resources->count_connectors; ++i) {
        connector.reset(drmModeGetConnector(fb.fd, resources->connectors[i]));
        if (!connector) {
            continue;
        }

        const QString sysfsName = QStringLiteral("%1-%2-%3")
                                      .arg(QFileInfo(drmDevicePath).fileName(),
                                           QString::fromLatin1(drmModeGetConnectorTypeName(connector->connector_type)),
                                           QString::number(connector->connector_type_id));

        if (m_connectorName != sysfsName) {
            connector.reset();
            continue;
        }

        break;
    }

    if (!connector) {
        goto err;
    }

    if (connector->encoder_id) {
        encoder.reset(drmModeGetEncoder(fb.fd, connector->encoder_id));
    }

    if (!encoder) {
        for (int i = 0; i < connector->count_encoders; ++i) {
            encoder.reset(drmModeGetEncoder(fb.fd, connector->encoders[i]));
            if (encoder) {
                break;
            }
        }
    }

    if (!encoder || !encoder->crtc_id) {
        goto err;
    }

    crtc.reset(drmModeGetCrtc(fb.fd, encoder->crtc_id));
    if (!crtc || !crtc->buffer_id) {
        goto err;
    }

    mfb.reset(drmModeGetFB2(fb.fd, crtc->buffer_id));
    if (!mfb) {
        goto err;
    }

    fb.fb_id = crtc->buffer_id;
    fb.crtc_id = crtc->crtc_id;
    fb.width = mfb->width;
    fb.height = mfb->height;
    fb.stride = mfb->pitches[0];
    fb.format = mfb->pixel_format;
    fb.bpp = (fb.format == DRM_FORMAT_RGB565) ? 16 : 32;
    fb.size = static_cast<std::size_t>(fb.stride) * fb.height;

    if (!mapFramebuffer(fb, mfb->handles[0])) {
        goto err;
    }

    return true;

err:
    releaseVkmsFrameBuffer(fb);
    return false;
}

void DisplayReader::releaseVkmsFrameBuffer(VkmsFrameBuffer &fb)
{
    if (fb.data && fb.size > 0) {
        ::munmap(fb.data, fb.size);
        fb.data = nullptr;
    }
    if (fb.buffer_fd >= 0) {
        ::close(fb.buffer_fd);
        fb.buffer_fd = -1;
    }
    if (fb.fd >= 0) {
        ::close(fb.fd);
        fb.fd = -1;
    }

    fb = {};
}

bool DisplayReader::getCursorFrameBuffer(CursorFrameBuffer &cursor, const VkmsFrameBuffer &primary)
{
    using namespace Drm;

    cursor = {};

    if (primary.fd < 0 || primary.crtc_id == 0) {
        qWarning() << "Invalid primary";
        return false;
    }

    // Must be set before drmModeGetPlaneResources, otherwise the kernel
    // hides cursor and overlay planes from us.
    drmSetClientCap(primary.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    DrmPlaneResPtr planeRes(drmModeGetPlaneResources(primary.fd));
    if (!planeRes) {
        qWarning() << "Invalid plane res\n";
        return false;
    }

    for (uint32_t i = 0; i < planeRes->count_planes; ++i) {
        DrmPlanePtr plane(drmModeGetPlane(primary.fd, planeRes->planes[i]));
        if (!plane) {
            qDebug() << "No plane\n";
            continue;
        }

        // Only planes active on our CRTC with a live framebuffer
        if (plane->crtc_id != primary.crtc_id || plane->fb_id == 0) {
            continue;
        }

        DrmObjectPropsPtr props(drmModeObjectGetProperties(primary.fd, plane->plane_id, DRM_MODE_OBJECT_PLANE));
        if (!props) {
            continue;
        }

        bool isCursor = false;
        int32_t crtcX = 0, crtcY = 0;

        for (uint32_t j = 0; j < props->count_props; ++j) {
            DrmPropertyPtr prop(drmModeGetProperty(primary.fd, props->props[j]));
            if (!prop) {
                continue;
            }

            const QLatin1String propName(prop->name);
            if (propName == QLatin1String("type")) {
                isCursor = (props->prop_values[j] == DRM_PLANE_TYPE_CURSOR);
            } else if (propName == QLatin1String("CRTC_X")) {
                crtcX = static_cast<int32_t>(props->prop_values[j]);
            } else if (propName == QLatin1String("CRTC_Y")) {
                crtcY = static_cast<int32_t>(props->prop_values[j]);
            }
        }

        if (!isCursor) {
            continue;
        }
        // Inside your loop where you have the plane object
        qDebug() << "plane->crtc_x = " << plane->crtc_x << ", plane->crtc_y = " << plane->crtc_y;
        qDebug() << "x: " << crtcX << " y: " << crtcY;

        DrmFB2Ptr mfb(drmModeGetFB2(primary.fd, plane->fb_id));
        if (!mfb || mfb->handles[0] == 0) {
            continue;
        }

        cursor.width = mfb->width;
        cursor.height = mfb->height;
        cursor.stride = mfb->pitches[0];
        cursor.format = mfb->pixel_format;
        cursor.size = static_cast<std::size_t>(cursor.stride) * cursor.height;
        // plane->x/y is the src offset within the cursor buffer (almost always 0,0)
        cursor.crtc_x = crtcX + static_cast<int32_t>(plane->x);
        cursor.crtc_y = crtcY + static_cast<int32_t>(plane->y);

        if (!mapCursorFramebuffer(cursor, primary.fd, mfb->handles[0])) {
            cursor = {};
            continue;
        }

        // fd is borrowed from the primary — do not close it in releaseCursorFrameBuffer
        cursor.fd = primary.fd;
        return true;
    }

    return false;
}

void DisplayReader::releaseCursorFrameBuffer(CursorFrameBuffer &cursor)
{
    if (cursor.data && cursor.size > 0) {
        ::munmap(cursor.data, cursor.size);
        cursor.data = nullptr;
    }
    if (cursor.buffer_fd >= 0) {
        ::close(cursor.buffer_fd);
        cursor.buffer_fd = -1;
    }
    // cursor.fd is borrowed from the primary VkmsFrameBuffer — do not close it here

    cursor = {};
}

QImage DisplayReader::compositeWithCursor(const QImage &primary, const CursorFrameBuffer &cursor)
{
    if (!cursor.data || cursor.width == 0 || cursor.height == 0) {
        return primary;
    }

    const QImage cursorImg = imageFromFrameBuffer(static_cast<const uint8_t *>(cursor.data), cursor.width, cursor.height, cursor.stride, cursor.format);

    if (cursorImg.isNull()) {
        return primary;
    }

    QImage result = primary.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(cursor.crtc_x, cursor.crtc_y, cursorImg);
    painter.end();

    return result;
}

QImage DisplayReader::imageFromFrameBuffer(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    if (!data || !width || !height || !stride) {
        return {};
    }

    const QImage::Format fmt = qtImageFormatFromDrm(format);
    if (fmt == QImage::Format_Invalid) {
        qWarning() << "Image format for frame is invalid.";
        return {};
    }

    return QImage(reinterpret_cast<const uchar *>(data), static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride), fmt).copy();
}

QPixmap DisplayReader::pixmapFromFrameBuffer(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    return QPixmap::fromImage(imageFromFrameBuffer(data, width, height, stride, format));
}
