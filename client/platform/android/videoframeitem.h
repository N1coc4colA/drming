#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>
#include <QSize>

#include <deque>

class QSGSimpleTextureNode;

namespace Platform {

class VideoDecoder;

class VideoFrameItem : public QQuickItem
{
    Q_OBJECT
public:
    enum TextureMode {
        Invalid,
        OES,
        Simple,
    };

    explicit VideoFrameItem(QQuickItem* parent = nullptr);

    // Set the decoder to use for zero‑copy texture rendering
    void setDecoder(VideoDecoder* decoder);

    Q_INVOKABLE void setImage(const QImage& image);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    static constexpr size_t MaxPresentationQueue = 2;

    VideoDecoder* m_decoder = nullptr;

    std::deque<QImage> m_frameQueue;
    QSize m_imageSize;
    QMutex m_mtx;
    TextureMode m_textureMode = TextureMode::Invalid;
    TextureMode m_previousTextureMode = TextureMode::Invalid;
};
} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
