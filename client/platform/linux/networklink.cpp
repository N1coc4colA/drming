#include "networklink.h"

#include "ffmpeg.h"
#include "videoframeitem.h"

namespace Platform {

void VideoLink::processPacket(const Packets::ClientResolution &res)
{
    qDebug() << "Got resolution";

    m_waitedForResolution = true;
    setBounds<&Packets::ServerImage::data>(quint64(0), static_cast<quint64>(res.width.data) * static_cast<quint64>(res.height.data));
}

void VideoLink::processPacket(const Packets::ServerStream &img)
{
    qDebug() << "Got stream";

    if (!m_item) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerImage);
        m_locked = true;
    }

    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(this);
        m_decoder->setFrameCallback([this](const QImage &image) {
            if (!image.isNull()) {
                m_item->setImage(image);
            }
        });
    }

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.data.constData()), img.data.size);
}

void VideoLink::processPacket(const Packets::ServerImage &img)
{
    qDebug() << "Got image";

    if (!m_item) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerStream);
        m_locked = true;
    }

    const QImage converted = QImage::fromData(img.data.data, img.format.data);
    if (!converted.isNull()) [[likely]] {
        m_item->setImage(converted);
    }
}

void VideoLink::setItem(VideoFrameItem *item, QObject *owner)
{
    m_item = item;

    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(owner);
        m_decoder->setFrameCallback([this](const QImage &image) {
            if (!image.isNull() && m_item) {
                m_item->setImage(image);
            }
        });
    }
}

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{
    QObject::connect(this, &NetworkLink::connectionInitialised, this, [this]() {
        m_vlink.reset();
        m_waitedForKey = false;
        Parser::clearRules();
    });
    QObject::connect(this, &NetworkLink::connectionReady, this, [this]() { m_requireCheck.start(); });
    QObject::connect(this, &NetworkLink::closed, this, [this]() {
        m_vlink.reset();
        m_waitedForKey = false;
        Parser::clearRules();
    });
    QObject::connect(&m_requireCheck, &QTimer::timeout, this, &NetworkLink::performRequirements);
    QObject::connect(this, &NetworkLink::closed, &m_requireCheck, &QTimer::stop);

    m_requireCheck.setInterval(500);
    m_requireCheck.stop();
}

void NetworkLink::performRequirements()
{
    const auto ok = m_vlink.waitedForResolution() && m_waitedForKey;
    if (ok) {
        m_requireCheck.stop();
        return;
    }

    if (!m_vlink.waitedForResolution()) [[unlikely]] {
        m_vlink.waitFor(Packets::Type::ClientResolution);

        static constexpr Packets::RequestClientResolution resReq{};
        write(Packets::Writer::generate(resReq));
    }

    if (!m_waitedForKey) [[unlikely]] {
        waitFor(Packets::Type::KeyUpdate);

        static constexpr Packets::RequestKey keyReq{};
        write(Packets::Writer::generate(keyReq));
    }

    if (!m_waitedForAudioSource) [[unlikely]] {
        waitFor(Packets::Type::AudioSource);

        static constexpr Packets::RequestAudioSource audReq{};
        write(Packets::Writer::generate(audReq));
    }

    if (!m_waitedForVideoSource) [[unlikely]] {
        waitFor(Packets::Type::VideoSource);

        static constexpr Packets::RequestVideoSource vidReq{};
        write(Packets::Writer::generate(vidReq));
    }
}

void NetworkLink::setItem(QObject *item)
{
    auto vitem = qobject_cast<VideoFrameItem *>(item);
    if (!vitem) {
        qWarning() << "NetworkLink::setItem expects a VideoFrameItem object.";
        return;
    }

    m_vlink.setItem(vitem, this);
}

void NetworkLink::processPacket(const Packets::KeyUpdate &ku)
{
    qDebug() << "Got key" << ku.keyStamp.data;
    m_waitedForKey = true;
    m_vlink.setKey(ku.key.data, ku.keyStamp.data);
    m_alink.setKey(ku.key.data, ku.keyStamp.data);
}

void NetworkLink::processPacket(const Packets::AudioSource &src)
{
    qDebug() << "Got audio source:" << QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())) << ':' << src.port.data;
    m_waitedForAudioSource = true;

    m_alink.close();
    if (src.isV4.data) {
        m_alink.connectToMulticast(QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())), src.port.data);
    } else {
        m_alink.connectToMulticast(QHostAddress(*reinterpret_cast<const quint8 *>(src.ip.data.constData())), src.port.data);
    }
}

void NetworkLink::processPacket(const Packets::VideoSource &src)
{
    qDebug() << "Got video source:" << QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())) << ':' << src.port.data;
    m_waitedForVideoSource = true;

    m_vlink.close();
    if (src.isV4.data) {
        m_vlink.connectToMulticast(QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())), src.port.data);
    } else {
        m_vlink.connectToMulticast(QHostAddress(*reinterpret_cast<const quint8 *>(src.ip.data.constData())), src.port.data);
    }
}

} // namespace Platform
