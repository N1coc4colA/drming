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

    /// Requests an AVFrame with the given dimensions and pixel format from the pool.
    AVFrame *request(int w, int h, AVPixelFormat fmt);
    /// Returns an AVFrame to the pool for reuse.
    void dispose(AVFrame *frame);
    /// Frees all cached AVFrame instances in the pool.
    void clear();

private:
    /// Queue of available reusable AVFrame instances.
    QQueue<AVFrame *> m_queue{};
    /// Total count of currently allocated AVFrame instances.
    qsizetype m_allocated = 0;
};

/// FFmpeg-based Linux decoder that converts stream packets into QImages.
class FfmpegDecoder : public QObject
{
    Q_OBJECT

public:
    using FrameCallback = std::function<void(const QImage &image, quint64 receiveNs)>;
    using LatencyCallback = std::function<void(quint64 latencyNs)>;

    explicit FfmpegDecoder(QObject *parent = nullptr);
    ~FfmpegDecoder();

    /// Returns the decoded frame width in pixels.
    int width() const
    {
        return m_width;
    }
    /// Returns the decoded frame height in pixels.
    int height() const
    {
        return m_height;
    }
    /// Reports whether the decoder is ready for input.
    bool isInitialized() const
    {
        return m_initialized;
    }

    /// Initializes the decoder for the requested frame size.
    int init();
    /// Releases decoder resources.
    void release();
    /// Discards queued decoder input and output.
    int flush();
    /// Submits encoded data to the decoder.
    int decode(const uint8_t *data, size_t size);

    /// Sets the callback invoked when a decoded frame is ready.
    void setFrameCallback(FrameCallback callback)
    {
        m_frameCallback = std::move(callback);
    }
    /// Records when the latest encoded frame arrived.
    void setLastInputNs(const quint64 ns)
    {
        m_lastInputNs = ns;
    }
    /// Sets the callback used to report decode latency.
    void setLatencyCallback(LatencyCallback callback)
    {
        m_latencyCallback = std::move(callback);
    }
    /// Associates telemetry used to record decoder timing.
    void setTelemetry(Telemetry *telemetry)
    {
        m_telemetry = telemetry;
    }

private:
    FrameCallback m_frameCallback = nullptr;
    LatencyCallback m_latencyCallback = nullptr;
    Telemetry *m_telemetry = nullptr;
    quint64 m_lastInputNs = 0;

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

    /// Decodes one frame and reports whether an IDR is needed.
    int decode_frame(AVPacket *pkt);
    /// Converts an AVFrame into a QImage for presentation.
    QImage frameToQImage(AVFrame *frame);
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
