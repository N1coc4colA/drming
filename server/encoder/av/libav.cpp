#include "libav.h"

#include <QDebug>

#include <drm_fourcc.h>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
}

namespace LibAV {

QString avError(const int err)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, errbuf, sizeof(errbuf));

    return errbuf;
}

AVPixelFormat formatFromFcc(const uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
        return AV_PIX_FMT_ARGB;
    case DRM_FORMAT_ABGR8888:
        return AV_PIX_FMT_ABGR;
    case DRM_FORMAT_RGB888:
        return AV_PIX_FMT_RGB24;
    case DRM_FORMAT_BGR888:
        return AV_PIX_FMT_BGR24;
    case DRM_FORMAT_RGB565:
        return AV_PIX_FMT_RGB565;
    default:
        return AV_PIX_FMT_NONE;
    }
}

AVPixelFormat formatFromQt(const QImage::Format format)
{
    switch (format) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
    case QImage::Format_Indexed8:
    case QImage::Format_Grayscale8:
    case QImage::Format_Alpha8:
        return AV_PIX_FMT_GRAY8;

    case QImage::Format_RGB16:
        return AV_PIX_FMT_RGB565LE;
    case QImage::Format_RGB555:
        return AV_PIX_FMT_RGB555LE;
    case QImage::Format_RGB444:
        return AV_PIX_FMT_RGB444LE;
    case QImage::Format_Grayscale16:
        return AV_PIX_FMT_GRAY16LE;

    case QImage::Format_RGB888:
        return AV_PIX_FMT_RGB24;
    case QImage::Format_BGR888:
        return AV_PIX_FMT_BGR24;

        // 32-bit formats - corrected for little-endian memory layout
    case QImage::Format_RGB32:
        return AV_PIX_FMT_BGR0; // [B, G, R, 0]

    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return AV_PIX_FMT_BGRA; // [B, G, R, A]

    case QImage::Format_RGBX8888:
        return AV_PIX_FMT_RGB0; // [R, G, B, 0]

    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return AV_PIX_FMT_RGBA; // [R, G, B, A]

    case QImage::Format_ARGB8565_Premultiplied:
    case QImage::Format_ARGB6666_Premultiplied:
    case QImage::Format_ARGB8555_Premultiplied:
        return AV_PIX_FMT_BGRA; // Convert to standard BGRA
    case QImage::Format_RGB666:
        return AV_PIX_FMT_RGB24;
    case QImage::Format_CMYK8888:
        qWarning() << "Format CMYK8888 unsupported by FFmpeg";
        return AV_PIX_FMT_NONE;

    case QImage::Format_BGR30:
        return AV_PIX_FMT_X2BGR10LE;
    case QImage::Format_RGB30:
        return AV_PIX_FMT_X2RGB10LE;
    case QImage::Format_A2BGR30_Premultiplied:
        return AV_PIX_FMT_X2BGR10LE;
    case QImage::Format_A2RGB30_Premultiplied:
        return AV_PIX_FMT_X2RGB10LE;

    case QImage::Format_RGBX64:
        return AV_PIX_FMT_RGBA64LE;
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        return AV_PIX_FMT_RGBA64LE;

    case QImage::Format_RGBX16FPx4:
        return AV_PIX_FMT_RGBAF16LE;
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return AV_PIX_FMT_RGBAF16LE;

    case QImage::Format_RGBX32FPx4:
        return AV_PIX_FMT_RGBAF32LE;
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return AV_PIX_FMT_RGBAF32LE;

    case QImage::Format_Invalid:
    default:
        qWarning() << "QImage::Format unsupported by FFmpeg:" << format;
        return AV_PIX_FMT_NONE;
    }
}

} // namespace LibAV
