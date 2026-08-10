#ifndef NETWORKLINKPLATFORM_H
#define NETWORKLINKPLATFORM_H

#include "../../../parser.h"
#include "../../networklink.h"

#include "../settings.h"

namespace Platform {

class FfmpegDecoder;
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
    void onPacketErrors();

    void setItem(QObject *item) override;

private:
    FfmpegDecoder *m_decoder = nullptr;
    VideoFrameItem *m_item = nullptr;
    bool m_waitedForResolution = false;
    bool m_locked = false;

    inline void addData(QByteArray additional) override
    {
        if (!m_waitedForResolution) [[unlikely]] {
            m_waitedForResolution = true;
            waitFor(Packets::Type::ClientResolution);

            static constexpr Packets::RequestClientResolution resReq{};
            write(Packets::Writer::generate(resReq));
        }

        Packets::Parser<NetworkLink, Settings::errorLimit>::addData(std::move(additional));
    }
};

} // namespace Platform

#endif // NETWORKLINKPLATFORM_H
