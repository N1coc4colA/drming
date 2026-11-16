#ifndef VIDEOFRAMEITEMPLATFORM_H
#define VIDEOFRAMEITEMPLATFORM_H

#include <QImage>
#include <QQuickItem>
#include <QSize>

#include <deque>

class Telemetry;
class QSGSimpleTextureNode;

namespace Platform {

class VideoDecoder;
class AndroidUi;

/**
 * Displays decoded frames as a Qt Quick item.
 * Android video item supporting either zero-copy OES textures or QImage frames.
 *
 * The selected texture mode can change when the decoder is recreated, so the
 * previous scene-graph node is not assumed to remain valid.
 */
class VideoFrameItem : public QQuickItem
{
    Q_OBJECT

public:
    using LatencyCallback = std::function<void(quint64 latencyNs)>;
    /// Texture rendering backend mode.
    enum TextureMode {
        /// Uninitialized or invalid texture mode.
        Invalid,
        /// Hardware-accelerated zero-copy GL_TEXTURE_EXTERNAL_OES rendering.
        OES,
        /// Software QImage texture rendering.
        Simple,
    };

    explicit VideoFrameItem(QQuickItem *parent = nullptr);

    /// Sets the decoder used by the zero-copy texture rendering path.
    void setDecoder(VideoDecoder *decoder);
    /// Sets the callback used to report decode latency.
    void setLatencyCallback(LatencyCallback cb)
    {
        m_latencyCallback = std::move(cb);
    }
    /// Associates telemetry used to record decoder timing.
    void setTelemetry(Telemetry *t)
    {
        m_telemetry = t;
    }

    /// Queues a decoded image for presentation.
    Q_INVOKABLE void setImage(const QImage &image, quint64 receiveNs);

protected:
    /// Updates the scene-graph texture with the next frame.
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    /// Requests a new frame presentation after a size change.
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    /// Maximum number of presentation frames buffered in queue.
    static constexpr size_t MaxPresentationQueue = 3;
    /// Pairs a decoded image with the time it was received.
    struct QueueEntry
    {
        /// Decoded video frame image.
        QImage image{};
        /// Receive latency in ns.
        quint64 receiveNs = 0;
    };

    AndroidUi *m_aui = nullptr;
    /// Currently used decoder.
    VideoDecoder *m_decoder = nullptr;

    /// Active texture rendering mode.
    std::atomic<TextureMode> m_textureMode{ Simple };
    /// Texture rendering mode from previous paint cycle.
    std::atomic<TextureMode> m_previousTextureMode{ Simple };

    /// Mutex protecting the frame presentation queue.
    QMutex m_mtx{};
    /// Presentation queue of decoded frames.
    std::deque<QueueEntry> m_frameQueue{};
    /// Current video frame dimensions.
    QSize m_imageSize{};

    /// Callback invoked when latency measurements are updated.
    LatencyCallback m_latencyCallback{};
    /// Telemetry recorder for frame and presentation metrics.
    Telemetry *m_telemetry = nullptr;
};
} // namespace Platform

#endif // VIDEOFRAMEITEMPLATFORM_H
