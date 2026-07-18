#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>
#include <QSize>

class QSGSimpleTextureNode;

namespace Platform {

class FfmpegDecoder;

class VideoFrameItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit VideoFrameItem(QQuickItem* parent = nullptr);

    // Set the decoder to use for zero‑copy texture rendering
    void setDecoder(FfmpegDecoder* decoder);

    // Fallback: set QImage (still supported)
    Q_INVOKABLE void setImage(const QImage& image);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    FfmpegDecoder* m_decoder = nullptr;

    // Fallback QImage
    QImage m_currentImage;
    QSize m_imageSize;
    QMutex m_mtx;
    bool m_imageDirty = false;
};

} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
