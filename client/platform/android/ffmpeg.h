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

namespace Platform {
class FfmpegDecoder : public QObject
{
    Q_OBJECT

public:
    explicit FfmpegDecoder(QObject* parent = nullptr);
    ~FfmpegDecoder();

    // Basic info
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool isInitialized() const { return m_initialized; }

    // Decoder control
    int init(int width = 0, int height = 0);
    void release();
    int flush();
    int decode(const uint8_t* data, size_t size);

    // Texture‑based rendering (called from render thread)
    bool consumeFrame(); // returns true if new frame was consumed
    GLuint oesTextureId() const { return m_oesTextureId; }

    QSize textureSize() const { return QSize(m_width, m_height); }

private:
    // Decoder state
    AMediaCodec* m_codec = nullptr;
    AMediaFormat* m_format = nullptr;
    bool m_initialized = false;
    bool m_needResync = false;
    int m_width = 0;
    int m_height = 0;

    // CSD (VPS/SPS/PPS)
    std::vector<uint8_t> m_vps, m_sps, m_pps;
    bool m_csdReady = false;

    // AImageReader & zero‑copy pipeline
    AImageReader* m_imageReader = nullptr;
    ANativeWindow* m_window = nullptr;
    std::atomic<bool> m_frameAvailable{false};
    GLuint m_oesTextureId = 0; // OES texture handle

    // EGLImage cache: AImageReader has a fixed pool of buffers (maxImages below),
    // so we cache one EGLImageKHR per AHardwareBuffer identity instead of
    // creating/destroying an EGLImage every frame. We hold our own reference
    // (AHardwareBuffer_acquire/_release) on each cached buffer so the pointer
    // can't be reused by the system for a different allocation while cached
    // (ABA hazard) — AImage_delete() only drops AImageReader's reference.
    static constexpr size_t kMaxCachedImages = 3; // must match AImageReader_new's maxImages
    struct CachedImage
    {
        EGLImageKHR image = EGL_NO_IMAGE_KHR;
        AHardwareBuffer* bufferRef = nullptr; // our own acquired reference
    };
    std::unordered_map<AHardwareBuffer*, CachedImage> m_eglImageCache;
    void destroyEglImageCache();

    bool m_resolutionDetected = false;

    // Helper functions
    int decode_frame(const uint8_t* data, size_t size, bool isKeyFrame);

    // Called from AImageReader callback (decoder thread)
    static void onImageAvailable(void* context, AImageReader* reader);

    // Called from render thread to update the texture
    void updateTextureFromHardwareBuffer(AHardwareBuffer* buffer);
};
} // namespace Platform

#endif // FFMPEGPLATFORM_H
