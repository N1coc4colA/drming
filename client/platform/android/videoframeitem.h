#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QQuickItem>
#include <QSize>

#include <deque>


class Telemetry;
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
    using LatencyCallback = std::function<void(qint64 latencyNs)>;
    enum TextureMode {
        Invalid,
        OES,
        Simple,
    };

    explicit VideoFrameItem(QQuickItem* parent = nullptr);

    /// Sets the decoder used by the zero-copy texture rendering path.
    void setDecoder(VideoDecoder* decoder);
    void setLatencyCallback(LatencyCallback cb) { m_latencyCallback = std::move(cb); }
    void setTelemetry(Telemetry *t) { m_telemetry = t; }

    Q_INVOKABLE void setImage(const QImage& image, qint64 receiveNs);

protected:
    QSGNode* updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    static constexpr size_t MaxPresentationQueue = 3;
    struct QueueEntry {
        QImage image{};
        qint64 receiveNs = 0;
    };

    AndroidUi *m_aui = nullptr;
    VideoDecoder *m_decoder = nullptr;

    TextureMode m_textureMode = TextureMode::Simple;
    TextureMode m_previousTextureMode = TextureMode::Simple;

    QMutex m_mtx{};
    std::deque<QueueEntry> m_frameQueue{};
    QSize m_imageSize{};

    LatencyCallback m_latencyCallback{};
    Telemetry *m_telemetry = nullptr;
};
} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
