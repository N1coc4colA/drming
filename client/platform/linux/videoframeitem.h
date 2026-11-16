#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>

#include <deque>
#include <functional>

class QSGSimpleTextureNode;

namespace Platform {

/// QML item that presents the most recent decoded frame on the scene graph.
class VideoFrameItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit VideoFrameItem(QQuickItem *parent = nullptr);

protected:
    /// Updates the scene-graph texture with the next frame.
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

public Q_SLOTS:
    /// Queues a decoded image for presentation.
    void setImage(const QImage &image, const quint64 receiveNs);
    /// Sets the callback used to report decode latency.
    void setLatencyCallback(std::function<void(qint64)> callback)
    {
        m_latencyCallback = std::move(callback);
    }

protected:
    /// Requests a new frame presentation after a size change.
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    /// Maximum number of decoded frames buffered for presentation.
    static constexpr size_t MaxPresentationQueue = 2;

    /// Pairs a decoded image with the time it was received.
    struct QueueEntry
    {
        /// Decoded video frame.
        QImage image{};
        /// Timestamp in nanoseconds when the frame was received.
        quint64 receiveNs = 0;
    };

    /// Presentation queue of decoded frames.
    std::deque<QueueEntry> m_frameQueue{};
    /// Dimensions of the decoded video frames.
    QSize m_imageSize{};
    /// Mutex synchronizing access to the presentation queue.
    QMutex m_mtx{};
    /// Callback invoked when new frame latency data is measured.
    std::function<void(quint64)> m_latencyCallback{};
};

} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
