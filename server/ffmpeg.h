#ifndef FFMPEG_H
#define FFMPEG_H

#include <QImage>

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVCodec;
struct AVCodecContext;
struct AVPacket;
struct SwsContext;
struct AVFrame;

namespace Ffmpeg {

QString avError(int err);

/**
 * Converts captured DRM images into the negotiated compressed stream format.
 *
 * The encoder is initialized lazily from the first frame because the display
 * dimensions and pixel format are properties of the active connector.
 */
class Encoder
{
public:
    using PacketCallback = std::function<void(const uint8_t *data, size_t size, int64_t pts)>;

    explicit Encoder(PacketCallback callback, int fps);
    ~Encoder();

    static AVPixelFormat formatFromFcc(uint32_t format);
    static AVPixelFormat formatFromQt(QImage::Format format);

    /// Encodes one frame and invokes the callback for each produced packet.
    void push_image(const uint8_t *data, int width, int height, QImage::Format format, uint32_t stride);

private:
    int init(int width, int height, QImage::Format format, uint32_t stride);
    void release();
    int flush();
    int encode(AVFrame *frame);

    PacketCallback m_callback;

    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_enc = nullptr;
    AVPacket *m_pkt = nullptr;
    struct SwsContext *m_sws = nullptr;
    AVFrame *m_yuv = nullptr;

    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int64_t m_pts = 0;

    enum AVPixelFormat m_src_fmt = AV_PIX_FMT_NONE;
    enum AVPixelFormat m_dst_fmt = AV_PIX_FMT_NV12; // AV_PIX_FMT_YUV420P;
    QImage::Format m_format = QImage::Format_Invalid;
    uint32_t m_stride = 0;
};

} // namespace Ffmpeg

#endif // FFMPEG_H
