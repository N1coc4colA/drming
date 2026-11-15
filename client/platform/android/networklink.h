#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include <QThread>
#include <atomic>

#include "../../networklink.h"

#include "imagedecoder.h"
#include "multicastlock.h"

#include "../net/parser.h"
#include "../settings.h"

namespace Platform {

class VideoDecoder;
class VideoFrameItem;

/// Receives and decodes the video stream.
class VideoLink : public UdpStreamSocket, public Packets::Parser<VideoLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit VideoLink(QObject *parent = nullptr);
    ~VideoLink() override;

    void processPacket(const Packets::ServerStream &img);
    void processPacket(const Packets::ClientResolution &res);
    /// Clears parser state after packet errors.
    inline void onPacketErrors()
    {
        Parser::clear();
    }

    /// Returns the item displaying decoded frames.
    inline VideoFrameItem *item()
    {
        return m_item;
    }
    inline const VideoFrameItem *item() const
    {
        return m_item;
    }

    /**
     * Associates the QML item that displays decoded frames.
     * [NOTE]: Calling this multiple times may trigger a bug.
     * Some changes are required before being able to re-attach.
     */
    void setItem(VideoFrameItem *item);

    /// Clears buffered parser and decoder state.
    inline void clear()
    {
        Parser::clear();
        m_imageDecoder.reset();
        clearJitterBuffer();
    }

    /// Associates telemetry used to record decoder timing.
    void setTelemetry(Telemetry *t);

Q_SIGNALS:
    void keyframeRequested();
    void activity();

public Q_SLOTS:
    /// Resets the stream and waits for a new resolution.
    void reset()
    {
        m_waitedForResolution = false;
        clearRules();
        clear();
    }

public:
    /// Reports whether the stream resolution has arrived.
    inline bool waitedForResolution() const
    {
        return m_waitedForResolution.load(std::memory_order_relaxed);
    }

    /// Waits until the requested packet type is available.
    inline auto waitFor(auto v)
    {
        return Parser::waitFor(v);
    }

protected:
    /**
     * Implementation for UdpStreamSocket.
     * Requests a keyframe after an incomplete frame.
     */
    void onFrameSkipped() override;

    /// Adds encoded bytes to the packet parser.
    void addData(const QByteArray &data) override
    {
        Parser::pushData(data);
    }

private:
    VideoDecoder *m_decoder = nullptr;
    ImageDecoder m_imageDecoder{};
    VideoFrameItem *m_item = nullptr;
    Telemetry *m_telemetry = nullptr;
    std::atomic<bool> m_waitedForResolution{ false };
};

/// Receives and parses the audio stream.
class AudioLink : public UdpStreamSocket, public Packets::Parser<AudioLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit AudioLink(QObject *parent = nullptr)
        : UdpStreamSocket(parent)
        , Packets::Parser<AudioLink, Settings::errorLimit>(*this)
    {
        QObject::connect(this, &UdpStreamSocket::connected, []() { qDebug() << "AudioLink connected."; });
    }

    /// Clears parser state after packet errors.
    inline void onPacketErrors()
    {
        Parser::clear();
    }
    /// Clears buffered parser and decoder state.
    inline void clear()
    {
        Parser::clear();
    }

protected:
    /// Adds encoded bytes to the packet parser.
    void addData(const QByteArray &data) override
    {
        Parser::pushData(data);
    }
};

/// Routes audio and video packets to their decoders.
class NetworkLink : public ::NetworkLink, Packets::Parser<NetworkLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent);
    ~NetworkLink() override;

    /// Processes the packet and updates stream state.
    inline void processPacket(const Packets::Reinit &)
    {
        if (m_alink) {
            QMetaObject::invokeMethod(m_alink, &AudioLink::clear);
        }
        if (m_vlink) {
            QMetaObject::invokeMethod(m_vlink, &VideoLink::clear);
        }

        Parser::clear();
    }

    void processPacket(const Packets::KeyUpdate &ku);
    void processPacket(const Packets::AudioSource &src);
    void processPacket(const Packets::VideoSource &src);
    /// Clears parser state after packet errors.
    inline void onPacketErrors()
    {
        Parser::clear();
    }

    /// Associates the QML item that displays decoded frames.
    void setItem(QObject *item) override;

    /// Clears stale packets before queuing new data.
    inline void pushData(const QByteArray &additional) override
    {
        Parser::clear();
        Parser::pushData(additional);
    }
protected:
    /// Closes the platform network links.
    void closePlatform() override;

private:
    MulticastLock m_lock{};
    QThread m_videoThread{};
    QThread m_audioThread{};
    VideoLink *m_vlink = nullptr;
    AudioLink *m_alink = nullptr;
    bool m_waitedForKey = false;
    bool m_waitedForAudioSource = false;
    bool m_waitedForVideoSource = false;
    QTimer m_requireCheck;

    /// Requests any stream sources that are still missing.
    void performRequirements();
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
