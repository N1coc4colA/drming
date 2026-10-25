#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QImage>
#include <QObject>
#include <QQueue>

#include <functional>

extern "C" {
#include <libavutil/pixfmt.h>
}

class Telemetry;

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

namespace Platform {

/// Reuses decoded AVFrames to reduce allocations during continuous playback.
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

/// FFmpeg-based Linux decoder that converts stream packets into QImages.
class FfmpegDecoder : public QObject
{
    Q_OBJECT

public:
    using FrameCallback = std::function<void(const QImage &image, qint64 receiveNs)>;
    using LatencyCallback = std::function<void(qint64 latencyNs)>;

    explicit FfmpegDecoder(QObject *parent = nullptr);
    ~FfmpegDecoder();

    int width() const
    {
        return m_width;
    }
    int height() const
    {
        return m_height;
    }
    bool isInitialized() const
    {
        return m_initialized;
    }

    int init();
    void release();
    int flush();
    int decode(const uint8_t *data, size_t size);

    void setFrameCallback(FrameCallback callback)
    {
        m_frameCallback = std::move(callback);
    }
    void setLastInputNs(const qint64 ns)
    {
        m_lastInputNs = ns;
    }
    void setLatencyCallback(LatencyCallback callback)
    {
        m_latencyCallback = std::move(callback);
    }
    void setTelemetry(Telemetry *telemetry)
    {
        m_telemetry = telemetry;
    }

private:
    FrameCallback m_frameCallback = nullptr;
    LatencyCallback m_latencyCallback = nullptr;
    Telemetry *m_telemetry = nullptr;
    qint64 m_lastInputNs = 0;

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
