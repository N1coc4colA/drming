#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QImage>
#include <QObject>
#include <QQueue>

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

namespace Platform {

class DecoderFramePool
{
public:
    ~DecoderFramePool();

    AVFrame *request(int w, int h, AVPixelFormat fmt);
    void dispose(AVFrame *frame);
    void clear();

private:
    QQueue<AVFrame *> m_queue{};
    qsizetype m_allocated = 0;
};

class FfmpegDecoder : public QObject
{
    Q_OBJECT

public:
    using FrameCallback = std::function<void(const QImage &image)>;

    explicit FfmpegDecoder(QObject *parent = nullptr);
    ~FfmpegDecoder();

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool isInitialized() const { return m_initialized; }

    int init();
    void release();
    int flush();
    int decode(const uint8_t *data, size_t size);

    void setFrameCallback(FrameCallback callback) { m_frameCallback = callback; }

private:
    FrameCallback m_frameCallback = nullptr;

    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_needResync = false;

    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_dec = nullptr;
    SwsContext *m_sws = nullptr;
    AVFrame *m_frame = nullptr;

    AVPixelFormat m_src_fmt = AV_PIX_FMT_YUV420P;
    AVPixelFormat m_dst_fmt = AV_PIX_FMT_RGB24;

    DecoderFramePool m_pool{};

    int decode_frame(AVPacket *pkt);
    QImage frameToQImage(AVFrame *frame);
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
