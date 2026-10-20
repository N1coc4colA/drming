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
class AndroidUi;

/**
 * Android video item supporting either zero-copy OES textures or QImage frames.
 *
 * The selected texture mode can change when the decoder is recreated, so the
 * previous scene-graph node is not assumed to remain valid.
 */
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

    /// Sets the decoder used by the zero-copy texture rendering path.
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

    AndroidUi *m_aui = nullptr;
};
} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
