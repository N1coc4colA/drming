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

class VideoLink : public UdpStreamSocket, public Packets::Parser<VideoLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit VideoLink(QObject *parent = nullptr);

    void processPacket(const Packets::ServerImage &img);
    void processPacket(const Packets::ServerStream &img);
    void processPacket(const Packets::ClientResolution &res);
    inline void onPacketErrors()
    {
        Parser::clear();
    }

    inline VideoFrameItem *item()
    {
        return m_item;
    }
    void setItem(VideoFrameItem *item);

    inline void clear()
    {
        Parser::clear();
        m_imageDecoder.reset();
        clearJitterBuffer();
    }

    void setTelemetry(Telemetry *t);

Q_SIGNALS:
    void keyframeRequested();
    void activity();

public Q_SLOTS:
    void reset()
    {
        m_locked = false;
        m_waitedForResolution = false;
        clear();
    }

public:
    inline bool waitedForResolution() const
    {
        return m_waitedForResolution.load(std::memory_order_relaxed);
    }

    void addData(QByteArray data) override
    {
        Parser::pushData(std::move(data));
    }

    inline auto waitFor(auto v)
    {
        return Parser::waitFor(v);
    }

protected:
    void onFrameSkipped() override;

private:
    VideoDecoder *m_decoder = nullptr;
    ImageDecoder m_imageDecoder;
    VideoFrameItem *m_item = nullptr;
    Telemetry *m_telemetry = nullptr;
    std::atomic<bool> m_waitedForResolution{ false };
    bool m_locked = false;
};

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

    inline void onPacketErrors()
    {
        Parser::clear();
    }
    inline void clear()
    {
        Parser::clear();
    }

    void addData(QByteArray data) override
    {
        Parser::pushData(std::move(data));
    }
};

class NetworkLink : public ::NetworkLink, Packets::Parser<NetworkLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent);
    ~NetworkLink() override;

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
    inline void onPacketErrors()
    {
        Parser::clear();
    }

    void setItem(QObject *item) override;

private:
    MulticastLock m_lock;
    QThread m_videoThread;
    QThread m_audioThread;
    VideoLink *m_vlink = nullptr;
    AudioLink *m_alink = nullptr;
    bool m_waitedForKey = false;
    bool m_waitedForAudioSource = false;
    bool m_waitedForVideoSource = false;
    QTimer m_requireCheck;

    void performRequirements();

    inline void pushData(QByteArray additional) override
    {
        Parser::clear();
        Parser::pushData(std::move(additional));
    }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
