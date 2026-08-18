#include "audioplayer.h"

#include <QtDebug>

#include "../settings.h"

namespace Platform {
AudioPlayer::AudioPlayer()
    : stream(nullptr)
    , running(false)
{}

bool AudioPlayer::open()
{
    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || builder == nullptr) {
        qCritical() << "AAudioStream builder creation error:" << result;
        return false;
    }

    // Configuration
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I32); // S32_LE
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setChannelCount(builder, Settings::channelCount);
    AAudioStreamBuilder_setSampleRate(builder, Settings::sampleRate);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, 1024);

    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || stream == nullptr) {
        qCritical() << "AAudioStream building failed:" << result;
        return false;
    }

    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        qCritical() << "AAudioStream start error: " << result;
        return false;
    }

    running = true;

    return true;
}

int AudioPlayer::write(const Settings::audioFormat* left, const Settings::audioFormat* right, const int numFrames)
{
    if (!stream || !running || !numFrames) {
        qDebug() << stream << running << numFrames;
        return -1;
    }

    // Perform interleave
    {
        m_interleaved.resize(numFrames);
        Settings::audioFormat* dst = m_interleaved.data();
        const Settings::audioFormat* l = left;
        const Settings::audioFormat* r = right;

        // Let the compiler do its magic.
        for (int i = 0; i < numFrames; ++i) {
            *dst++ = *l++;
            *dst++ = *r++;
        }
    }

    // 0 timeout for non-blocking.
    const aaudio_result_t result = AAudioStream_write(stream, m_interleaved.data(), numFrames, 0);

    if (result < 0) [[unlikely]] {
        if (result == AAUDIO_ERROR_INTERNAL) {
            qDebug() << "Not enough data !";
            return 0;
        } else {
            qWarning() << "Write error:" << result;
            return -1;
        }
    }

    return static_cast<int>(result);
}

void AudioPlayer::close()
{
    running = false;
    if (stream) {
        AAudioStream_requestStop(stream);
        AAudioStream_close(stream);
        stream = nullptr;
    }
}
} // namespace Platform
