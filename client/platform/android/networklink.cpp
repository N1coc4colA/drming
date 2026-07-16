#include "networklink.h"

#include "ffmpeg.h"

namespace Platform {

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{}

void NetworkLink::processPacket(const Packets::ServerStream &img)
{
    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(this);
        m_decoder->setFrameCallback([this](const QImage &image) {
            if (!image.isNull()) {
                [[likely]];

                Q_EMIT imageReady(image);
            }
        });
    }

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.constData()), img.frameSize);
}

void NetworkLink::processPacket(const Packets::ServerImage &img)
{
    const auto converted = QImage::fromData(img.data, img.format);

    if (!converted.isNull()) {
        [[likely]];

        Q_EMIT imageReady(converted);
    }
}

void NetworkLink::processPacket(const Packets::ServerBrightness &brightness)
{
    Q_UNUSED(brightness);
}

} // namespace Platform
