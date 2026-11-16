#ifndef DRMING_LIBAV_H
#define DRMING_LIBAV_H

#include <cstdint>

#include <QImage>
#include <QString>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace LibAV
{
    /// Converts an FFmpeg error integer code to a descriptive QString.
    QString avError(int err);

    /// Maps a DRM FourCC code to the corresponding libav pixel format.
    AVPixelFormat formatFromFcc(uint32_t format);
    /// Maps a Qt image format to the corresponding libav pixel format.
    AVPixelFormat formatFromQt(QImage::Format format);

} // namespace LibAV


#endif //DRMING_LIBAV_H
