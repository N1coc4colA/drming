#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QImage>
#include <QQueue>

extern "C" {
#include <libavutil/pixfmt.h>
}

#include "../../ffmpeg.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

namespace Platform {

class DecoderFramePool
{
public:
    ~DecoderFramePool();

    AVFrame *request(int w, int h, AVPixelFormat fmt);
    void dispose(AVFrame *frame);
    void clear();

private:
    QQueue<AVFrame *> m_queue{};
    qsizetype m_allocated = 0;
};

class FfmpegDecoder : public ::FfmpegDecoder
{
public:
    explicit FfmpegDecoder(QObject *parent = nullptr);
    ~FfmpegDecoder();

    int init();
    void release();
    int flush();

    int decode(const uint8_t *data, size_t size) override;

private:
    int decode_frame(AVPacket *pkt);
    QImage frameToQImage(AVFrame *frame);

    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_dec = nullptr;
    SwsContext *m_sws = nullptr;
    AVFrame *m_frame = nullptr;

    AVPixelFormat m_src_fmt = AV_PIX_FMT_YUV420P;
    AVPixelFormat m_dst_fmt = AV_PIX_FMT_RGB24;

    DecoderFramePool m_pool{};
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
