#ifndef FFMPEG_H
#define FFMPEG_H

#include <QObject>

#include <cstdint>
#include <functional>

class QImage;

class FfmpegDecoder : public QObject
{
    Q_OBJECT

public:
    using FrameCallback = std::function<void(const QImage &image)>;

    explicit FfmpegDecoder(QObject *parent = nullptr);

    void setFrameCallback(FrameCallback callback) { m_frameCallback = callback; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool isInitialized() const { return m_initialized; }

    virtual int decode(const uint8_t *data, size_t size) = 0;

    static FfmpegDecoder *instance();

protected:
    FrameCallback m_frameCallback = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;

private:
    static FfmpegDecoder *m_instance;
};

#endif // FFMPEG_H
