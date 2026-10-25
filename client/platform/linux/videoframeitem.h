#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>

#include <deque>

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
    void setImage(const QImage &image);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    static constexpr size_t MaxPresentationQueue = 2;

    std::deque<QImage> m_frameQueue{};
    QSize m_imageSize{};
    QMutex m_mtx{};
};

} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
