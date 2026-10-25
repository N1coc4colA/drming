#ifndef VKMSFB_H
#define VKMSFB_H

#include <memory>

#include <cstddef>
#include <cstdint>

#include <xf86drm.h>
#include <xf86drmMode.h>

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

inline void split_fourcc(const uint32_t code, uint8_t &a, uint8_t &b, uint8_t &c, uint8_t &d)
{
    a = static_cast<uint8_t>((code >> 0) & 0xff);
    b = static_cast<uint8_t>((code >> 8) & 0xff);
    c = static_cast<uint8_t>((code >> 16) & 0xff);
    d = static_cast<uint8_t>((code >> 24) & 0xff);
}

inline void split_fourcc(const uint32_t code, char &a, char &b, char &c, char &d)
{
    a = static_cast<char>((code >> 0) & 0xff);
    b = static_cast<char>((code >> 8) & 0xff);
    c = static_cast<char>((code >> 16) & 0xff);
    d = static_cast<char>((code >> 24) & 0xff);
}

} // namespace Drm

/// Metadata and mapped memory for a primary VKMS framebuffer.
struct VkmsFrameBuffer
{
    int fd = -1;
    int buffer_fd = -1;
    void *data = nullptr;
    std::size_t size = 0;
    uint32_t fb_id = 0;
    uint32_t crtc_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t bpp = 0;
    uint32_t format = 0;
};

/// Metadata and mapped memory for the cursor plane framebuffer.
struct CursorFrameBuffer
{
    int fd = -1;
    int buffer_fd = -1;
    void *data = nullptr;
    std::size_t size = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t format = 0;
    int32_t crtc_x = 0;
    int32_t crtc_y = 0;
};

#endif // VKMSFB_H
