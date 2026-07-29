#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include "../../../parser.h"
#include "../../networklink.h"

namespace Platform {

class FfmpegDecoder;
class VideoFrameItem;

class NetworkLink : public ::NetworkLink, Packets::Parser<NetworkLink>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent);

    void processPacket(const Packets::ServerImage &img);
    inline void processPacket(const Packets::ClientResolution &) {}
    inline void processPacket(const Packets::ServerBrightness &) {};
    void processPacket(const Packets::ServerStream &img);
    inline void processPacket(const Packets::HeartBeat &) {};
    void processPacket(const Packets::Reinit &);
    void onPacketErrors();

    void setItem(QObject *item) override;

private:
    FfmpegDecoder *m_decoder = nullptr;
    VideoFrameItem *m_item = nullptr;

    inline void addData(QByteArray additional) override { Packets::Parser<NetworkLink>::addData(std::move(additional)); }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
