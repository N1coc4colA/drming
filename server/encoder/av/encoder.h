#ifndef DRMING_AV_ENCODER_H
#define DRMING_AV_ENCODER_H

extern "C" {
#include <libavutil/pixfmt.h>
}

#include "../../encoder.h"

struct AVCodec;
struct AVCodecContext;
struct AVPacket;
struct SwsContext;
struct AVFrame;

namespace LibAV
{

class Encoder : public ::Encoder
{
public:
    using SetupFunction = AVPixelFormat (*)(AVCodecContext *ctx);

    class Entry
    {
    public:
        explicit Entry(const char *name, SetupFunction setupFn)
            : m_name(name)
            , m_setupFn(setupFn)
        {
            assert(name);
            assert(setupFn);
        }

        inline const char *name() const
        {
            return m_name;
        }
        inline SetupFunction setupFunction() const
        {
            return m_setupFn;
        }

    private:
        const char *m_name;
        const SetupFunction m_setupFn;
    };

    explicit Encoder(PacketCallback callback, int fps);
    ~Encoder() override;

    void forceKeyframe() override
    {
        m_forceKeyframe.store(true, std::memory_order_relaxed);
    }

    void push_image(const uint8_t *data, int width, int height, QImage::Format format, qsizetype stride) override;

    static ::Encoder *create(PacketCallback callback, int fps)
    {
        return new Encoder(callback, fps);
    }

    static ::Encoder *createAVEncoder(const PacketCallback callback, const int fps)
    {
        return new Encoder(callback, fps);
    }

private:
    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_enc = nullptr;
    AVPacket *m_pkt = nullptr;
    SwsContext *m_sws = nullptr;
    std::array<AVFrame *, 2> m_yuv = { nullptr, nullptr };
    /// Index of the active double-buffered YUV frame.
    size_t m_yuvIndex = 0;

    /// Source libav pixel format.
    AVPixelFormat m_src_fmt = AV_PIX_FMT_NONE;
    /// Destination libav pixel format used for encoding.
    AVPixelFormat m_dst_fmt = AV_PIX_FMT_NONE;
    /// Active Qt image format of input frames.
    QImage::Format m_format = QImage::Format_Invalid;
    /// Input pixel line stride in bytes.
    qsizetype m_stride = 0;

    std::atomic<bool> m_forceKeyframe{ false };

    int init(int width, int height, QImage::Format format, qsizetype stride);
    /// Releases allocated FFmpeg frames, packets, and contexts.
    void release();
    int flush();
    /// Submits an AVFrame to the encoder and flushes available output packets.
    int encode(AVFrame &frame);
};

} // namespace LibAV

#endif //DRMING_AV_ENCODER_H
