#include "ffmpeg.h"

#include <QDebug>

#include "videoframeitem.h"

namespace Platform {

// Helper: Check if data is a keyframe (H.265)
bool isKeyFrameH265(const uint8_t* data, const size_t size)
{
    if (size < 2) {
        return false;
    }

    size_t pos = 0;
    while (pos < size - 4) {
        if (data[pos] == 0x00 && data[pos + 1] == 0x00) {
            if (data[pos + 2] == 0x01) {
                pos += 3;
                break;
            } else if (data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
                pos += 4;
                break;
            }
        }
        pos++;
    }

    if (pos >= size) {
        return false;
    }

    const uint8_t nalType = (data[pos] >> 1) & 0x3F;
    return (nalType == 19 || nalType == 20 || nalType == 21);
}

// Helper: Extract VPS, SPS, PPS from keyframe
void extractCSDH265(const uint8_t* data, const size_t size, std::vector<uint8_t>& vps, std::vector<uint8_t>& sps, std::vector<uint8_t>& pps)
{
    size_t pos = 0;
    while (pos < size - 4) {
        while (pos < size - 4) {
            if (data[pos] == 0x00 && data[pos + 1] == 0x00) {
                if (data[pos + 2] == 0x01 || (data[pos + 2] == 0x00 && data[pos + 3] == 0x01)) {
                    break;
                }
            }
            pos++;
        }

        if (pos >= size - 4)
            break;

        const size_t nalStart = pos;
        if (data[pos + 2] == 0x01) {
            pos += 3;
        } else {
            pos += 4;
        }

        size_t nalEnd = pos;
        while (nalEnd < size - 4) {
            if (data[nalEnd] == 0x00 && data[nalEnd + 1] == 0x00) {
                if (data[nalEnd + 2] == 0x01 || (data[nalEnd + 2] == 0x00 && data[nalEnd + 3] == 0x01)) {
                    break;
                }
            }
            nalEnd++;
        }

        size_t nalSize = nalEnd - nalStart;
        if (nalSize > 0) {
            const uint8_t nalType = (data[nalStart] >> 1) & 0x3F;

            if (nalType == 32) { // VPS
                vps.assign(data + nalStart, data + nalStart + nalSize);
            } else if (nalType == 33) { // SPS
                sps.assign(data + nalStart, data + nalStart + nalSize);
            } else if (nalType == 34) { // PPS
                pps.assign(data + nalStart, data + nalStart + nalSize);
            }
        }

        pos = nalEnd;
    }
}

FfmpegDecoder::FfmpegDecoder(QObject* parent)
    : QObject(parent)
{}

FfmpegDecoder::~FfmpegDecoder()
{
    release();
}

int FfmpegDecoder::init(const int width, const int height)
{
    if (m_initialized) {
        return 0;
    }

    m_width = width;
    m_height = height;
    m_resolutionDetected = true; // you need to add this member or just use width>0

#ifdef USE_NAT_SURFACES
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
        // Fallback to CPU path (original code)
        m_useImageReader = false;
        goto fallback;
    }
    case AMEDIA_ERROR_UNKNOWN: {
        qWarning() << "AImageReader_new failed with unknown error.";
        // Fallback to CPU path (original code)
        m_useImageReader = false;
        goto fallback;
    }
    default: {
        qWarning() << "AImageReader_new failed with unexpected error code:" << status;
        // Fallback to CPU path (original code)
        m_useImageReader = false;
        goto fallback;
    }
    }

    // 2. Get ANativeWindow from the reader
    status = AImageReader_getWindow(m_imageReader, &m_window);
    if (status != AMEDIA_OK) {
        qWarning() << "AImageReader_getWindow failed:" << status;
        AImageReader_delete(m_imageReader);
        m_imageReader = nullptr;
        m_useImageReader = false;
        goto fallback;
    }

    // 3. Set up the image listener (callback from decoder thread)
    AImageReader_ImageListener listener;
    listener.context = this;
    listener.onImageAvailable = FfmpegDecoder::onImageAvailable;
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
        if (!m_vps.empty())
            AMediaFormat_setBuffer(m_format, "csd-0", m_vps.data(), m_vps.size());
        if (!m_sps.empty())
            AMediaFormat_setBuffer(m_format, "csd-1", m_sps.data(), m_sps.size());
        if (!m_pps.empty())
            AMediaFormat_setBuffer(m_format, "csd-2", m_pps.data(), m_pps.size());
    }

    // 5. Configure MediaCodec with the ANativeWindow (surface)
    status = AMediaCodec_configure(m_codec, m_format, m_window, nullptr, 0);
    if (status != AMEDIA_OK) {
        qCritical() << "AMediaCodec_configure with surface failed:" << status;
        goto fallback;
    }

    status = AMediaCodec_start(m_codec);
    if (status != AMEDIA_OK) {
        qCritical() << "AMediaCodec_start failed:" << status;
        goto fallback;
    }

    m_useImageReader = true;

    m_initialized = true;
    qDebug() << "MediaCodec decoder initialized with AImageReader (zero-copy)";
    return 0;

fallback:
    // Clean up any half‑created resources and use the old CPU path
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
#endif

    {
        // Create decoder without surface
        m_codec = AMediaCodec_createDecoderByType("video/hevc");
        if (!m_codec) {
            qCritical() << "Failed to create MediaCodec decoder (fallback)";
            return -1;
        }

        m_format = AMediaFormat_new();
        AMediaFormat_setString(m_format, AMEDIAFORMAT_KEY_MIME, "video/hevc");
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_HEIGHT, height);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_PRIORITY, 0);
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

#ifdef USE_NAT_SURFACES
        status = AMediaCodec_configure(m_codec, m_format, nullptr, nullptr, 0);
#else
        media_status_t status = AMediaCodec_configure(m_codec, m_format, nullptr, nullptr, 0);
#endif

        if (status != AMEDIA_OK) {
            qCritical() << "Failed to configure MediaCodec (fallback):" << status;
            return -1;
        }
        status = AMediaCodec_start(m_codec);
        if (status != AMEDIA_OK) {
            qCritical() << "Failed to start MediaCodec (fallback):" << status;
            return -1;
        }

#ifdef USE_NAT_SURFACES
        m_useImageReader = false;
#endif

        m_initialized = true;
        qDebug() << "MediaCodec decoder initialized with CPU fallback";
        return 0;
    }
}

void FfmpegDecoder::release()
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

#ifdef USE_NAT_SURFACES
    if (m_imageReader) {
        AImageReader_delete(m_imageReader);
        m_imageReader = nullptr;
    }
    if (m_window) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }

    // Destroy EGL image and texture
    if (m_eglImage != EGL_NO_IMAGE_KHR) {
        const auto display = eglGetCurrentDisplay();
        if (display != EGL_NO_DISPLAY) {
            eglDestroyImageKHR(display, m_eglImage);
        }
        m_eglImage = EGL_NO_IMAGE_KHR;
    }
    if (m_oesTextureId != 0) {
        glDeleteTextures(1, &m_oesTextureId);
        m_oesTextureId = 0;
    }

    m_useImageReader = false;

#endif

    m_initialized = false;
    m_width = m_height = 0;
    m_resolutionDetected = false;
    m_width = 0;
    m_height = 0;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_frameQueue.empty()) {
        m_frameQueue.pop();
    }
}

int FfmpegDecoder::flush()
{
    if (m_codec) {
        AMediaCodec_flush(m_codec);
        return 0;
    }
    return -1;
}

int FfmpegDecoder::decode(const uint8_t* data, const size_t size)
{
    const auto isKeyFrame = isKeyFrameH265(data, size);

    // If we don't have resolution, use a default
    if (!m_resolutionDetected || (m_needResync && isKeyFrame)) {
        [[unlikely]];

        qWarning() << "No resolution detected, using default 1920x1080";
        const auto ret = init(1920, 1080);
        if (ret < 0) {
            [[unlikely]];

            qDebug() << "Failed to reinit (1).";
            return ret;
        }

        m_needResync = false;
    }

    if (!m_initialized) {
        [[unlikely]];

        const int ret = init(m_width, m_height);
        if (ret < 0) {
            [[unlikely]];

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

    // Check if this is a keyframe with CSD data
    if (isKeyFrame) {
        extractCSDH265(data, size, m_vps, m_sps, m_pps);
        if (!m_vps.empty() && !m_sps.empty() && !m_pps.empty()) {
            m_csdReady = true;
            // Reinitialize with CSD for better decoding
            release();
            return init(m_width, m_height);
        }
    }

    const auto ret = decode_frame(data, size);
    if (ret < 0) {
        [[unlikely]];

        m_needResync = true;
        qDebug() << "Resync required.";
    }

    return ret;
}

int FfmpegDecoder::decode_frame(const uint8_t* data, const size_t size)
{
    // Always queue input buffer (works for both paths)
    const auto inputIndex = AMediaCodec_dequeueInputBuffer(m_codec, 10000);
    if (inputIndex >= 0) {
        // Most likely case, we have a buffer, just continue.
        [[likely]];
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
        [[unlikely]];

        qWarning() << "Failed to get input buffer: " << inputBuffer;
        return -1;
    }
    if (size > bufferSize) {
        [[unlikely]];

        // [TODO] handle buffer resizing.
        qWarning() << "Input data too large:" << size << ">" << bufferSize;
        return -1;
    }
    memcpy(inputBuffer, data, size);

    const uint32_t flags = isKeyFrameH265(data, size) ? AMEDIACODEC_BUFFER_FLAG_KEY_FRAME : 0;
    const auto status = AMediaCodec_queueInputBuffer(
        m_codec,
        inputIndex,
        0,
        size,
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(),
        flags);
    if (status != AMEDIA_OK) {
        [[unlikely]];

        qWarning() << "MC Failed to queue input buffer:" << status;
        return -1;
    }

#ifdef USE_NAT_SURFACES
    if (m_useImageReader) {
        AMediaCodecBufferInfo info{};
        for (;;) {
            const auto outIdx = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 0);
            if (outIdx >= 0) {
                [[likely]];

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
#endif

    qDebug() << "Direct delivery";

    // CPU fallback: original output buffer processing
    AMediaCodecBufferInfo info{};
    const auto outputIndex = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 10000);
    if (outputIndex >= 0) {
        [[likely]];

        /*
        At or before API level 35, the offset in the AMediaCodecBufferInfo struct was invalid and
        should be ignored; however, at the same time the buffer size could only be obtained from
        this struct. After API level 35, the offset returned in the struct is always set to 0, and
        the buffer size can also be obtained from the AMediaCodec_getOutputBuffer() call.
        */

        size_t outBufferSize = 0;
        const auto outputBuffer = AMediaCodec_getOutputBuffer(m_codec, outputIndex, &outBufferSize);
        if (outputBuffer && outBufferSize > 0) {
            qDebug() << "Fallen back to CPU.";

            if (m_pixelFormat == 0) {
                auto outputFormat = AMediaCodec_getOutputFormat(m_codec);
                if (outputFormat) {
                    if (!AMediaFormat_getInt32(outputFormat, "color-format", &m_pixelFormat)) {
                        [[unlikely]];

                        qWarning() << "Failed to get property color-format";
                    }
                    if (!AMediaFormat_getInt32(outputFormat, AMEDIAFORMAT_KEY_STRIDE, &m_stride)) {
                        [[unlikely]];

                        qWarning() << "Failed to get property" << AMEDIAFORMAT_KEY_STRIDE;
                    }
                    if (!AMediaFormat_getInt32(outputFormat, "slice-height", &m_sliceHeight)) {
                        [[unlikely]];

                        qWarning() << "Failed to get property slice-height";
                    }

                    qDebug() << "Pixel format:" << m_pixelFormat << "stride:" << m_stride << "slice-height:" << m_sliceHeight;

                    // Update width/height from output format if available
                    int width = 0, height = 0;
                    if (AMediaFormat_getInt32(outputFormat, AMEDIAFORMAT_KEY_WIDTH, &width)
                        && AMediaFormat_getInt32(outputFormat, AMEDIAFORMAT_KEY_HEIGHT, &height)) {
                        [[likely]];

                        if (width != m_width || height != m_height) {
                            [[likely]];

                            m_width = width;
                            m_height = height;
                            qDebug() << "Resolution from output:" << m_width << "x" << m_height;
                        }
                    }

                    AMediaFormat_delete(outputFormat);
                }
            }

            if (m_width > 0 && m_height > 0) {
                const auto image = convertToQImage(outputBuffer + info.offset, info.size, m_width, m_height, m_stride, m_sliceHeight, m_pixelFormat);

                if (!image.isNull() && m_frameCallback) {
                    m_frameCallback(image);
                }
            }
        }

        AMediaCodec_releaseOutputBuffer(m_codec, outputIndex, false);
    } else {
        switch (outputIndex) {
        case AMEDIACODEC_INFO_TRY_AGAIN_LATER: {
            qDebug() << "Dequeue output TAL";
            // Nothing much to do but wait.
            break;
        }
        case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED: {
            qDebug() << "Format changed !";
            // [TODO] Handle format change.
            break;
        }
        case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED: {
            qDebug() << "Output buffers changed !";
            // [TODO] Handle output buffers change.
            break;
        }
        default:
            qWarning() << "Unexpected output result:" << outputIndex;
            return -1;
        }
    }

    return 0;
}

QImage FfmpegDecoder::convertToQImage(
    const uint8_t* data, const size_t size, const int width, const int height, const int stride, const int sliceHeight, const int pixelFormat)
{
    if (!data || width <= 0 || height <= 0) {
        [[unlikely]];

        return {};
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        [[unlikely]];

        qWarning() << "Failed to allocate QImage";
        return {};
    }

    uint8_t* dst = image.bits();

    const int actualStride = stride > 0 ? stride : width;
    const int actualSliceHeight = sliceHeight > 0 ? sliceHeight : height;

    // Common Android pixel formats
    // OMX_COLOR_FormatYUV420Planar = 0x13 (YUV420P)
    // OMX_COLOR_FormatYUV420SemiPlanar = 0x15 (NV12)
    // OMX_COLOR_FormatYUV420PackedPlanar = 0x7f000100
    // OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7fa30c04

    // For QCOM tiled format, we need to handle it differently
    const bool isQComTiled = (pixelFormat == 0x7fa30c04);
    const bool isYUV420P = (pixelFormat == 0x13 || pixelFormat == 0x7f000100);
    const bool isNV12 = (pixelFormat == 0x15 || pixelFormat == 0x7fa30c04);

    // For QCOM tiled format, the stride and slice-height are different
    if (isQComTiled) {
        // QCOM tiled format uses 64x32 tiles
        // The actual stride is aligned to 128 bytes for Y and 256 bytes for UV
        // For simplicity, we'll try to handle it as NV12 with special tiling
        // In practice, you might need to detile the data first
        qDebug() << "QCOM tiled format detected - attempting to handle as NV12";
    }

    const uint8_t* yPlane = data;
    const uint8_t* uPlane = nullptr;
    const uint8_t* vPlane = nullptr;
    const uint8_t* uvPlane = nullptr;

    if (isYUV420P) {
        // YUV420P: Y plane, then U plane, then V plane
        const size_t ySize = actualStride * actualSliceHeight;
        const size_t uvSize = (actualStride / 2) * (actualSliceHeight / 2);
        uPlane = data + ySize;
        vPlane = uPlane + uvSize;
    } else {
        // NV12: Y plane, then interleaved UV plane
        uvPlane = data + (actualStride * actualSliceHeight);
    }

    // Convert to RGBA
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int Y, U, V;

            if (isYUV420P) {
                // YUV420P (planar)
                const int yIdx = y * actualStride + x;
                const int uvIdx = (y / 2) * (actualStride / 2) + (x / 2);
                Y = yPlane[yIdx] & 0xFF;
                U = uPlane[uvIdx] & 0xFF;
                V = vPlane[uvIdx] & 0xFF;
            } else {
                // NV12 (semi-planar)
                const int yIdx = y * actualStride + x;
                const int uvIdx = (y / 2) * actualStride + (x / 2) * 2;
                Y = yPlane[yIdx] & 0xFF;
                U = uvPlane[uvIdx] & 0xFF;
                V = uvPlane[uvIdx + 1] & 0xFF;
            }

            // BT.601 YUV to RGB conversion
            int R = static_cast<int>(Y + 1.402f * (V - 128));
            int G = static_cast<int>(Y - 0.344f * (U - 128) - 0.714f * (V - 128));
            int B = static_cast<int>(Y + 1.772f * (U - 128));

            R = qBound(0, R, 255);
            G = qBound(0, G, 255);
            B = qBound(0, B, 255);

            const int pixelIdx = (y * width + x) * 4;
            dst[pixelIdx + 0] = R;
            dst[pixelIdx + 1] = G;
            dst[pixelIdx + 2] = B;
            dst[pixelIdx + 3] = 255; // Fully opaque
        }
    }

    return image;
}

#ifdef USE_NAT_SURFACES
void FfmpegDecoder::onImageAvailable(void* context, AImageReader* reader)
{
    auto self = static_cast<FfmpegDecoder*>(context);
    // Just set a flag; the actual acquisition happens on the render thread
    self->m_frameAvailable.store(true, std::memory_order_release);
}

bool FfmpegDecoder::consumeFrame()
{
    if (!m_useImageReader || !m_imageReader) {
        [[unlikely]];

        qDebug() << "Invalid consume:" << m_useImageReader << m_imageReader;
        return false;
    }

    const bool hadFrame = m_frameAvailable.exchange(false, std::memory_order_acq_rel);
    if (!hadFrame) {
        [[unlikely]];

        qDebug() << "No new frame";
        return false;
    }

    AImage* image = nullptr;
    auto status = AImageReader_acquireNextImage(m_imageReader, &image);
    switch (status) {
    case AMEDIA_OK: {
        // Most likely case, just continue.
        [[likely]];
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
    if (status != AMEDIA_OK || !hardwareBuffer) {
        [[unlikely]];

        qWarning() << "Failed to get AHardwareBuffer from image";
        AImage_delete(image);
        return false;
    }

    // Update the OpenGL texture using the hardware buffer
    updateTextureFromHardwareBuffer(hardwareBuffer);

    AImage_delete(image);
    return true;
}

void FfmpegDecoder::updateTextureFromHardwareBuffer(AHardwareBuffer* buffer)
{
    const auto display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        [[unlikely]];

        qWarning() << "No current EGL display";
        return;
    }

    const auto context = eglGetCurrentContext();
    if (context == EGL_NO_CONTEXT) {
        [[unlikely]];

        qWarning() << "No current EGL context!";
        return;
    }

    // Get EGL client buffer from AHardwareBuffer
    const auto clientBuffer = eglGetNativeClientBufferANDROID(buffer);
    if (!clientBuffer) {
        [[unlikely]];

        qWarning() << "eglGetNativeClientBufferANDROID failed";
        return;
    }

    // Create EGLImage
    constexpr EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    const auto eglImage = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);
    if (eglImage == EGL_NO_IMAGE_KHR) {
        [[unlikely]];

        qWarning() << "eglCreateImageKHR failed";
        return;
    }

    // Create/update the OES texture
    if (m_oesTextureId == 0) {
        [[unlikely]];

        glGenTextures(1, &m_oesTextureId);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTextureId);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            [[unlikely]];

            qWarning() << "glEGLImageTargetTexture2DOES failed:" << err;
        }
    }

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTextureId);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);

    const GLenum bindErr = glGetError();
    if (bindErr != GL_NO_ERROR) {
        [[unlikely]];

        qWarning() << "glEGLImageTargetTexture2DOES failed:" << bindErr;
    }

    // Destroy old EGL image (if any)
    if (m_eglImage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(display, m_eglImage);
    }

    m_eglImage = eglImage;

    const auto err = glGetError();
    if (err != GL_NO_ERROR) {
        [[unlikely]];

        qWarning() << "OpenGL error after glEGLImageTargetTexture2DOES:" << err;
    }
}
#endif

} // namespace Platform
