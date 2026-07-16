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
    void processPacket(const Packets::ClientResolution &res) { Q_UNUSED(res); }
    void processPacket(const Packets::ServerBrightness &brightness);
    void processPacket(const Packets::ServerStream &img);

    void setItem(QObject *item) override;

private:
    FfmpegDecoder *m_decoder = nullptr;
    VideoFrameItem *m_item = nullptr;

    inline void addData(QByteArray additional) override { Packets::Parser<NetworkLink>::addData(std::move(additional)); }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
