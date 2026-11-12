#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QMutex>
#include <QObject>
#include <QSize>

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

// Android NDK headers
#include <android/hardware_buffer.h>
#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

// EGL / GLES for zero‑copy texture
#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES 1
#endif
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

class Telemetry;

namespace Platform {

/**
 * H.265 MediaCodec decoder backed by an AImageReader/EGL zero-copy pipeline.
 *
 * Decoding and rendering use separate threads: the decoder publishes an
 * available hardware buffer and the scene graph consumes it on demand.
 */
class VideoDecoder : public QObject
{
    Q_OBJECT

public:
    using LatencyCallback = std::function<void(quint64 latencyNs)>;

    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder();

    /// Returns the decoded frame width in pixels.
    int width() const
    {
        return m_width;
    }
    /// Returns the decoded frame height in pixels.
    int height() const
    {
        return m_height;
    }
    /// Reports whether the decoder is ready for input.
    bool isInitialized() const
    {
        return m_initialized;
    }

    /// Initializes the decoder for the requested frame size.
    bool init(int width = 0, int height = 0);
    /// Releases resources used by the decoder.
    void release();
    /// Discards queued decoder input and output.
    bool flush();
    /// Submits encoded data to the decoder.
    bool decode(const uint8_t *data, size_t size);

    /// Publishes the next decoded frame to the render thread.
    /// Returns true if new frame was consumed
    bool consumeFrame();
    /// Returns the current external OES texture handle.
    std::optional<GLuint> oesTextureId() const
    {
        if (m_oesTextureIds.has_value()) {
            return m_oesTextureIds.value()[m_textureIndex];
        }

        return {};
    }

    /// Returns the dimensions of the decoded texture.
    QSize textureSize() const
    {
        return QSize(m_width, m_height);
    }

    /// Sets the callback used to report decode latency.
    void setLatencyCallback(LatencyCallback cb)
    {
        m_latencyCallback = std::move(cb);
    }
    /// Records when the latest encoded frame arrived.
    void setLastInputNs(const quint64 ns)
    {
        m_lastInputNs.store(ns, std::memory_order_relaxed);
    }

    /// Associates telemetry used to record decoder timing.
    void setTelemetry(Telemetry *t)
    {
        m_telemetry = t;
    }

    /// Invalidates graphics resources so they can be recreated.
    void invalidateGLResources();

Q_SIGNALS:
    void needsKeyframe();

private:
    /// Reports decoder status and whether an IDR frame is required.
    struct RetWithIdr
    {
        /// Whether decode completed successfully.
        bool status = false;
        /// Whether an IDR keyframe is required to recover decoder state.
        bool needsIdr = false;
    };

    mutable std::recursive_mutex m_codecMutex{};
    // Decoder state
    AMediaCodec *m_codec = nullptr;
    AMediaFormat *m_format = nullptr;
    int m_width = 0;
    int m_height = 0;
    /// OES texture handles
    std::optional<std::array<GLuint, 2>> m_oesTextureIds{};
    size_t m_textureIndex = 0;
    int m_consecutiveTALs = 0;

    std::atomic<quint8> m_framesInReader{ 0 };

    std::atomic<quint64> m_lastInputNs{ 0 };
    LatencyCallback m_latencyCallback{};

    bool m_initialized = false;
    bool m_needResync = false;
    bool m_resolutionDetected = false;
    bool m_csdReady = false;

    // CSD (VPS/SPS/PPS)
    std::vector<uint8_t> m_vps, m_sps, m_pps;

    // AImageReader & zero‑copy pipeline
    AImage *m_pendingImage = nullptr;
    AImage *m_previousImage = nullptr;
    AImageReader *m_imageReader = nullptr;
    ANativeWindow *m_window = nullptr;

    Telemetry *m_telemetry = nullptr;
    quint64 m_lastKeyframeRequestNs = 0;

    // AImageReader's maxImages. Must be >= 2 so the render thread always has a
    // free buffer to acquire. The throttle in decode_frame uses this - 1.
    static constexpr size_t kImageReaderMaxImages = 6; // must match AImageReader_new's maxImages

    // EGL image cache size. Each entry holds an AHardwareBuffer reference,
    // preventing the AImageReader from recycling that buffer. Keep this
    // strictly smaller than kImageReaderMaxImages so the reader is never
    // starved.
    static constexpr size_t kMaxCachedEglImages = 2;

    /// Submits encoded data while holding the codec lock.
    RetWithIdr decode_locked(const uint8_t *data, size_t size);

    /// Associates an EGL image with its retained hardware buffer.
    struct CachedImage
    {
        /// EGLImage wrapping the hardware buffer.
        EGLImageKHR image = EGL_NO_IMAGE_KHR;
        /// Retained reference to the AHardwareBuffer.
        AHardwareBuffer *bufferRef = nullptr; // our own acquired reference
    };
    std::unordered_map<AHardwareBuffer *, CachedImage> m_eglImageCache{};
    /// Releases cached EGL images and their buffer references.
    void destroyEglImageCache();

    /// Decodes one frame and reports whether an IDR is needed.
    bool decode_frame(const uint8_t *data, size_t size, bool isKeyFrame);
    /// Decodes one frame while holding the codec lock.
    RetWithIdr decode_frame_locked(const uint8_t *data, size_t size, bool isKeyFrame);

    /// Called from AImageReader callback (decoder thread)
    static void onImageAvailable(void *context, AImageReader *reader);

    /// Updates the external texture from a hardware buffer.
    void updateTextureFromHardwareBuffer(AHardwareBuffer *buffer);
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
