#include "ffmpeg.h"

#include <QDebug>

namespace Platform {

// Helper: Check if data is a keyframe (H.265)
bool isKeyFrameH265(const uint8_t* data, const size_t size)
{
    if (size < 2)
        return false;

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
    m_resolutionDetected = true;

    m_codec = AMediaCodec_createDecoderByType("video/hevc");
    if (!m_codec) {
        [[unlikely]];
        qCritical() << "Failed to create MediaCodec decoder";
        return -1;
    }

    m_format = AMediaFormat_new();
    AMediaFormat_setString(m_format, AMEDIAFORMAT_KEY_MIME, "video/hevc");
    AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_HEIGHT, height);

    // Low latency settings
    AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_PRIORITY, 0);

    // Set CSD if available
    if (m_csdReady) {
        if (!m_vps.empty()) {
            AMediaFormat_setBuffer(m_format, "csd-0", &m_vps[0], m_vps.size());
        }
        if (!m_sps.empty()) {
            AMediaFormat_setBuffer(m_format, "csd-1", &m_sps[0], m_sps.size());
        }
        if (!m_pps.empty()) {
            AMediaFormat_setBuffer(m_format, "csd-2", &m_pps[0], m_pps.size());
        }
    }

    media_status_t status = AMediaCodec_configure(m_codec, m_format, nullptr, nullptr, 0);
    if (status != AMEDIA_OK) {
        [[unlikely]];
        qCritical() << "Failed to configure MediaCodec:" << status;
        return -1;
    }

    status = AMediaCodec_start(m_codec);
    if (status != AMEDIA_OK) {
        [[unlikely]];
        qCritical() << "Failed to start MediaCodec:" << status;
        return -1;
    }

    m_initialized = true;
    qDebug() << "MediaCodec decoder initialized with resolution:" << width << "x" << height;
    return 0;
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

    m_initialized = false;
    m_resolutionDetected = false;
    m_width = 0;
    m_height = 0;
    m_stride = 0;
    m_sliceHeight = 0;
    m_pixelFormat = 0;

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
    // If we don't have resolution, use a default
    if (!m_resolutionDetected) {
        qWarning() << "No resolution detected, using default 1920x1080";
        int ret = init(1920, 1080);
        if (ret < 0) {
            [[unlikely]];
            return ret;
        }
    }

    if (!m_initialized) {
        int ret = init(m_width, m_height);
        if (ret < 0) {
            return ret;
        }
    }

    const auto isKeyFrame = isKeyFrameH265(data, size);
    if (isKeyFrame && m_needResync) {
        AMediaCodec_flush(m_codec);
        m_needResync = false;
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
    }

    return ret;
}

int FfmpegDecoder::decode_frame(const uint8_t* data, const size_t size)
{
    ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(m_codec, 10000);
    if (inputIndex < 0) {
        [[unlikely]];
        qWarning() << "Failed to dequeue input buffer";
        return -1;
    }

    size_t bufferSize = 0;
    uint8_t* inputBuffer = AMediaCodec_getInputBuffer(m_codec, inputIndex, &bufferSize);
    if (!inputBuffer) {
        [[unlikely]];
        qWarning() << "Failed to get input buffer";
        return -1;
    }

    if (size > bufferSize) {
        [[unlikely]];
        qWarning() << "Input data too large:" << size << ">" << bufferSize;
        return -1;
    }

    memcpy(inputBuffer, data, size);

    const uint32_t flags = isKeyFrameH265(data, size) ? AMEDIACODEC_BUFFER_FLAG_KEY_FRAME : 0;
    const media_status_t status = AMediaCodec_queueInputBuffer(
        m_codec,
        inputIndex,
        0,
        size,
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(),
        flags);

    if (status != AMEDIA_OK) {
        [[unlikely]];
        qWarning() << "Failed to queue input buffer:" << status;
        return -1;
    }

    AMediaCodecBufferInfo info{};
    ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 10000);

    if (outputIndex >= 0) {
        if (info.size > 0) {
            size_t outBufferSize = 0;
            const uint8_t* outputBuffer = AMediaCodec_getOutputBuffer(m_codec, outputIndex, &outBufferSize);

            if (outputBuffer && info.size > 0) {
                // Detect format if not known
                if (m_pixelFormat == 0) {
                    AMediaFormat* outputFormat = AMediaCodec_getOutputFormat(m_codec);
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
                    const QImage image
                        = convertToQImage(outputBuffer + info.offset, info.size, m_width, m_height, m_stride, m_sliceHeight, m_pixelFormat);

                    if (!image.isNull() && m_frameCallback) {
                        m_frameCallback(image);
                    }
                }
            }
        }

        AMediaCodec_releaseOutputBuffer(m_codec, outputIndex, false);
    } else if (outputIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        [[unlikely]];

        AMediaFormat* outputFormat = AMediaCodec_getOutputFormat(m_codec);
        if (outputFormat) {
            int width = 0, height = 0;
            if (AMediaFormat_getInt32(outputFormat, AMEDIAFORMAT_KEY_WIDTH, &width)
                && AMediaFormat_getInt32(outputFormat, AMEDIAFORMAT_KEY_HEIGHT, &height)) {
                m_width = width;
                m_height = height;
                m_resolutionDetected = true;
                qDebug() << "Format changed - resolution:" << m_width << "x" << m_height;
            }
            AMediaFormat_getInt32(outputFormat, "pixel-format", &m_pixelFormat);
            AMediaFormat_getInt32(outputFormat, AMEDIAFORMAT_KEY_STRIDE, &m_stride);
            AMediaFormat_getInt32(outputFormat, "slice-height", &m_sliceHeight);

            qDebug() << "Format changed - pixel-format:" << m_pixelFormat << "stride:" << m_stride << "slice-height:" << m_sliceHeight;

            AMediaFormat_delete(outputFormat);
        }
        return 0;
    }

    return 0;
}

QImage FfmpegDecoder::convertToQImage(
    const uint8_t* data, const size_t size, const int width, const int height, const int stride, const int sliceHeight, const int pixelFormat)
{
    if (!data || width <= 0 || height <= 0) {
        return {};
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        [[unlikely]];
        qWarning() << "Failed to allocate QImage";
        return {};
    }

    uint8_t* dst = image.bits();

    int actualStride = stride > 0 ? stride : width;
    int actualSliceHeight = sliceHeight > 0 ? sliceHeight : height;

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

} // namespace Platform
