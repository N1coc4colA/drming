#include "speaker.h"

#include <QTimer>

AudioCapture* AudioCapture::m_instance = nullptr;

AudioCapture::AudioCapture(const std::string& device, QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    assert(!m_instance);

    m_instance = this;

    int err = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        throw std::runtime_error("SND Failed to open " + device + ": " + snd_strerror(err));
    }

    snd_pcm_hw_params_alloca(&params);
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        throw std::runtime_error("SND Settings error: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
    if (err < 0) {
        throw std::runtime_error("SND Access error: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S32_LE);
    if (err < 0) {
        throw std::runtime_error("SND Format error: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_channels(handle, params, Settings::channelCount);
    if (err < 0) {
        throw std::runtime_error("SND Channel error: " + std::string(snd_strerror(err)));
    }

    unsigned int rate = Settings::sampleRate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr);
    if (err < 0) {
        throw std::runtime_error("SND Sample rate error: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_period_size_near(handle, params, &m_periodSize, nullptr);
    if (err) {
        throw std::runtime_error("SND Period size error: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        throw std::runtime_error("SND Failed to apply parameters: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_get_period_size(params, &m_periodSize, nullptr);
    if (err < 0) {
        throw std::runtime_error("SND Getting period size error: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_prepare(handle);
    if (err < 0) {
        throw std::runtime_error("SND Stream preparation error: " + std::string(snd_strerror(err)));
    }

    m_bufferByteSize = m_periodSize * sizeof(Settings::audioFormat);

    if (posix_memalign(&m_buffer.data()[0], 4096, m_bufferByteSize) != 0 || posix_memalign(&m_buffer.data()[1], 4096, m_bufferByteSize) != 0) {
        qCritical() << "posix_memalign failed";

        free(m_buffer.data()[0]);
        free(m_buffer.data()[1]);
        snd_pcm_close(handle);

        throw std::runtime_error("SND Aligned memory allocation failed: " + std::string(snd_strerror(err)));
    }

    QObject::connect(m_timer, &QTimer::timeout, this, &AudioCapture::performRead);

    m_timer->setInterval(500);
    m_timer->stop();
}

AudioCapture::~AudioCapture()
{
    if (handle) {
        snd_pcm_drop(handle);
        snd_pcm_close(handle);
    }

    if (m_instance == this) {
        m_instance = nullptr;
    }

    if (m_buffer.leftData()) {
        free(m_buffer.leftData());
    }
    if (m_buffer.rightData()) {
        free(m_buffer.rightData());
    }

    snd_pcm_close(handle);
}

void AudioCapture::stop()
{
    m_timer->stop();
    snd_pcm_drop(handle);
}

void AudioCapture::start()
{
    snd_pcm_start(handle);
    m_timer->start();
}

bool AudioCapture::readFrame()
{
    const AudioBufferWriteLock lock(this->m_buffer);

    // Use mmap read with begin/commit for robustness
    const snd_pcm_channel_area_t *areas = nullptr;
    snd_pcm_uframes_t offset = 0;
    snd_pcm_uframes_t frames = m_periodSize;
    auto err = snd_pcm_mmap_begin(handle, &areas, &offset, &frames);
    if (err < 0) {
        if (err == -EPIPE) {
            qDebug() << "Overrun, recovering...";
            snd_pcm_prepare(handle);
            snd_pcm_start(handle);

            return false;
        }

        qCritical() << "mmap_begin error: " << snd_strerror(err);

        return false;
    }

    // If frames is zero, skip
    if (frames == 0) {
        m_buffer.setSize(0);
        m_buffer.setFrames(0);

        return true;
    }

    // Copy from mmap areas to our buffers (only if we need to, but we can directly use areas)
    // However, the areas give pointers to the mmap memory; we can read directly if we want.
    // To avoid copying, we can process the data in place.
    // But we already have our own buffers, so we can copy into them, and it's a bit more
    // thread-concurrent friendly.
    // The areas are for each channel, non-interleaved.
    // We'll copy to our buffers.
    for (unsigned int ch = 0; ch < Settings::channelCount; ++ch) {
        const auto src = static_cast<const Settings::audioFormat *>(areas[ch].addr) + offset;
        auto dst = static_cast<Settings::audioFormat *>(m_buffer.data()[ch]);
        std::memcpy(dst, src, frames * sizeof(int32_t));
    }

    // Commit the read
    err = snd_pcm_mmap_commit(handle, offset, frames);
    if (err < 0) {
        qDebug() << "mmap_commit error: " << snd_strerror(err);
        return false;
    }

    // Update buffer's info.
    m_buffer.setSize(frames * sizeof(Settings::audioFormat));
    m_buffer.setFrames(frames);

    return true;
}
