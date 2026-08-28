#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include "../../networklink.h"
#include "../net/parser.h"

#include "../settings.h"

namespace Platform {

class VideoDecoder;
class VideoFrameItem;

class NetworkLink : public ::NetworkLink, Packets::Parser<NetworkLink, Settings::errorLimit>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent);

    void processPacket(const Packets::ServerImage &img);
    void processPacket(const Packets::ServerStream &img);
    void processPacket(const Packets::Reinit &);
    void processPacket(const Packets::ClientResolution &res);
    void processPacket(const Packets::KeyUpdate &ku);
    void onPacketErrors();

    void setItem(QObject *item) override;

private:
    VideoDecoder *m_decoder = nullptr;
    VideoFrameItem *m_item = nullptr;
    bool m_waitedForResolution = false;
    bool m_waitedForKey = false;
    bool m_locked = false;
    QTimer m_requireCheck;

    void performRequirements();

    inline void addData(QByteArray additional) override
    {
        Parser::clear();
        Parser::addData(std::move(additional));
    }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
