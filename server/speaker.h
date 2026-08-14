#ifndef SPEAKER_H
#define SPEAKER_H

#include <QObject>
#include <QThread>
#include <QtDebug>

#include <alsa/asoundlib.h>

#include <cstring>
#include <vector>

class AudioCapture
{
public:
    explicit AudioCapture(const std::string& device = "hw:Loopback,0,0");
    ~AudioCapture();

    // Lire une période de données audio
    inline bool readFrame()
    {
        const int err = snd_pcm_readi(handle, buffer.data(),
                                      buffer_size / (2 * 2)); // frames
        if (err == -EPIPE) {
            // Sous-écoulement (overrun) : on réinitialise
            qWarning() << "Overrun, réinitialisation...";
            snd_pcm_prepare(handle);
            return false;
        } else if (err < 0) {
            qWarning() << "Erreur de lecture: " << snd_strerror(err);
            return false;
        } else if (err == 0) {
            // Pas de données disponibles
            return false;
        }

        return true;
    }

    // Accès aux données audio (format S16_LE, entrelacé)
    inline const int16_t* getAudioData() const { return reinterpret_cast<const int16_t*>(buffer.data()); }
    inline size_t getBufferSize() const { return buffer_size; }

private:
    snd_pcm_t* handle = nullptr;
    snd_pcm_hw_params_t* params = nullptr;
    std::vector<uint8_t> buffer{};
    size_t buffer_size = 0;
};

#endif // SPEAKER_H
