#ifndef SPEAKER_H
#define SPEAKER_H

#include <QObject>
#include <QReadWriteLock>
#include <QThread>
#include <QtDebug>

#include <alsa/asoundlib.h>

#include <cstring>
#include <vector>

#include "../settings.h"

class QTimer;
class AudioBuffer;

class AudioBufferLock
{
public:
    explicit AudioBufferLock(const AudioBuffer& buffer);

private:
    QReadLocker m_lock;
};

class AudioBufferWriteLock
{
public:
    explicit AudioBufferWriteLock(AudioBuffer& buffer);

private:
    QWriteLocker m_lock;
};

class AudioBuffer
{
public:
    inline void setFrames(const std::size_t frames) { m_frames = frames; }
    inline void setSize(const std::size_t newSize) { m_size = newSize; }
    inline void ensureAvailable(const std::size_t size)
    {
        if (m_buffer.size() < size) {
            m_buffer.resize(size);
        }
    }
    inline void setData(const std::uint8_t* newData, const std::size_t newSize)
    {
        std::memcpy(m_buffer.data(), newData, newSize);
        m_size = newSize;
    }

    inline uint8_t* data() { return m_buffer.data(); }

    inline std::size_t size() const { return m_size; }
    inline const uint8_t* getData() const { return m_buffer.data(); }
    inline std::size_t frameCount() const { return m_frames; }

private:
    mutable QReadWriteLock m_lock;
    std::vector<uint8_t> m_buffer{};
    std::size_t m_size = 0;
    std::size_t m_frames = 0;

    friend class AudioBufferLock;
    friend class AudioBufferWriteLock;
};

inline AudioBufferLock::AudioBufferLock(const AudioBuffer& buffer)
    : m_lock(&buffer.m_lock)
{}

inline AudioBufferWriteLock::AudioBufferWriteLock(AudioBuffer& buffer)
    : m_lock(&buffer.m_lock)
{}

class AudioCapture : public QObject
{
    Q_OBJECT

public:
    explicit AudioCapture(const std::string& device = "hw:Loopback,0,0", QObject* parent = nullptr);
    ~AudioCapture();

    static AudioCapture* instance() { return m_instance; }

    // Lire une période de données audio
    inline bool readFrame()
    {
        const AudioBufferWriteLock lock(this->m_buffer);

        const auto frames = snd_pcm_readi(handle, m_buffer.data(),
                                          m_maxBufferSize / (2 * Settings::channelCount)); // frames
        if (frames == -EPIPE) {
            // Sous-écoulement (overrun) : on réinitialise
            qWarning() << "Overrun, réinitialisation...";
            snd_pcm_prepare(handle);
            return false;
        } else if (frames < 0) {
            qWarning() << "Erreur de lecture: " << snd_strerror(frames);
            return false;
        } else if (frames == 0) {
            // Pas de données disponibles
            return false;
        }

        m_buffer.setSize(frames * 2 * Settings::channelCount);
        m_buffer.setFrames(frames);

        return true;
    }

    inline void performRead() { Q_UNUSED(readFrame()); }

    // Accès aux données audio (format S16_LE, entrelacé)
    inline const AudioBuffer& getBuffer() const { return m_buffer; }

    void stop();
    void start();

private:
    snd_pcm_t* handle = nullptr;
    snd_pcm_hw_params_t* params = nullptr;
    AudioBuffer m_buffer;
    std::size_t m_maxBufferSize;
    QTimer* m_timer;

    static AudioCapture* m_instance;
};

#endif // SPEAKER_H
