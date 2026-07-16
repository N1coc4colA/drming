#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>

class QSGSimpleTextureNode;

namespace Platform {

class VideoFrameItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit VideoFrameItem(QQuickItem* parent = nullptr);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

public Q_SLOTS:
    void setImage(const QImage& image)
    {
        {
            QMutexLocker locker(&m_mtx);
            m_currentImage = image.convertToFormat(QImage::Format_RGBA8888);
            m_imageSize = m_currentImage.size();
            m_imageDirty = true;
        }

        update();
    }

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    QImage m_currentImage{};
    QSize m_imageSize{};
    QMutex m_mtx{};
    bool m_imageDirty = false;
};

} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
