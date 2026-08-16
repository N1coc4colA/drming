#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <aaudio/AAudio.h>

#include <atomic>
#include <vector>

#include "../settings.h"

namespace Platform {
class AudioPlayer
{
public:
    AudioPlayer();

    inline ~AudioPlayer() { close(); }

    bool open();
    void close();

    /** @brief Écrire des données audio (S16_LE)
    Retourne le nombre de trames écrites, ou -1 en cas d'erreur
    **/
    int write(const Settings::audioFormat* left, const Settings::audioFormat* rigth, int numFrames);

    // Obtenir la taille de la "burst" (tampon recommandé par le système)
    int32_t getBurstSize() const
    {
        if (stream) {
            return AAudioStream_getFramesPerBurst(stream);
        }

        return 1024; // fallback
    }

private:
    AAudioStream* stream;
    std::vector<Settings::audioFormat> m_interleaved;
    std::atomic<bool> running;
};
} // namespace Platform

#endif // AUDIOPLAYER_H
