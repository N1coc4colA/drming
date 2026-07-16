#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include "../../../parser.h"
#include "../../networklink.h"

namespace Platform {

class FfmpegDecoder;

class NetworkLink : public ::NetworkLink, Packets::Parser<NetworkLink>
{
public:
    explicit NetworkLink(QObject *parent);

    void processPacket(const Packets::ServerImage &img);
    void processPacket(const Packets::ClientResolution &res) { Q_UNUSED(res); }
    void processPacket(const Packets::ServerBrightness &brightness);
    void processPacket(const Packets::ServerStream &img);

private:
    FfmpegDecoder *m_decoder = nullptr;

    inline void addData(QByteArray additional) override { Packets::Parser<NetworkLink>::addData(std::move(additional)); }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
