#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QMutex>
#include <QObject>
#include <QSize>

#include <atomic>
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
 * MediaCodec decoder backed by an AImageReader/EGL zero-copy pipeline.
 *
 * Decoding and rendering use separate threads: the decoder publishes an
 * available hardware buffer and the scene graph consumes it on demand.
 */
class VideoDecoder : public QObject
{
    Q_OBJECT

public:
    using LatencyCallback = std::function<void(qint64 latencyNs)>;

    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder();

    // Basic info
    int width() const
    {
        return m_width;
    }
    int height() const
    {
        return m_height;
    }
    bool isInitialized() const
    {
        return m_initialized;
    }

    // Decoder control
    int init(int width = 0, int height = 0);
    void release();
    int flush();
    int decode(const uint8_t *data, size_t size);

    // Texture-based rendering (called from the render thread).
    bool consumeFrame(); // returns true if new frame was consumed
    GLuint oesTextureId() const
    {
        return m_oesTextureId;
    }

    QSize textureSize() const
    {
        return QSize(m_width, m_height);
    }

    void setLatencyCallback(LatencyCallback cb)
    {
        m_latencyCallback = std::move(cb);
    }
    void setLastInputNs(const qint64 ns)
    {
        m_lastInputNs.store(ns, std::memory_order_relaxed);
    }

    void setTelemetry(Telemetry *t)
    {
        m_telemetry = t;
    }

Q_SIGNALS:
    void needsKeyframe();

private:
    // Decoder state
    AMediaCodec *m_codec = nullptr;
    AMediaFormat *m_format = nullptr;
    int m_width = 0;
    int m_height = 0;
    GLuint m_oesTextureId = 0; // OES texture handle

    std::atomic<qint64> m_lastInputNs{ 0 };
    LatencyCallback m_latencyCallback{};

    bool m_initialized = false;
    bool m_needResync = false;
    bool m_resolutionDetected = false;
    bool m_csdReady = false;
    std::atomic<bool> m_frameAvailable{ false };

    // CSD (VPS/SPS/PPS)
    std::vector<uint8_t> m_vps, m_sps, m_pps;

    // AImageReader & zero‑copy pipeline
    AImage *m_pendingImage = nullptr;
    AImageReader *m_imageReader = nullptr;
    ANativeWindow *m_window = nullptr;

    Telemetry *m_telemetry = nullptr;

    // EGLImage cache: AImageReader has a fixed pool of buffers (maxImages below),
    // so we cache one EGLImageKHR per AHardwareBuffer identity instead of
    // creating/destroying an EGLImage every frame. We hold our own reference
    // (AHardwareBuffer_acquire/_release) on each cached buffer so the pointer
    // can't be reused by the system for a different allocation while cached
    // (ABA hazard) — AImage_delete() only drops AImageReader's reference.
    static constexpr size_t kMaxCachedImages = 6; // must match AImageReader_new's maxImages
    struct CachedImage
    {
        EGLImageKHR image = EGL_NO_IMAGE_KHR;
        AHardwareBuffer *bufferRef = nullptr; // our own acquired reference
    };
    std::unordered_map<AHardwareBuffer *, CachedImage> m_eglImageCache;
    void destroyEglImageCache();

    // Helper functions
    int decode_frame(const uint8_t *data, size_t size, bool isKeyFrame);

    // Called from AImageReader callback (decoder thread)
    static void onImageAvailable(void *context, AImageReader *reader);

    // Called from render thread to update the texture
    void updateTextureFromHardwareBuffer(AHardwareBuffer *buffer);
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
