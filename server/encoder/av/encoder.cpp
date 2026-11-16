#include "encoder.h"

#include <drm_fourcc.h>

#include <QDebug>

#ifndef Q_MOC_RUN
#include <ctrack.hpp>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include "libav.h"

#include "../../registrar.h"

namespace {
Registrar<Encoder::Entry>::AutoRegister s_ffmpeg_enc("libav (ffmpeg)", false, &LibAV::Encoder::createAVEncoder);
} // namespace

namespace LibAV
{

Encoder::Encoder(PacketCallback callback, const int fps)
    : ::Encoder(callback, fps)
{
    av_log_set_level(AV_LOG_INFO);
}

Encoder::~Encoder()
{
    flush();
    release();
}

int Encoder::init(const int width, const int height, const QImage::Format format, const qsizetype stride)
{
    qDebug() << "Initing FFMPEG";

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
    const AVCodec *found_codec = nullptr;
    size_t chosen = 0;
    int lastError = 0;
    bool instanciated = false;

    const auto entries = Registrar<Encoder::Entry>::instance().entries();
    for (size_t i = 0; i < entries.size(); i++) {
        auto &entry = entries[i];

        const char *encoderName = entry.name();
        const AVCodec *codec = avcodec_find_encoder_by_name(encoderName);
        qDebug() << "Candidate" << encoderName << "codec:" << (codec ? "found" : "NOT FOUND");
        if (!codec) {
            continue;
        }

        // Try to allocate and open the encoder
        AVCodecContext *test_ctx = avcodec_alloc_context3(codec);
        if (!test_ctx) {
            qDebug() << "Encoder not found:" << codec;
            continue;
        }

        instanciated = true;

        const auto setupFn = entry.setupFunction();

        // Set basic parameters for testing
        test_ctx->width = width;
        test_ctx->height = height;
        test_ctx->time_base = AVRational{ 1, m_fps };
        test_ctx->framerate = AVRational{ m_fps, 1 };
        test_ctx->pix_fmt = m_dst_fmt = setupFn(nullptr);
        // Intra frames are substantially more expensive than predictive
        // frames. Keep recovery points frequent enough for a live stream,
        // without forcing one every few frames.
        test_ctx->gop_size = std::max(m_fps * 2, 1);
        test_ctx->max_b_frames = 0;
        // Ignored when setting CRF
        // test_ctx->bit_rate = 5 * 1000 * 1000;

        if (m_dst_fmt == AV_PIX_FMT_NONE) {
            qCritical() << "Encoder candidate returned no pixel format.";
            lastError = AVERROR(EINVAL);
            avcodec_free_context(&test_ctx);
            continue;
        }

        // Apply encoder-specific setup
        const auto result = setupFn(test_ctx);
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
            chosen = i;
            qDebug() << "Using HEVC encoder:" << encoderName;
            break;
        } else {
            lastError = ret;
            qDebug() << "Encoder" << encoderName << "failed to open:" << avError(ret);
        }
    }

    if (!found_codec) {
        if (lastError) {
            qCritical() << "No working HEVC encoder found! Last error:" << avError(lastError);
        } else if (instanciated) {
            qCritical() << "No working HEVC encoder found!";
        } else {
            qCritical() << "No HEVC encoder has been found at all!";
        }

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
    m_enc->time_base = AVRational{ 1, m_fps };
    m_enc->framerate = AVRational{ m_fps, 1 };
    m_enc->pix_fmt = m_dst_fmt;
    m_enc->gop_size = std::max(m_fps * 2, 1);
    m_enc->max_b_frames = 0;
    m_enc->thread_count = 0;
    m_enc->thread_type = FF_THREAD_SLICE;
    // Ignored when setting CRF.
    // m_enc->bit_rate = 5 * 1000 * 1000;
    m_enc->color_primaries = AVCOL_PRI_BT709;
    m_enc->color_trc = AVCOL_TRC_BT709;
    m_enc->colorspace = AVCOL_SPC_BT709;
    m_enc->color_range = AVCOL_RANGE_JPEG;

    // Apply the setup for the chosen encoder
    if (entries[chosen].setupFunction()(m_enc) == AV_PIX_FMT_NONE) {
        return AVERROR(EINVAL);
    }

    int ret = avcodec_open2(m_enc, m_codec, nullptr);
    if (ret < 0) {
        qCritical() << "Failed to open encoder:" << avError(ret);
        return ret;
    }

    m_pkt = av_packet_alloc();
    if (!m_pkt) {
        return AVERROR(ENOMEM);
    }

    // Scaling context
    m_sws = sws_getContext(
        width,
        height,
        m_src_fmt,
        width,
        height,
        m_dst_fmt,
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr
    );
    if (!m_sws) {
        return AVERROR(EINVAL);
    }

    /*
     * sws_getContext() defaults to BT.601, limited range (16-235) output
     * regardless of what we set on m_enc above. That default has nothing to
     * do with what gets written into the SPS/VUI — it's the actual sample
     * values swscale produces. If we leave this unset, the bitstream will
     * *claim* full-range BT.709 (per m_enc->colorspace/color_range) while the
     * real pixel data is limited-range BT.601, and a spec-compliant decoder
     * will render it without expanding the range back out — i.e. flat,
     * washed-out colors. Keep this in sync with m_enc's color settings above.
     */
    const int *coeffs = sws_getCoefficients(SWS_CS_ITU709);
    sws_setColorspaceDetails(
        m_sws,
        coeffs,
        1, // src: unused for RGB input, kept consistent
        coeffs,
        1, // dst: 1 = full range, matches AVCOL_RANGE_JPEG above
        0,
        1 << 16,
        1 << 16
    );

    for (auto &yuv : m_yuv) {
        yuv = av_frame_alloc();
        if (!yuv) {
            return AVERROR(ENOMEM);
        }

        yuv->format = m_dst_fmt;
        yuv->width = width;
        yuv->height = height;

        yuv->color_primaries = m_enc->color_primaries;
        yuv->color_trc = m_enc->color_trc;
        yuv->colorspace = m_enc->colorspace;
        yuv->color_range = m_enc->color_range;

        // 32-byte plane alignment: FFmpeg's own examples use this. Align=1
        // works but defeats the SIMD fast paths in sws_scale and libx265.
        ret = av_frame_get_buffer(yuv, 32);
        if (ret < 0) {
            return ret;
        }
    }

    qDebug() << "Encoder initialized successfully with:" << entries[chosen].name();
    return 0;
}

void Encoder::release()
{
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    for (auto &yuv : m_yuv) {
        if (yuv) {
            av_frame_free(&yuv);
        }
    }
    if (m_enc) {
        avcodec_free_context(&m_enc);
    }
    if (m_pkt) {
        av_packet_free(&m_pkt);
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

    if (!m_pkt) {
        return AVERROR(ENOMEM);
    }

    int status = 0;
    while (1) {
        ret = avcodec_receive_packet(m_enc, m_pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) [[unlikely]] {
            break;
        }
        if (ret < 0) [[unlikely]] {
            status = ret;
            break;
        }

        if (m_callback) [[likely]] {
            m_callback(m_pkt->data, m_pkt->size, m_pkt->pts);
        }

        av_packet_unref(m_pkt);
    }

    return status;
}

int Encoder::encode(AVFrame &frame)
{
    CTRACK;

    auto *yuv = m_yuv[m_yuvIndex];
    m_yuvIndex ^= 1;

    int ret = av_frame_make_writable(yuv);
    if (ret < 0) {
        return ret;
    }

    sws_scale(m_sws, (const uint8_t *const *)frame.data, frame.linesize, 0, frame.height, yuv->data, yuv->linesize);

    yuv->pts = frame.pts;

    /*
     * Force an IDR if a prior frame's transmission was truncated (EAGAIN).
     * FFmpeg's libx265 wrapper maps AV_PICTURE_TYPE_I to X265_TYPE_IDR, so
     * the very next encoded frame becomes a recovery point. h265dataForward
     * sets the flag when sendData() fails; without this, recovery is bounded
     * by gop_size (up to ~2 s of frozen-artifact playback on the client).
     *
     * m_yuv is reused across frames, so pict_type must be cleared explicitly
     * on the non-forced path; otherwise the previous I-frame mark persists.
     */
    yuv->pict_type = m_forceKeyframe.exchange(false, std::memory_order_relaxed) ? AV_PICTURE_TYPE_I
                                                                                : AV_PICTURE_TYPE_NONE;

    if (yuv->pict_type == AV_PICTURE_TYPE_I) {
        qDebug() << "Encoding forced IDR, pts =" << yuv->pts;
    }

    ret = avcodec_send_frame(m_enc, yuv);
    if (ret < 0) [[unlikely]] {
        return ret;
    }

    if (!m_pkt) {
        return AVERROR(ENOMEM);
    }

    int status = 0;
    while (1) {
        ret = avcodec_receive_packet(m_enc, m_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) [[unlikely]] {
            break;
        }
        if (ret < 0) [[unlikely]] {
            status = ret;
            break;
        }

        if (m_callback) [[likely]] {
            m_callback(m_pkt->data, m_pkt->size, m_pkt->pts);
        }

        av_packet_unref(m_pkt);
    }

    return status;
}

void Encoder::push_image(
    const uint8_t *data,
    const int width,
    const int height,
    const QImage::Format format,
    const qsizetype stride
)
{
    CTRACK;

    if (m_width != width || m_height != height || m_format != format || m_stride != stride) [[unlikely]] {
        const auto flush_ret = flush();
        if (flush_ret < 0) [[unlikely]] {
            qWarning() << "Failed to flush encoder with code:" << flush_ret;
            return;
        }
        const auto init_ret = init(width, height, format, stride);
        if (init_ret < 0) [[unlikely]] {
            qWarning() << "Failed to reinitialize encoder with code:" << init_ret;
            return;
        }
        qDebug() << "Performed init with code:" << init_ret;
    }

    if (!m_enc) [[unlikely]] {
        // init failed earlier; nothing to encode
        return;
    }

    AVFrame frame{};
    frame.width = width;
    frame.height = height;
    frame.format = m_src_fmt;
    frame.data[0] = const_cast<uint8_t *>(data);
    frame.linesize[0] = static_cast<int>(stride);
    frame.pts = m_pts++;

    if (encode(frame) < 0) [[unlikely]] {
        qWarning() << "Failed to encode frame";
    }
}

} // namespace LibAV
