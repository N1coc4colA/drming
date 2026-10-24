#include "imagedecoder.h"

#include <QDebug>

#include "../net/telemetry.h"


ImageDecoder::ImageDecoder(QObject *parent)
    : QObject(parent)
    , m_thread(&ImageDecoder::workerLoop, this)
{
}

ImageDecoder::~ImageDecoder()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        m_pendingFrames.clear();
    }
    m_cv.notify_one();

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ImageDecoder::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_callback = std::move(callback);
}

void ImageDecoder::queueFrame(QByteArray data, QByteArray format, const qint64 receiveNs)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_pendingFrames.push_back({std::move(data), std::move(format), receiveNs});

        while (m_pendingFrames.size() > MaxPendingFrames) {
            m_pendingFrames.pop_front();
            if (m_telemetry) {
                m_telemetry->recordDropped();
            }
        }

        // Report depth after the eviction so it reflects what the worker
        // will actually see when it wakes up.
        if (m_telemetry) {
            m_telemetry->setDecodeQueueDepth(static_cast<quint8>(
                std::min<std::size_t>(m_pendingFrames.size(), 255)));
        }
    }

    m_cv.notify_one();
}

void ImageDecoder::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_pendingFrames.clear();
    if (m_telemetry) {
        m_telemetry->setDecodeQueueDepth(0);
    }
}

void ImageDecoder::workerLoop()
{
    while (true) {
        PendingFrame frame{};
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_running || !m_pendingFrames.empty(); });
            if (!m_running && m_pendingFrames.empty()) {
                break;
            }
            frame = std::move(m_pendingFrames.front());
            m_pendingFrames.pop_front();

            if (m_telemetry) {
                m_telemetry->setDecodeQueueDepth(static_cast<quint8>(
                    std::min<std::size_t>(m_pendingFrames.size(), 255)));
            }
        }

        if (!frame.data.isEmpty()) {
            QImage img = QImage::fromData(frame.data, frame.format.isEmpty() ? nullptr : frame.format.constData());
            if (!img.isNull()) {
                QImage converted = img.convertToFormat(QImage::Format_RGBA8888);
                FrameCallback cb;
                {
                    std::lock_guard<std::mutex> lock(m_cbMutex);
                    cb = m_callback;
                }

                if (cb) {
                    cb(converted, frame.receiveNs);
                }
            } else {
                qWarning() << "ImageDecoder: Failed to decode image of format" << frame.format << "size" << frame.data.size();
            }
        }
    }
}
