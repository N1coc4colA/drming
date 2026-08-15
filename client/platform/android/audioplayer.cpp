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
        qCritical() << "Erreur création builder:" << result;
        return false;
    }

    // Configuration
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16); // S16_LE
    AAudioStreamBuilder_setChannelCount(builder, Settings::channelCount);
    AAudioStreamBuilder_setSampleRate(builder, Settings::sampleRate);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, 1024); // Taille tampon

    // Ouvrir le flux
    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder); // Le builder n'est plus nécessaire

    if (result != AAUDIO_OK || stream == nullptr) {
        qCritical() << "Erreur ouverture stream:" << result;
        return false;
    }

    // Démarrer la lecture (le flux est en pause par défaut)
    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        qCritical() << "Erreur démarrage: " << result;
        return false;
    }

    running = true;
    qDebug() << "Flux AAudio ouvert et démarré.";

    return true;
}

int AudioPlayer::write(const int16_t* data, const int numFrames)
{
    if (!stream || !running) {
        return -1;
    }

    // AAudioStream_write est bloquant par défaut.
    // Le timeout est en nanosecondes. Utilisez 0 pour non-bloquant.
    const int64_t timeoutNs = 0;
    const aaudio_result_t result = AAudioStream_write(stream, data, numFrames, timeoutNs);

    if (result < 0) {
        // Gérer les erreurs (sous-écoulement, déconnexion...)
        if (result == AAUDIO_ERROR_INTERNAL) {
            // Sous-écoulement : pas assez de données fournies, on ignore
            return 0;
        } else {
            qWarning() << "Erreur écriture:" << result;
            return -1;
        }
    }

    return static_cast<int>(result); // Nombre de trames réellement écrites
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
