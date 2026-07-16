#include "ffmpeg.h"

#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace Platform {

QString avError(const int err)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, errbuf, sizeof(errbuf));

    return errbuf;
}

DecoderFramePool::~DecoderFramePool()
{
    clear();
}

void DecoderFramePool::clear()
{
    for (auto &ptr : m_queue) {
        av_frame_free(&ptr);
    }
    m_queue.clear();
    m_allocated = 0;
}

AVFrame *DecoderFramePool::request(const int w, const int h, const AVPixelFormat fmt)
{
    if (m_queue.isEmpty()) {
        auto frame = av_frame_alloc();
        if (!frame) {
            return nullptr;
        }

        frame->format = fmt;
        frame->width = w;
        frame->height = h;
        m_allocated++;

        return frame;
    }
    return m_queue.dequeue();
}

void DecoderFramePool::dispose(AVFrame *frame)
{
    if (frame) {
        m_queue.enqueue(frame);
    }
}

FfmpegDecoder::FfmpegDecoder(QObject *parent)
    : ::FfmpegDecoder(parent)
{
    av_log_set_level(AV_LOG_WARNING);
}

FfmpegDecoder::~FfmpegDecoder()
{
    flush();
    release();
}

int FfmpegDecoder::init()
{
    release();

    m_codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!m_codec) {
        qCritical() << "HEVC decoder not found";
        return AVERROR_DECODER_NOT_FOUND;
    }

    m_dec = avcodec_alloc_context3(m_codec);
    if (!m_dec) {
        qCritical() << "Failed to allocate decoder context";
        return AVERROR(ENOMEM);
    }

    // Enable multi-threading
    m_dec->thread_count = 0; // Auto-detect
    m_dec->thread_type = FF_THREAD_SLICE;
    m_dec->thread_count = 1;
    m_dec->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_dec->flags2 |= AV_CODEC_FLAG2_FAST;

    int ret = avcodec_open2(m_dec, m_codec, NULL);
    if (ret < 0) {
        qCritical() << "Failed to open decoder:" << avError(ret);
        return ret;
    }

    m_frame = av_frame_alloc();
    if (!m_frame) {
        qCritical() << "Failed to allocate frame";
        return AVERROR(ENOMEM);
    }

    m_initialized = true;
    qDebug() << "HEVC decoder initialized";

    return 0;
}

void FfmpegDecoder::release()
{
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_dec) {
        avcodec_free_context(&m_dec);
    }
    m_initialized = false;
}

int FfmpegDecoder::decode(const uint8_t *data, const size_t size)
{
    if (!m_initialized) {
        const int ret = init();
        if (ret < 0) {
            return ret;
        }
    }

    bool isKeyFrame = false;
    if (size >= 4) {
        // Check for start code (0x00 0x00 0x00 0x01)
        if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) {
            uint8_t nalType = (data[4] >> 1) & 0x3F;
            isKeyFrame = (nalType == 19 || nalType == 20 || nalType == 21);
        }
    }

    if (m_needResync && isKeyFrame) {
        avcodec_flush_buffers(m_dec);
        m_needResync = false;
        qDebug() << "Decoder synced to keyframe";
    }

    // If we're out of sync and this is not a keyframe, skip it
    if (m_needResync && !isKeyFrame) {
        qDebug() << "Skipping non-keyframe while out of sync";
        return 0;
    }

    AVPacket pkt{};
    av_init_packet(&pkt);
    pkt.data = const_cast<uint8_t *>(data);
    pkt.size = static_cast<int>(size);

    const int ret = decode_frame(&pkt);
    av_packet_unref(&pkt);

    if (ret < 0) {
        m_needResync = true;
    }

    return ret;
}

int FfmpegDecoder::decode_frame(AVPacket *pkt)
{
    int ret = avcodec_send_packet(m_dec, pkt);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            qWarning() << "Error sending packet to decoder:" << avError(ret);
        }
        return ret;
    }

    while (true) {
        ret = avcodec_receive_frame(m_dec, m_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            qWarning() << "Error receiving frame from decoder:" << avError(ret);
            return ret;
        }

        // Convert decoded frame to QImage
        const QImage image = frameToQImage(m_frame);
        if (!image.isNull() && m_frameCallback) {
            m_frameCallback(image);
        }

        av_frame_unref(m_frame);
    }

    return 0;
}

int FfmpegDecoder::flush()
{
    if (!m_initialized) {
        return 0;
    }

    int ret = avcodec_send_packet(m_dec, nullptr);
    if (ret < 0) {
        return ret;
    }

    while (true) {
        ret = avcodec_receive_frame(m_dec, m_frame);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            return ret;
        }

        QImage image = frameToQImage(m_frame);
        if (!image.isNull() && m_frameCallback) {
            m_frameCallback(image);
        }

        av_frame_unref(m_frame);
    }

    return 0;
}

QImage FfmpegDecoder::frameToQImage(AVFrame *frame)
{
    if (!frame) {
        return {};
    }

    // Update dimensions if changed
    if (frame->width != m_width || frame->height != m_height || frame->format != m_src_fmt) {
        m_width = frame->width;
        m_height = frame->height;
        m_src_fmt = static_cast<AVPixelFormat>(frame->format);

        if (m_sws) {
            sws_freeContext(m_sws);
            m_sws = nullptr;
        }

        m_sws = sws_getContext(m_width, m_height, m_src_fmt, m_width, m_height, m_dst_fmt, SWS_BILINEAR, NULL, NULL, NULL);

        if (!m_sws) {
            qWarning() << "Failed to create scale context";
            return {};
        }
    }

    // Allocate destination buffer
    QImage image(m_width, m_height, QImage::Format_RGB888);
    if (image.isNull()) {
        qWarning() << "Failed to allocate QImage";
        return {};
    }

    uint8_t *dst_data[1] = {image.bits()};
    const int dst_linesize[1] = {static_cast<int>(image.bytesPerLine())};

    // Convert YUV to RGB
    sws_scale(m_sws, (const uint8_t *const *) frame->data, frame->linesize, 0, m_height, dst_data, dst_linesize);

    return image;
}

} // namespace Platform
