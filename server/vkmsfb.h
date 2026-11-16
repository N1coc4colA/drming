#ifndef VKMSFB_H
#define VKMSFB_H

#include <memory>

#include <cstddef>

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

/// Splits a 32-bit FourCC integer code into four byte components.
inline void split_fourcc(const uint32_t code, uint8_t &a, uint8_t &b, uint8_t &c, uint8_t &d)
{
    a = static_cast<uint8_t>((code >> 0) & 0xff);
    b = static_cast<uint8_t>((code >> 8) & 0xff);
    c = static_cast<uint8_t>((code >> 16) & 0xff);
    d = static_cast<uint8_t>((code >> 24) & 0xff);
}

/// Splits a 32-bit FourCC integer code into four ASCII characters.
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
    /// DRM device file descriptor.
    int fd = -1;
    /// Prime dma-buf file descriptor.
    int buffer_fd = -1;
    /// Memory-mapped framebuffer pixel pointer.
    void *data = nullptr;
    std::size_t size = 0;
    /// DRM framebuffer ID.
    uint32_t fb_id = 0;
    /// CRTC identifier.
    uint32_t crtc_id = 0;
    /// Frame width in pixels.
    uint32_t width = 0;
    /// Frame height in pixels.
    uint32_t height = 0;
    /// Line stride in bytes.
    uint32_t stride = 0;
    /// Bits per pixel.
    uint32_t bpp = 0;
    /// DRM FourCC pixel format.
    uint32_t format = 0;
};

/// Metadata and mapped memory for the cursor plane framebuffer.
struct CursorFrameBuffer
{
    /// DRM device file descriptor.
    int fd = -1;
    /// Prime dma-buf file descriptor.
    int buffer_fd = -1;
    /// Memory-mapped framebuffer pixel pointer.
    void *data = nullptr;
    std::size_t size = 0;
    /// Cursor width in pixels.
    uint32_t width = 0;
    /// Cursor height in pixels.
    uint32_t height = 0;
    /// Cursor line stride in bytes.
    uint32_t stride = 0;
    /// DRM FourCC pixel format.
    uint32_t format = 0;
    /// Cursor horizontal position on the CRTC.
    int32_t crtc_x = 0;
    /// Cursor vertical position on the CRTC.
    int32_t crtc_y = 0;
};

#endif // VKMSFB_H
