#include "speaker.h"

#include <QTimer>

AudioCapture* AudioCapture::m_instance = nullptr;

AudioCapture::AudioCapture(const std::string& device, QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    assert(!m_instance);

    m_instance = this;

    // 1. Ouvrir le périphérique en mode capture
    int err = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        throw std::runtime_error("Impossible d'ouvrir " + device + ": " + snd_strerror(err));
    }

    // 2. Allouer et initialiser les paramètres matériels
    snd_pcm_hw_params_alloca(&params);
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        throw std::runtime_error("Erreur paramètres: " + std::string(snd_strerror(err)));
    }

    // 3. Configurer les paramètres
    // Format : 16 bits signé, stéréo, 16000 Hz
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        throw std::runtime_error("Erreur accès: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        throw std::runtime_error("Erreur format: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_channels(handle, params, Settings::channelCount);
    if (err < 0) {
        throw std::runtime_error("Erreur canaux: " + std::string(snd_strerror(err)));
    }

    unsigned int rate = Settings::sampleRate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr);
    if (err < 0) {
        throw std::runtime_error("Erreur débit: " + std::string(snd_strerror(err)));
    }

    // 4. Appliquer les paramètres
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        throw std::runtime_error("Impossible d'appliquer les paramètres: " + std::string(snd_strerror(err)));
    }

    // 5. Récupérer la taille d'une période (pour allouer le buffer)
    snd_pcm_uframes_t period_size = 0;
    snd_pcm_hw_params_get_period_size(params, &period_size, nullptr);

    m_maxBufferSize = period_size * Settings::channelCount * 2; // frames * canaux * bytes par échantillon
    m_buffer.ensureAvailable(m_maxBufferSize);

    QObject::connect(m_timer, &QTimer::timeout, this, &AudioCapture::performRead);

    m_timer->setInterval(500);
    m_timer->stop();
}

AudioCapture::~AudioCapture()
{
    if (handle) {
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
    }

    if (m_instance == this) {
        m_instance = nullptr;
    }
}

void AudioCapture::stop()
{
    m_timer->stop();
}

void AudioCapture::start()
{
    m_timer->start();
}
