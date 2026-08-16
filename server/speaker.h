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

    inline void* leftData() { return m_buffers[0]; }
    inline void* rightData() { return m_buffers[1]; }
    inline void** data() { return m_buffers; }

    inline std::size_t size() const { return m_size; }
    inline const void* getLeft() const { return m_buffers[0]; }
    inline const void* getRight() const { return m_buffers[1]; }
    inline std::size_t frameCount() const { return m_frames; }

private:
    mutable QReadWriteLock m_lock;
    void* m_buffers[2] = {nullptr};
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
    explicit AudioCapture(const std::string& device = "hw:Loopback,1,0", QObject* parent = nullptr);
    ~AudioCapture();

    static AudioCapture* instance() { return m_instance; }

    // Lire une période de données audio
    bool readFrame();

    inline void performRead() { Q_UNUSED(readFrame()); }

    // Accès aux données audio (format S16_LE, entrelacé)
    inline const AudioBuffer& getBuffer() const { return m_buffer; }

    void stop();
    void start();

private:
    snd_pcm_t* handle = nullptr;
    snd_pcm_hw_params_t* params = nullptr;
    AudioBuffer m_buffer;
    snd_pcm_uframes_t m_periodSize = 1024;     // Arbitrary default because it's this one on my computer.
    snd_pcm_uframes_t m_maxBufferSize = 32768; // Arbitrary default because it's this one on my computer.
    std::size_t m_bufferByteSize = 0;
    QTimer* m_timer;

    static AudioCapture* m_instance;
};

#endif // SPEAKER_H
