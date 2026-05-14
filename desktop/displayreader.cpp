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

using DrmResourcesPtr = std::unique_ptr<drmModeRes, DrmResourcesDeleter>;
using DrmConnectorPtr = std::unique_ptr<drmModeConnector, DrmConnectorDeleter>;
using DrmEncoderPtr = std::unique_ptr<drmModeEncoder, DrmEncoderDeleter>;
using DrmCrtcPtr = std::unique_ptr<drmModeCrtc, DrmCrtcDeleter>;
using DrmFB2Ptr = std::unique_ptr<drmModeFB2, DrmFB2Deleter>;

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

QImage DisplayReader::imageFromFrameBuffer(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    if (!data || !width || !height || !stride) {
        return {};
    }

    const QImage::Format fmt = qtImageFormatFromDrm(format);
    if (fmt == QImage::Format_Invalid) {
        std::cerr << "Image format for frame is invalid.\n";
        return {};
    }

    return QImage(reinterpret_cast<const uchar *>(data), static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride), fmt).copy();
}

QPixmap DisplayReader::pixmapFromFrameBuffer(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    return QPixmap::fromImage(imageFromFrameBuffer(data, width, height, stride, format));
}
