#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include "../../../net/parser.h"
#include "../../../net/udp.h"

#include "../../imagedecoder.h"
#include "../../networklink.h"
#include "../settings.h"

#include <QThread>

namespace Platform {

class FfmpegDecoder;
class VideoFrameItem;

/// Receives and decodes the video stream.
class VideoLink : public UdpStreamSocket, public Packets::Parser<VideoLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit VideoLink(QObject *parent = nullptr)
        : UdpStreamSocket(parent)
        , Packets::Parser<VideoLink, Settings::errorLimit>(*this)
    {
        QObject::connect(this, &UdpStreamSocket::connected, []() { qDebug() << "VideoLink connected."; });
    }

    void processPacket(const Packets::ServerStream &img);
    void processPacket(const Packets::ClientResolution &res);
    /// Associates telemetry used to record decoder timing.
    void setTelemetry(Telemetry *telemetry);

Q_SIGNALS:
    void keyframeRequested();
    void activity();

protected:
    /**
     * Implementation for UdpStreamSocket.
     * Requests a keyframe after an incomplete frame.
     */
    void onFrameSkipped() override;

public:
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
    /// Associates the QML item that displays decoded frames.
    void setItem(VideoFrameItem *item, QObject *owner);

    /// Clears buffered parser and decoder state.
    inline void clear()
    {
        Parser::clear();
        m_imageDecoder.reset();
    }
    /// Resets the stream and waits for a new resolution.
    void reset()
    {
        m_waitedForResolution = false;
        clear();
    }

    /// Reports whether the stream resolution has arrived.
    inline bool waitedForResolution() const
    {
        return m_waitedForResolution;
    }

    /// Adds encoded bytes to the packet parser.
    void addData(QByteArray data) override
    {
        Parser::pushData(std::move(data));
    }

    /// Waits until the requested packet type is available.
    inline auto waitFor(auto v)
    {
        return Parser::waitFor(v);
    }

private:
    FfmpegDecoder *m_decoder = nullptr;
    ImageDecoder m_imageDecoder;
    VideoFrameItem *m_item = nullptr;
    Telemetry *m_telemetry = nullptr;
    bool m_waitedForResolution = false;
};

/// Receives and parses the audio stream.
class AudioLink : Packets::Parser<AudioLink, Settings::errorLimit>, public UdpStreamSocket
{
public:
    explicit AudioLink(QObject *parent = nullptr)
        : Packets::Parser<AudioLink, Settings::errorLimit>(*this)
        , UdpStreamSocket(parent)
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

    /// Adds encoded bytes to the packet parser.
    void addData(QByteArray data) override
    {
        Parser::pushData(std::move(data));
    }
};

/// Routes audio and video packets to their decoders.
class NetworkLink : public ::NetworkLink, Packets::Parser<NetworkLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent);

    inline void processPacket(const Packets::Reinit &)
    {
        m_alink.clear();
        m_vlink.clear();
        Parser::clear();
    }
    void processPacket(const Packets::KeyUpdate &ku);
    void processPacket(const Packets::AudioSource &src);
    void processPacket(const Packets::VideoSource &src);
    inline void onPacketErrors()
    {
        Parser::clear();
    }

    /// Associates the QML item that displays decoded frames.
    void setItem(QObject *item) override;

private:
    VideoLink m_vlink{};
    AudioLink m_alink{};
    bool m_waitedForKey = false;
    bool m_waitedForAudioSource = false;
    bool m_waitedForVideoSource = false;
    QTimer m_requireCheck{};

    /// Requests any stream sources that are still missing.
    void performRequirements();

    /// Clears stale packets before queuing new data.
    inline void pushData(QByteArray additional) override
    {
        Parser::clear();
        Parser::pushData(std::move(additional));
    }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
