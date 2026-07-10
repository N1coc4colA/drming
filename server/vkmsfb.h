#ifndef VKMSFB_H
#define VKMSFB_H

#include <cstddef>
#include <cstdint>

struct VkmsFrameBuffer
{
    uint32_t fb_id = 0;
    uint32_t crtc_id = 0;
    int fd = -1;
    int buffer_fd = -1;
    void *data = nullptr;
    std::size_t size = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t bpp = 0;
    uint32_t format = 0;
};

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
