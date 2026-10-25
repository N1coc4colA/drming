#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

#include <QByteArray>
#include <QImage>
#include <QObject>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

class Telemetry;

/**
 * Decodes compressed stream frames away from the UI thread.
 *
 * The bounded queue deliberately favors recent frames so rendering remains
 * responsive when decoding cannot keep up with the incoming stream. This
 * class only processes *IMAGE* stream data, not video stream data.
 */
class ImageDecoder : public QObject
{
    Q_OBJECT

public:
    using FrameCallback = std::function<void(const QImage &, qint64 receiveNs)>;

    explicit ImageDecoder(QObject *parent = nullptr);
    ~ImageDecoder() override;

    /// Sets the callback invoked by the decoder thread for each decoded frame.
    void setFrameCallback(FrameCallback callback);
    void setTelemetry(Telemetry *t)
    {
        m_telemetry = t;
    }

    // Thread-safe: queues a new compressed frame, keeping at most 2 frames and dropping oldest
    void queueFrame(QByteArray data, QByteArray format, qint64 receiveNs);

    /// Discards queued work and restores the decoder to its idle state.
    void reset();

private:
    void workerLoop();

    struct PendingFrame
    {
        QByteArray data{};
        QByteArray format{};
        qint64 receiveNs = 0;
    };

    static constexpr size_t MaxPendingFrames = 3;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<PendingFrame> m_pendingFrames;
    bool m_running = true;

    std::mutex m_cbMutex;
    FrameCallback m_callback;

    Telemetry *m_telemetry = nullptr;
};

#endif // IMAGEDECODER_H
