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
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

public Q_SLOTS:
    void setImage(const QImage &image, qint64 receiveNs);
    void setLatencyCallback(std::function<void(qint64)> callback)
    {
        m_latencyCallback = std::move(callback);
    }

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    static constexpr size_t MaxPresentationQueue = 2;

    struct QueueEntry
    {
        QImage image{};
        qint64 receiveNs = 0;
    };

    std::deque<QueueEntry> m_frameQueue{};
    QSize m_imageSize{};
    QMutex m_mtx{};
    std::function<void(qint64)> m_latencyCallback{};
};

} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
