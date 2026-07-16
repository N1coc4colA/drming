#ifndef FFMPEGPLATFORM_H
#define FFMPEGPLATFORM_H

#include <QImage>

#include <condition_variable>
#include <mutex>
#include <queue>

extern "C" {
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
}

#include "../../ffmpeg.h"

namespace Platform {

class FfmpegDecoder : public ::FfmpegDecoder
{
    Q_OBJECT

public:
    explicit FfmpegDecoder(QObject* parent = nullptr);
    ~FfmpegDecoder();

    int init(int width = 0, int height = 0);
    void release();
    int flush();
    int decode(const uint8_t* data, size_t size) override;

private:
    int decode_frame(const uint8_t* data, size_t size);
    QImage convertToQImage(const uint8_t* data, size_t size, int width, int height, int stride, int sliceHeight, int pixelFormat);

    AMediaCodec* m_codec = nullptr;
    AMediaFormat* m_format = nullptr;

    // CSD data (VPS, SPS, PPS)
    std::vector<uint8_t> m_vps{};
    std::vector<uint8_t> m_sps{};
    std::vector<uint8_t> m_pps{};
    bool m_csdReady = false;

    // Frame queue for output
    std::queue<QImage> m_frameQueue{};
    std::mutex m_queueMutex{};
    std::condition_variable m_queueCond{};

    // Decoder state
    bool m_initialized = false;
    bool m_resolutionDetected = false;
    int m_width = 0;
    int m_height = 0;
    int m_stride = 0;
    int m_sliceHeight = 0;
    int m_pixelFormat = 0;

    bool isDecoderAlive();
};

} // namespace Platform

#endif // FFMPEGPLATFORM_H
