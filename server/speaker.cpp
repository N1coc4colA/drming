#include "speaker.h"

#include <QDebug>

AudioCapture::AudioCapture(const std::string& device)
{
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
    // Format : 16 bits signé, stéréo, 48000 Hz
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        throw std::runtime_error("Erreur accès: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        throw std::runtime_error("Erreur format: " + std::string(snd_strerror(err)));
    }

    err = snd_pcm_hw_params_set_channels(handle, params, 2);
    if (err < 0) {
        throw std::runtime_error("Erreur canaux: " + std::string(snd_strerror(err)));
    }

    unsigned int rate = 48000;
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
    buffer_size = period_size * 2 * 2; // frames * canaux * bytes par échantillon
    buffer.resize(buffer_size);
}

AudioCapture::~AudioCapture()
{
    if (handle) {
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
    }
}
