#include "decode.h"

#include <QDebug>

#include "videoframeitem.h"

namespace Platform {
namespace {
bool nextAnnexBNal(const uint8_t *data, const size_t size, size_t &cursor, size_t &nalStart, size_t &nalSize)
{
    while (cursor + 3 < size) {
        if (data[cursor] == 0x00 && data[cursor + 1] == 0x00) {
            const size_t startCodeSize = data[cursor + 2] == 0x01 ? 3 : (cursor + 3 < size && data[cursor + 2] == 0x00 && data[cursor + 3] == 0x01 ? 4 : 0);
            if (startCodeSize != 0) {
                nalStart = cursor + startCodeSize;
                if (nalStart >= size) {
                    return false;
                }

                size_t next = nalStart;
                bool foundNext = false;
                while (next + 3 < size) {
                    if (data[next] == 0x00 && data[next + 1] == 0x00 &&
                        (data[next + 2] == 0x01 || (next + 3 < size && data[next + 2] == 0x00 && data[next + 3] == 0x01))) {
                        foundNext = true;
                        break;
                    }
                    ++next;
                }

                if (!foundNext) {
                    next = size;
                }

                nalSize = next - nalStart;
                cursor = next;
                return true;
            }
        }

        ++cursor;
    }

    return false;
}

bool isKeyFrameH265(const uint8_t *data, const size_t size)
{
    size_t cursor = 0;
    size_t nalStart = 0;
    size_t nalSize = 0;

    while (nextAnnexBNal(data, size, cursor, nalStart, nalSize)) {
        const uint8_t nalType = (data[nalStart] >> 1) & 0x3F;
        if (nalType == 19 || nalType == 20 || nalType == 21) {
            return true;
        }
    }

    return false;
}

void extractCSDH265(const uint8_t *data, const size_t size, std::vector<uint8_t> &vps, std::vector<uint8_t> &sps, std::vector<uint8_t> &pps)
{
    size_t cursor = 0;
    size_t nalStart = 0;
    size_t nalSize = 0;

    while (nextAnnexBNal(data, size, cursor, nalStart, nalSize)) {
        const uint8_t nalType = (data[nalStart] >> 1) & 0x3F;

        if (nalType == 32) {
            vps.assign(data + nalStart, data + nalStart + nalSize);
        } else if (nalType == 33) {
            sps.assign(data + nalStart, data + nalStart + nalSize);
        } else if (nalType == 34) {
            pps.assign(data + nalStart, data + nalStart + nalSize);
        }
    }
}
} // namespace

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent)
{}

VideoDecoder::~VideoDecoder()
{
    release();
}

int VideoDecoder::init(const int width, const int height)
{
    if (m_initialized) {
        return 0;
    }

    m_width = width;
    m_height = height;
    m_resolutionDetected = width > 0 && height > 0;

    media_status_t status = AImageReader_new(width,
                                             height,
                                             AIMAGE_FORMAT_YUV_420_888, // most common
                                             3,
                                             &m_imageReader);
    switch (status) {
    case AMEDIA_OK: {
        // Most likely case. We can continue normally.
        break;
    }
    case AMEDIA_ERROR_INVALID_PARAMETER: {
        qWarning() << "AImageReader_new failed:" << status;
        return -1;
    }
    case AMEDIA_ERROR_UNKNOWN: {
        qWarning() << "AImageReader_new failed with unknown error.";
        return -1;
    }
    default: {
        qWarning() << "AImageReader_new failed with unexpected error code:" << status;
        return -1;
    }
    }

    // 2. Get ANativeWindow from the reader
    status = AImageReader_getWindow(m_imageReader, &m_window);
    if (status != AMEDIA_OK) {
        qWarning() << "AImageReader_getWindow failed:" << status;
        AImageReader_delete(m_imageReader);
        m_imageReader = nullptr;
        return -1;
    }

    // 3. Set up the image listener (callback from decoder thread)
    AImageReader_ImageListener listener;
    listener.context = this;
    listener.onImageAvailable = VideoDecoder::onImageAvailable;
    AImageReader_setImageListener(m_imageReader, &listener);

    // 4. Create MediaCodec decoder
    m_codec = AMediaCodec_createDecoderByType("video/hevc");
    if (!m_codec) {
        qCritical() << "Failed to create MediaCodec decoder";
        return -1;
    }

    m_format = AMediaFormat_new();
    AMediaFormat_setString(m_format, AMEDIAFORMAT_KEY_MIME, "video/hevc");
    AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_PRIORITY, 0);

    // Set CSD if available
    if (m_csdReady) {
        if (!m_vps.empty()) {
            AMediaFormat_setBuffer(m_format, "csd-0", m_vps.data(), m_vps.size());
        }
        if (!m_sps.empty()) {
            AMediaFormat_setBuffer(m_format, "csd-1", m_sps.data(), m_sps.size());
        }
        if (!m_pps.empty()) {
            AMediaFormat_setBuffer(m_format, "csd-2", m_pps.data(), m_pps.size());
        }
    }

    // 5. Configure MediaCodec with the ANativeWindow (surface)
    status = AMediaCodec_configure(m_codec, m_format, m_window, nullptr, 0);
    if (status != AMEDIA_OK) {
        qCritical() << "AMediaCodec_configure with surface failed:" << status;
        return -1;
    }

    status = AMediaCodec_start(m_codec);
    if (status != AMEDIA_OK) {
        qCritical() << "AMediaCodec_start failed:" << status;
        return -1;
    }

    m_initialized = true;
    qDebug() << "MediaCodec decoder initialized with AImageReader (zero-copy)";
    return 0;
}

void VideoDecoder::release()
{
    if (m_codec) {
        AMediaCodec_stop(m_codec);
        AMediaCodec_delete(m_codec);
        m_codec = nullptr;
    }
    if (m_format) {
        AMediaFormat_delete(m_format);
        m_format = nullptr;
    }

    if (m_imageReader) {
        AImageReader_delete(m_imageReader);
        m_imageReader = nullptr;
    }
    if (m_window) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }

    // Destroy all cached EGL images and release our buffer references
    destroyEglImageCache();

    if (m_oesTextureId != 0) {
        glDeleteTextures(1, &m_oesTextureId);
        m_oesTextureId = 0;
    }

    m_initialized = false;
    m_width = m_height = 0;
    m_resolutionDetected = false;
    m_width = 0;
    m_height = 0;
}

void VideoDecoder::destroyEglImageCache()
{
    if (m_eglImageCache.empty()) {
        return;
    }

    const auto display = eglGetCurrentDisplay();
    for (auto& [buffer, entry] : m_eglImageCache) {
        if (display != EGL_NO_DISPLAY && entry.image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(display, entry.image);
        }
        if (entry.bufferRef) {
            AHardwareBuffer_release(entry.bufferRef);
        }
    }
    m_eglImageCache.clear();
}

int VideoDecoder::flush()
{
    if (m_codec) {
        AMediaCodec_flush(m_codec);
        return 0;
    }

    return -1;
}

int VideoDecoder::decode(const uint8_t* data, const size_t size)
{
    const bool needNalScan = m_needResync || !m_csdReady;
    const bool isKeyFrame = needNalScan ? isKeyFrameH265(data, size) : false;

    // If we don't have resolution, use a default
    if (!m_resolutionDetected || (m_needResync && isKeyFrame)) [[unlikely]] {
        qWarning() << "No resolution detected, using default 1920x1080";
        const auto ret = init(1920, 1080);
        if (ret < 0) [[unlikely]] {
            qDebug() << "Failed to reinit (1).";
            return ret;
        }

        m_needResync = false;
    }

    if (!m_initialized) [[unlikely]] {
        const int ret = init(m_width, m_height);
        if (ret < 0) {
            qDebug() << "Failed to reinit (2).";
            return ret;
        }
    }

    if (isKeyFrame && m_needResync) {
        // [TODO] Handle flush status
        AMediaCodec_flush(m_codec);
        m_needResync = false;
        // Requires resend of key frame.
    }

    if (m_needResync && !isKeyFrame) {
        return 0;
    }

    if (isKeyFrame && !m_csdReady) {
        extractCSDH265(data, size, m_vps, m_sps, m_pps);
        if (!m_vps.empty() && !m_sps.empty() && !m_pps.empty()) {
            m_csdReady = true;
        }
    }

    const auto ret = decode_frame(data, size, isKeyFrame);
    if (ret < 0) [[unlikely]] {
        m_needResync = true;
        qDebug() << "Resync required.";
    }

    return ret;
}

int VideoDecoder::decode_frame(const uint8_t* data, const size_t size, const bool isKeyFrame)
{
    // Always queue input buffer (works for both paths)
    const auto inputIndex = AMediaCodec_dequeueInputBuffer(m_codec, 10000);
    if (inputIndex >= 0) [[likely]] {
        // Most likely case, we have a buffer, just continue.

    } else if (inputIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
        qDebug() << "Dequeue input TAL";
        // Just got to wait longer.
        return 0;
    } else {
        qWarning() << "Failed to dequeue input buffer, unexpected error occurred:" << inputIndex;
        return -1;
    }

    size_t bufferSize = 0;
    const auto inputBuffer = AMediaCodec_getInputBuffer(m_codec, inputIndex, &bufferSize);
    if (!inputBuffer) {
        qWarning() << "Failed to get input buffer: " << inputBuffer;
        return -1;
    }
    if (size > bufferSize) [[unlikely]] {
        // [TODO] handle buffer resizing.
        qWarning() << "Input data too large:" << size << ">" << bufferSize;
        return -1;
    }
    memcpy(inputBuffer, data, size);

    const uint32_t flags = isKeyFrame ? AMEDIACODEC_BUFFER_FLAG_KEY_FRAME : 0;
    const auto status = AMediaCodec_queueInputBuffer(
        m_codec,
        inputIndex,
        0,
        size,
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(),
        flags);
    if (status != AMEDIA_OK) [[unlikely]] {
        qWarning() << "MC Failed to queue input buffer:" << status;
        return -1;
    }

    AMediaCodecBufferInfo info{};
    for (;;) {
        const auto outIdx = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 0);
        if (outIdx >= 0) [[likely]] {
            // render = true: hands the buffer to the ANativeWindow/AImageReader.
            AMediaCodec_releaseOutputBuffer(m_codec, outIdx, true);
            continue; // more than one frame may be ready
        } else if (outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            break;
        } else if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            qDebug() << "Format changed!";
            continue;
        } else if (outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            continue;
        } else {
            qWarning() << "Unexpected output result:" << outIdx;
            break;
        }
    }

    return 0;
}

void VideoDecoder::onImageAvailable(void* context, AImageReader* reader)
{
    auto self = static_cast<VideoDecoder*>(context);
    // Just set a flag; the actual acquisition happens on the render thread
    self->m_frameAvailable.store(true, std::memory_order_release);
}

bool VideoDecoder::consumeFrame()
{
    if (!m_imageReader) {
        qDebug() << "Invalid consume.";
        return false;
    }

    const bool hadFrame = m_frameAvailable.exchange(false, std::memory_order_acq_rel);
    if (!hadFrame) {
        return false;
    }

    AImage* image = nullptr;
    auto status = AImageReader_acquireNextImage(m_imageReader, &image);
    switch (status) {
    case AMEDIA_OK:
        [[likely]]
        {
            // Most likely case, just continue.

            break;
        }
    case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE: {
        qDebug() << "No availble buffers";
        return false;
    }
    case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED: {
        qWarning() << "MC IR Too many IMG acquired.";
        break;
    }
    default: {
        qWarning() << "AImageReader_acquireNextImage failed:" << status;
        return false;
    }
    }

    // Get the hardware buffer
    AHardwareBuffer* hardwareBuffer = nullptr;
    status = AImage_getHardwareBuffer(image, &hardwareBuffer);
    if (status != AMEDIA_OK || !hardwareBuffer) [[unlikely]] {
        qWarning() << "Failed to get AHardwareBuffer from image";
        AImage_delete(image);
        return false;
    }

    // Update the OpenGL texture using the hardware buffer
    updateTextureFromHardwareBuffer(hardwareBuffer);

    AImage_delete(image);
    return true;
}

void VideoDecoder::updateTextureFromHardwareBuffer(AHardwareBuffer* buffer)
{
    const auto display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) [[unlikely]] {
        qWarning() << "No current EGL display";
        return;
    }

    // Get EGL client buffer from AHardwareBuffer
    const auto clientBuffer = eglGetNativeClientBufferANDROID(buffer);
    if (!clientBuffer) [[unlikely]] {
        qWarning() << "eglGetNativeClientBufferANDROID failed";
        return;
    }

    EGLImageKHR eglImage = EGL_NO_IMAGE_KHR;

    // Create EGLImage if needed, otherwise reuse the cached one for this buffer.
    auto it = m_eglImageCache.find(buffer);
    if (it != m_eglImageCache.end()) [[likely]] {
        eglImage = it->second.image;
    } else {
        constexpr EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
        eglImage = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);
        if (eglImage == EGL_NO_IMAGE_KHR) [[unlikely]] {
            qWarning() << "eglCreateImageKHR failed";
            return;
        }

        // The pool is fixed-size (AImageReader's maxImages). If we somehow
        // exceed it, evict one entry rather than growing unbounded.
        if (m_eglImageCache.size() >= kMaxCachedImages) [[unlikely]] {
            auto victim = m_eglImageCache.begin();
            eglDestroyImageKHR(display, victim->second.image);
            AHardwareBuffer_release(victim->second.bufferRef);
            m_eglImageCache.erase(victim);
        }

        // Pin our own reference so this buffer's address can't be reused by
        // the system for a different allocation while we hold it cached
        // (AImage_delete() in consumeFrame() only drops AImageReader's ref).
        AHardwareBuffer_acquire(buffer);
        m_eglImageCache.insert({buffer, CachedImage{eglImage, buffer}});
    }

    // Create/update the OES texture
    if (m_oesTextureId == 0) [[unlikely]] {
        glGenTextures(1, &m_oesTextureId);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTextureId);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTextureId);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);
}
} // namespace Platform
