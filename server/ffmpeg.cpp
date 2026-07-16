#include "ffmpeg.h"

#include <QDebug>

#include <drm_fourcc.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include "parameters.h"

namespace Ffmpeg {

QString avError(const int err)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, errbuf, sizeof(errbuf));

    return errbuf;
}

// Update the setup functions with correct pixel formats

AVPixelFormat setupNVENC(AVCodecContext *ctx)
{
    if (ctx) {
        av_opt_set(ctx->priv_data, "preset", "p1", 0);
        av_opt_set(ctx->priv_data, "tune", "ll", 0);
        av_opt_set_int(ctx->priv_data, "qp", -1, 0);
        av_opt_set(ctx->priv_data, "rc", "cbr", 0);
        av_opt_set_int(ctx->priv_data, "b_ref_mode", 0, 0);
        av_opt_set_int(ctx->priv_data, "lookahead", 0, 0);
        av_opt_set_int(ctx->priv_data, "no_scenecut", 1, 0);
        av_opt_set(ctx->priv_data, "global_header", "1", 0);
    }
    return AV_PIX_FMT_NV12;
}

AVPixelFormat setupAMF(AVCodecContext *ctx)
{
    if (ctx) {
        av_opt_set(ctx->priv_data, "quality", "speed", 0);
        av_opt_set(ctx->priv_data, "usage", "ultralowlatency", 0);
        av_opt_set_int(ctx->priv_data, "qp_i", 28, 0);
        av_opt_set_int(ctx->priv_data, "qp_p", 28, 0);
        av_opt_set_int(ctx->priv_data, "qp_b", 30, 0);
        av_opt_set_int(ctx->priv_data, "b", ctx->bit_rate, 0);
        av_opt_set_int(ctx->priv_data, "maxrate", ctx->bit_rate * 1.5, 0);
        av_opt_set(ctx->priv_data, "global_header", "1", 0);
    }
    return AV_PIX_FMT_NV12; // AMF supports NV12
}

AVPixelFormat setupQSV(AVCodecContext *ctx)
{
    if (ctx) {
        // QSV doesn't support "ultrafast" - use "veryfast" or remove it
        // av_opt_set(ctx->priv_data, "preset", "veryfast", 0);
        av_opt_set(ctx->priv_data, "low_power", "1", 0);
        av_opt_set_int(ctx->priv_data, "global_quality", 25, 0);
        av_opt_set(ctx->priv_data, "look_ahead", "0", 0);
        av_opt_set(ctx->priv_data, "global_header", "1", 0);
        // Set GOP size as string
        char gop_str[16];
        snprintf(gop_str, sizeof(gop_str), "%d", ctx->gop_size);
        av_opt_set(ctx->priv_data, "gop_size", gop_str, 0);
    }
    return AV_PIX_FMT_NV12; // QSV requires NV12
}

AVPixelFormat setupVAAPI(AVCodecContext *ctx)
{
    if (ctx) {
        // VA-API requires special handling - we'll skip it for now
        // since it needs hardware frames context
        av_opt_set(ctx->priv_data, "compression_level", "1", 0);
        av_opt_set_int(ctx->priv_data, "global_quality", 25, 0);
        av_opt_set_int(ctx->priv_data, "qp", 28, 0);
        av_opt_set(ctx->priv_data, "global_header", "1", 0);
    }
    return AV_PIX_FMT_VAAPI; // VA-API requires VAAPI pixel format
}

AVPixelFormat setupLibX265(AVCodecContext *ctx)
{
    if (ctx) {
        // Critical settings for low latency
        av_opt_set(ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(ctx->priv_data, "tune", "zerolatency", 0);
        av_opt_set(ctx->priv_data, "global_header", "1", 0);
        av_opt_set_int(ctx->priv_data, "crf", 28, 0);

        // Force no lookahead and no b-frames
        av_opt_set(ctx->priv_data,
                   "x265-params",
                   ("bframes=0:"
                    "rc-lookahead=0:"
                    "lookahead-slices=0:"
                    "scenecut=0:"
                    "no-sao=1:"
                    "no-deblock=1:"
                    "no-strong-intra-smoothing=1:"
                    "ref=1:"
                    "me=dia:"
                    "subme=0:"
                    "merange=16:"
                    "no-rect=1:"
                    "no-amp=1:"
                    "early-skip=1:"
                    "fast-intra=1:"
                    "no-weightb=1:"
                    "no-weightp=1:"
                    "no-open-gop=1:"
                    "keyint="
                    + std::to_string(ctx->gop_size) + ":" + "min-keyint=" + std::to_string(ctx->gop_size) + ":")
                       .c_str(),
                   0);
    }
    return AV_PIX_FMT_YUV420P;
}

FramePool::~FramePool()
{
    clear();
}

void FramePool::clear()
{
    if (m_queue.length() < m_allocated) {
        [[unlikely]];

        // [TODO] Generate an error message here.
        exit(1);
    }

    for (auto &ptr : m_queue) {
        av_frame_free(&ptr);
    }
}

AVFrame *FramePool::request(const int w, const int h, const AVPixelFormat fmt)
{
    if (m_queue.isEmpty()) {
        [[unlikely]];

        auto frame = av_frame_alloc();
        frame->format = fmt;
        frame->width = w;
        frame->height = h;
        frame->pts = 0; // [NOTE] For ow, we leave it that way.

        m_allocated++;
        return frame;
    }

    return m_queue.dequeue();
}

void FramePool::dispose(AVFrame *frame)
{
    m_queue.enqueue(frame);
}

AVPixelFormat Encoder::formatFromFcc(const uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
        return AV_PIX_FMT_ARGB;
    case DRM_FORMAT_ABGR8888:
        return AV_PIX_FMT_ABGR;
    case DRM_FORMAT_RGB888:
        return AV_PIX_FMT_RGB24;
    case DRM_FORMAT_BGR888:
        return AV_PIX_FMT_BGR24;
    case DRM_FORMAT_RGB565:
        return AV_PIX_FMT_RGB565;
    default:
        return AV_PIX_FMT_NONE;
    }
}

AVPixelFormat Encoder::formatFromQt(const QImage::Format format)
{
    switch (format) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
    case QImage::Format_Indexed8:
    case QImage::Format_Grayscale8:
    case QImage::Format_Alpha8:
        return AV_PIX_FMT_GRAY8;

    case QImage::Format_RGB16:
        return AV_PIX_FMT_RGB565LE;
    case QImage::Format_RGB555:
        return AV_PIX_FMT_RGB555LE;
    case QImage::Format_RGB444:
        return AV_PIX_FMT_RGB444LE;
    case QImage::Format_Grayscale16:
        return AV_PIX_FMT_GRAY16LE;

    case QImage::Format_RGB888:
        return AV_PIX_FMT_RGB24;
    case QImage::Format_BGR888:
        return AV_PIX_FMT_BGR24;

    // 32-bit formats - corrected for little-endian memory layout
    case QImage::Format_RGB32:
        return AV_PIX_FMT_BGR0; // [B, G, R, 0]

    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return AV_PIX_FMT_BGRA; // [B, G, R, A]

    case QImage::Format_RGBX8888:
        return AV_PIX_FMT_RGB0; // [R, G, B, 0]

    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return AV_PIX_FMT_RGBA; // [R, G, B, A]

    case QImage::Format_ARGB8565_Premultiplied:
    case QImage::Format_ARGB6666_Premultiplied:
    case QImage::Format_ARGB8555_Premultiplied:
        return AV_PIX_FMT_BGRA; // Convert to standard BGRA
    case QImage::Format_RGB666:
        return AV_PIX_FMT_RGB24;
    case QImage::Format_CMYK8888:
        qWarning() << "Format CMYK8888 unsupported by FFmpeg";
        return AV_PIX_FMT_NONE;

    case QImage::Format_BGR30:
        return AV_PIX_FMT_X2BGR10LE;
    case QImage::Format_RGB30:
        return AV_PIX_FMT_X2RGB10LE;
    case QImage::Format_A2BGR30_Premultiplied:
        return AV_PIX_FMT_X2BGR10LE;
    case QImage::Format_A2RGB30_Premultiplied:
        return AV_PIX_FMT_X2RGB10LE;

    case QImage::Format_RGBX64:
        return AV_PIX_FMT_RGBA64LE;
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        return AV_PIX_FMT_RGBA64LE;

    case QImage::Format_RGBX16FPx4:
        return AV_PIX_FMT_RGBAF16LE;
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return AV_PIX_FMT_RGBAF16LE;

    case QImage::Format_RGBX32FPx4:
        return AV_PIX_FMT_RGBAF32LE;
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return AV_PIX_FMT_RGBAF32LE;

    case QImage::Format_Invalid:
    default:
        qWarning() << "QImage::Format unsupported by FFmpeg:" << format;
        return AV_PIX_FMT_NONE;
    }
}

Encoder::Encoder(PacketCallback callback, const int fps)
    : m_callback(callback)
    , m_fps(fps)
{
    av_log_set_level(AV_LOG_INFO);
}

Encoder::~Encoder()
{
    flush();
    release();
}

int Encoder::init(const int width, const int height, const QImage::Format format, const uint32_t stride)
{
    release();

    m_width = width;
    m_height = height;
    m_stride = stride;
    m_src_fmt = formatFromQt(format);
    m_pts = 0;
    m_format = format;

    if (m_src_fmt == AV_PIX_FMT_NONE) {
        return AVERROR(EINVAL);
    }

    // Try each encoder in order, actually testing if it works
    struct EncoderCandidate
    {
        const char *name;
        QString displayName;
        AVPixelFormat (*setupFunc)(AVCodecContext *);
    };

    const EncoderCandidate candidates[] = {{"hevc_nvenc", "NVENC", setupNVENC},
                                           {"hevc_amf", "AMF", setupAMF},
                                           //{"hevc_qsv", "QSV", setupQSV},
                                           //{"hevc_vaapi", "VA-API", setupVAAPI},
                                           {"libx265", "libx265 (software)", setupLibX265}};

    const AVCodec *found_codec = nullptr;
    EncoderCandidate chosen{};
    int lastError = 0;

    for (const auto &candidate : candidates) {
        const AVCodec *codec = avcodec_find_encoder_by_name(candidate.name);
        if (!codec) {
            continue;
        }

        // Try to allocate and open the encoder
        AVCodecContext *test_ctx = avcodec_alloc_context3(codec);
        if (!test_ctx) {
            continue;
        }

        // Set basic parameters for testing
        test_ctx->width = width;
        test_ctx->height = height;
        test_ctx->time_base = (AVRational) {1, m_fps};
        test_ctx->framerate = (AVRational) {m_fps, 1};
        test_ctx->pix_fmt = m_dst_fmt = candidate.setupFunc(nullptr);
        test_ctx->gop_size = m_fps / 2;
        test_ctx->max_b_frames = 0;
        test_ctx->bit_rate = 5 * 1000 * 1000;

        // Apply encoder-specific setup
        const auto result = candidate.setupFunc(test_ctx);
        if (result == AV_PIX_FMT_NONE) {
            avcodec_free_context(&test_ctx);
            continue;
        }

        // Try to open the encoder
        int ret = avcodec_open2(test_ctx, codec, nullptr);
        avcodec_free_context(&test_ctx);

        if (ret == 0) {
            // This encoder works!
            found_codec = codec;
            chosen = candidate;
            qDebug() << "Using HEVC encoder:" << chosen.displayName;
            break;
        } else {
            lastError = ret;
            qDebug() << "Encoder" << candidate.name << "failed to open:" << avError(ret);
        }
    }

    if (!found_codec) {
        qCritical() << "No working HEVC encoder found! Last error:" << avError(lastError);
        return AVERROR_ENCODER_NOT_FOUND;
    }

    m_codec = found_codec;

    // Now actually initialize the encoder with the working one
    m_enc = avcodec_alloc_context3(m_codec);
    if (!m_enc) {
        return AVERROR(ENOMEM);
    }

    // Common settings
    m_enc->width = width;
    m_enc->height = height;
    m_enc->time_base = (AVRational) {1, m_fps};
    m_enc->framerate = (AVRational) {m_fps, 1};
    m_enc->pix_fmt = m_dst_fmt;
    m_enc->gop_size = m_fps / 2;
    m_enc->max_b_frames = 0;
    m_enc->thread_count = 0;
    m_enc->thread_type = FF_THREAD_SLICE;
    m_enc->bit_rate = 5 * 1000 * 1000;
    av_opt_set_int(m_enc->priv_data, "rc_lookahead", 0, 0);
    av_opt_set_int(m_enc->priv_data, "lookahead_slices", 0, 0);

    if (chosen.displayName == "libx265 (software)") {
        av_opt_set_int(m_enc->priv_data, "rc_lookahead", 0, 0);
        av_opt_set_int(m_enc->priv_data, "lookahead_slices", 0, 0);
        av_opt_set(m_enc->priv_data,
                   "x265-params",
                   "no-sao=1:no-deblock=1:no-strong-intra-smoothing=1:rc-lookahead=0:scenecut=0:bframes=0:lookahead-slices=0",
                   0);
    }

    // Apply the setup for the chosen encoder
    if (chosen.setupFunc(m_enc) == AV_PIX_FMT_NONE) {
        return AVERROR(EINVAL);
    }

    int ret = avcodec_open2(m_enc, m_codec, nullptr);
    if (ret < 0) {
        qCritical() << "Failed to open encoder:" << avError(ret);
        return ret;
    }

    // Scaling context
    m_sws = sws_getContext(width, height, m_src_fmt, width, height, m_dst_fmt, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_sws) {
        return AVERROR(EINVAL);
    }

    m_yuv = av_frame_alloc();
    if (!m_yuv) {
        return AVERROR(ENOMEM);
    }

    m_yuv->format = m_dst_fmt;
    m_yuv->width = width;
    m_yuv->height = height;

    ret = av_frame_get_buffer(m_yuv, 1);
    if (ret < 0) {
        return ret;
    }

    qDebug() << "Encoder initialized successfully with:" << chosen.displayName;
    return 0;
}

void Encoder::release()
{
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_yuv) {
        av_frame_free(&m_yuv);
    }
    if (m_enc) {
        avcodec_free_context(&m_enc);
    }
}

int Encoder::flush()
{
    if (!m_enc) {
        return 0;
    }

    int ret = avcodec_send_frame(m_enc, nullptr);
    if (ret < 0) {
        return ret;
    }

    while (1) {
        AVPacket pkt;
        av_init_packet(&pkt);

        ret = avcodec_receive_packet(m_enc, &pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            [[unlikely]];

            break;
        }
        if (ret < 0) {
            [[unlikely]];

            return ret;
        }

        if (m_callback) {
            [[likely]];

            m_callback(pkt.data, pkt.size, pkt.pts);
        }

        av_packet_unref(&pkt);
    }

    return 0;
}

int Encoder::encode(AVFrame *frame)
{
    int ret = av_frame_make_writable(m_yuv);
    if (ret < 0) {
        return ret;
    }

    sws_scale(m_sws, (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height, m_yuv->data, m_yuv->linesize);

    m_yuv->pts = frame->pts;

    ret = avcodec_send_frame(m_enc, m_yuv);
    if (ret < 0) {
        [[unlikely]];

        return ret;
    }

    while (1) {
        AVPacket pkt;
        av_init_packet(&pkt);

        ret = avcodec_receive_packet(m_enc, &pkt); // Can this be used from separate thread ?
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            [[unlikely]];

            break;
        }
        if (ret < 0) {
            [[unlikely]];

            return ret;
        }

        if (m_callback) {
            [[likely]];

            m_callback(pkt.data, pkt.size, pkt.pts);
        }

        av_packet_unref(&pkt);
    }

    return 0;
}

void Encoder::push_image(const uint8_t *data, const int width, const int height, const QImage::Format format, const uint32_t stride)
{
    if (m_width != width || m_height != height || m_format != format || m_stride != stride) {
        [[unlikely]];

        if (flush() < 0) {
            [[unlikely]];

            qWarning() << "Failed to flush encoder";
            return;
        }
        if (init(width, height, format, stride) < 0) {
            [[unlikely]];

            qWarning() << "Failed to reinitialize encoder";
            return;
        }
    }

    auto frame = m_pool.request(width, height, m_src_fmt);
    if (!frame) {
        qWarning() << "Failed to allocate source frame";
        return;
    }

    frame->width = width;
    frame->height = height;
    frame->format = m_src_fmt;

    if (av_image_fill_arrays(frame->data, frame->linesize, data, m_src_fmt, width, height, 1) < 0) {
        qWarning() << "Failed to map source image into frame";
        av_frame_unref(frame);
        m_pool.dispose(frame);
        return;
    }

    frame->pts = m_pts++;

    if (encode(frame) < 0) {
        [[unlikely]];

        qWarning() << "Failed to encode frame";
    }

    av_frame_unref(frame);
    m_pool.dispose(frame);
}

} // namespace Ffmpeg
