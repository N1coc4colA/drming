#ifndef VIDEOFRAMEITEM_H
#define VIDEOFRAMEITEM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>

class QSGSimpleTextureNode;

class VideoFrameItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit VideoFrameItem(QQuickItem* parent = nullptr);

    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

public Q_SLOTS:
    void setImage(const QImage& image)
    {
        {
            QMutexLocker locker(&m_mtx);
            m_currentImage = image.convertToFormat(QImage::Format_RGBA8888);
            m_imageDirty = true;
        }

        update();
    }

private:
    QImage m_currentImage{};
    QMutex m_mtx{};
    bool m_imageDirty = false;
};

#endif // VIDEOFRAMEITEM_H
