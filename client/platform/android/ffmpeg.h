#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>

#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <queue>
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
    using FrameCallback = std::function<void(const QImage& image)>;

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
    int m_stride = 0;
    int m_sliceHeight = 0;
    int m_pixelFormat = 0;

    // CSD (VPS/SPS/PPS)
    std::vector<uint8_t> m_vps, m_sps, m_pps;
    bool m_csdReady = false;

    // Frame queue fallback (if not using surface)
    std::queue<QImage> m_frameQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;

    // AImageReader & zero‑copy pipeline
    AImageReader* m_imageReader = nullptr;
    ANativeWindow* m_window = nullptr;
    std::atomic<bool> m_frameAvailable{false};
    GLuint m_oesTextureId = 0; // OES texture handle
    EGLImageKHR m_eglImage = EGL_NO_IMAGE_KHR;
    std::mutex m_frameMutex;

    bool m_resolutionDetected = false;

    // Helper functions
    int decode_frame(const uint8_t* data, size_t size);

    // Called from AImageReader callback (decoder thread)
    static void onImageAvailable(void* context, AImageReader* reader);

    // Called from render thread to update the texture
    void updateTextureFromHardwareBuffer(AHardwareBuffer* buffer);

    // Fallback QImage callback (if AImageReader not available)
    FrameCallback m_frameCallback = nullptr;
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
