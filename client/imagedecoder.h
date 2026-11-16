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
    /// Callback type invoked with each decoded image and its receive timestamp.
    using FrameCallback = std::function<void(const QImage &, quint64 receiveNs)>;

    explicit ImageDecoder(QObject *parent = nullptr);
    /// Stops the worker thread, discards queued frames, and frees resources.
    ~ImageDecoder() override;

    /// Sets the callback invoked by the decoder thread for each decoded frame.
    void setFrameCallback(FrameCallback callback);
    /// Associates telemetry used to record decoder timing.
    void setTelemetry(Telemetry *t)
    {
        m_telemetry = t;
    }

    /// Queues a compressed frame while retaining only the newest pending frames.
    void queueFrame(QByteArray data, QByteArray format, quint64 receiveNs);

    /// Discards queued work and restores the decoder to its idle state.
    void reset();

private:
    /// Drains the queue on the worker thread until shutdown is requested.
    void workerLoop();

    /// Compressed frame waiting for the decoder worker.
    struct PendingFrame
    {
        /// Encoded image bytes.
        QByteArray data{};
        /// Optional image format name.
        QByteArray format{};
        /// Monotonic receive time associated with the frame.
        quint64 receiveNs = 0;
    };

    /// Maximum number of frames retained between producer and worker.
    static constexpr size_t MaxPendingFrames = 3;

    /// Decoder worker thread.
    std::thread m_thread;
    /// Synchronizes producers with the worker and shutdown path.
    std::mutex m_mutex{};
    /// Signals the worker when work arrives or shutdown begins.
    std::condition_variable m_cv{};
    /// Bounded queue of compressed frames.
    std::deque<PendingFrame> m_pendingFrames{};
    /// Whether the worker should continue processing.
    bool m_running = true;

    /// Synchronizes callback replacement with frame delivery.
    std::mutex m_cbMutex{};
    /// Consumer callback for decoded frames.
    FrameCallback m_callback{};

    /// Receives queue and drop statistics when configured.
    Telemetry *m_telemetry = nullptr;
};

#endif // IMAGEDECODER_H
