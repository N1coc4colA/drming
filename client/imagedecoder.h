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

class ImageDecoder : public QObject
{
    Q_OBJECT
public:
    using FrameCallback = std::function<void(const QImage &)>;

    explicit ImageDecoder(QObject *parent = nullptr);
    ~ImageDecoder() override;

    void setFrameCallback(FrameCallback callback);

    // Thread-safe: queues a new compressed frame, keeping at most 2 frames and dropping oldest
    void queueFrame(QByteArray data, QByteArray format);

    void reset();

private:
    void workerLoop();

    struct PendingFrame
    {
        QByteArray data;
        QByteArray format;
    };

    static constexpr size_t MaxPendingFrames = 2;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<PendingFrame> m_pendingFrames;
    bool m_running = true;

    std::mutex m_cbMutex;
    FrameCallback m_callback;
};

#endif // IMAGEDECODER_H
