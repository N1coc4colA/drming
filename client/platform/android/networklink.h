#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include "../../networklink.h"

#include "multicastlock.h"

#include "../net/parser.h"
#include "../settings.h"

namespace Platform {

class VideoDecoder;
class VideoFrameItem;

class VideoLink : Packets::Parser<VideoLink, Settings::errorLimit>, public UdpStreamSocket
{
public:
    explicit VideoLink()
        : Packets::Parser<VideoLink, Settings::errorLimit>(*this)
    {
        QObject::connect(this, &UdpStreamSocket::connected, []() { qDebug() << "VideoLink connected."; });
    }

    void processPacket(const Packets::ServerImage &img);
    void processPacket(const Packets::ServerStream &img);
    void processPacket(const Packets::ClientResolution &res);
    inline void onPacketErrors() { Parser::clear(); }

    inline VideoFrameItem *item() { return m_item; }
    inline void setItem(VideoFrameItem *item, QObject *owner);

    inline void clear() { Parser::clear(); }
    void reset()
    {
        m_locked = false;
        m_waitedForResolution = false;
        clear();
    }

    inline bool waitedForResolution() const { return m_waitedForResolution; }

    void addData(QByteArray data) override
    {
        qDebug() << "Got video data";
        Parser::addData(data);
    }

    inline auto waitFor(auto v) { return Parser::waitFor(v); }

private:
    VideoDecoder *m_decoder = nullptr;
    VideoFrameItem *m_item = nullptr;
    bool m_waitedForResolution = false;
    bool m_locked = false;
};

class AudioLink : Packets::Parser<AudioLink, Settings::errorLimit>, public UdpStreamSocket
{
public:
    explicit AudioLink()
        : Packets::Parser<AudioLink, Settings::errorLimit>(*this)
    {
        QObject::connect(this, &UdpStreamSocket::connected, []() { qDebug() << "AudioLink connected."; });
    }

    inline void onPacketErrors() { Parser::clear(); }

    inline void clear() { Parser::clear(); }

    void addData(QByteArray data) override
    {
        qDebug() << "Got audio data";
        Parser::addData(data);
    }
};

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
    inline void onPacketErrors() { Parser::clear(); }

    void setItem(QObject *item) override;

private:
    MulticastLock m_lock;
    VideoLink m_vlink;
    AudioLink m_alink;
    bool m_waitedForKey = false;
    bool m_waitedForAudioSource = false;
    bool m_waitedForVideoSource = false;
    QTimer m_requireCheck;

    void performRequirements();

    inline void addData(QByteArray additional) override
    {
        qDebug() << "Got data";
        Parser::clear();
        Parser::addData(std::move(additional));
    }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
