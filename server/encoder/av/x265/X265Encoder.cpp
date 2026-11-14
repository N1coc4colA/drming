#include "X265Encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include "../../../../registrar.h"

namespace {
Registrar<LibAV::Encoder::Entry>::AutoRegister s_ffmpeg_x265_enc("x265", &X265::X265Encoder::setup);
} // namespace

namespace X265 {

AVPixelFormat X265Encoder::setup(AVCodecContext *ctx)
{
    if (!ctx) {
        return AV_PIX_FMT_YUV420P;
    }
    /*
     * Preset / tune:
     *
     * - preset=fast.  Unlike ultrafast, the fast preset keeps the
     *   deblocking filter and SAO enabled, both of which are what hide
     *   block-boundary artifacts on high-motion content.  Ultrafast
     *   disables both, and combined with crf=28: sharp seams between
     *   current-frame intra blocks and stale motion-compensated
     *   blocks. fast also gives ref=3, me=hex, subme=2, merange=57 by
     *   default, which is much better motion vectors than ultrafast's
     *   dia/subme=0.
     *
     * - tune=zerolatency.  Forces rc-lookahead=0, bframes=0, scenecut=0,
     *   slices=1, lookahead-slices=0. Required for the encode step to
     *   fit inside the per-frame budget at 40+ fps.
     */
    av_opt_set(ctx->priv_data, "preset", "fast", 0);
    av_opt_set(ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(ctx->priv_data, "global_header", "1", 0);
    av_opt_set_int(ctx->priv_data, "forced-idr", 1, 0);

    /*
     * The only three parameters we override on top of preset+tune:
     *
     * - crf=22. Previous 28 is very aggressive on 1080p desktop
     *   content. At 22 the bitrate rises ~4x but the encoder still
     *   fits comfortably in the budget (measured ~8 ms at 40 fps
     *   against a 25 ms window).
     *
     * - keyint / min-keyint.  Tied to ctx->gop_size (fps * 2), so a
     *   recovery IDR arrives every ~2 seconds.  Shorter than that
     *   raises bitrate and encode time without a meaningful latency
     *   benefit given the RequestKeyframe path handles on-demand IDRs.
     *
     * - no-open-gop=1.  Every IDR must be a self-contained recovery
     *   point; some hardware decoders (notably MediaTek's HEVC) have
     *   issues with open-GOP CRA frames arriving after a lost frame.
     *
     * Everything else (deblock, sao, weightp, ref, me, subme, merange,
     * bframes, rc-lookahead, scenecut, lookahead-slices) comes from the
     * preset / tune defaults above and is intentionally left at those
     * values.
     */
    av_opt_set(
        ctx->priv_data,
        "x265-params",
        ("no-open-gop=1:"
         "crf=22:"
         "keyint="
            + std::to_string(ctx->gop_size) + ":" + "min-keyint=" + std::to_string(ctx->gop_size) + ":")
            .c_str(),
        0
    );

    return AV_PIX_FMT_YUV420P;
}
} // namespace X265
