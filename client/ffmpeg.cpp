#include "ffmpeg.h"

#include <QCoreApplication>

#ifdef Q_OS_ANDROID
#include "platform/android/ffmpeg.h"
#else
#include "platform/linux/ffmpeg.h"
#endif

FfmpegDecoder *FfmpegDecoder::m_instance = nullptr;

FfmpegDecoder *FfmpegDecoder::instance()
{
    if (!m_instance) {
        m_instance = new Platform::FfmpegDecoder(qApp);
    }

    return m_instance;
}

FfmpegDecoder::FfmpegDecoder(QObject *parent)
    : QObject(parent)
{
    assert(!m_instance);
    m_instance = this;
}
