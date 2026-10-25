#include "networklink.h"

#include "ffmpeg.h"
#include "videoframeitem.h"

namespace Platform {

void VideoLink::processPacket(const Packets::ClientResolution &res)
{
    qDebug() << "Got resolution";

    m_waitedForResolution = true;
    setBounds<&Packets::ServerImage::data>(
        quint64(0),
        static_cast<quint64>(res.width.data) * static_cast<quint64>(res.height.data) * 4
    );
}

void VideoLink::processPacket(const Packets::ServerStream &img)
{
    qDebug() << "Got stream";
    Q_EMIT activity();

    if (!m_item) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerImage);
        m_locked = true;
    }

    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(this);
        m_decoder->setFrameCallback([this](const QImage &image, const qint64 receiveNs) {
            if (!image.isNull() && m_item) {
                QMetaObject::invokeMethod(
                    m_item,
                    "setImage",
                    Qt::QueuedConnection,
                    Q_ARG(QImage, image),
                    Q_ARG(qint64, receiveNs)
                );
            }
        });
    }

    const auto receiveNs = steadyNowNs();
    if (m_telemetry) {
        m_telemetry->recordReceived(img.data.size);
    }
    m_decoder->setLastInputNs(receiveNs);
    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.data.constData()), img.data.size);
}

void VideoLink::processPacket(const Packets::ServerImage &img)
{
    Q_EMIT activity();

    if (!m_item) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerStream);
        m_locked = true;
    }

    const auto receiveNs = steadyNowNs();
    if (m_telemetry) {
        m_telemetry->recordReceived(img.data.data.size());
    }
    m_imageDecoder.queueFrame(img.data.data, img.format.data, receiveNs);
}

void VideoLink::setItem(VideoFrameItem *item, QObject *owner)
{
    m_item = item;

    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(owner);
        m_decoder->setFrameCallback([this](const QImage &image, const qint64 receiveNs) {
            if (!image.isNull() && m_item) {
                QMetaObject::invokeMethod(
                    m_item,
                    "setImage",
                    Qt::QueuedConnection,
                    Q_ARG(QImage, image),
                    Q_ARG(qint64, receiveNs)
                );
            }
        });
    }

    m_imageDecoder.setFrameCallback([this](const QImage &image, const qint64 receiveNs) {
        if (m_item) {
            QMetaObject::invokeMethod(
                m_item,
                "setImage",
                Qt::QueuedConnection,
                Q_ARG(QImage, image),
                Q_ARG(qint64, receiveNs)
            );
        }
    });

    m_decoder->setTelemetry(m_telemetry);
    m_decoder->setLatencyCallback([this](const qint64 ns) {
        if (m_telemetry) {
            m_telemetry->recordDecoded(ns);
        }
    });

    item->setLatencyCallback([this](const qint64 ns) {
        if (m_telemetry) {
            m_telemetry->recordDecoded(ns);
        }
    });
}

void VideoLink::setTelemetry(Telemetry *telemetry)
{
    m_telemetry = telemetry;
    m_imageDecoder.setTelemetry(telemetry);
    if (m_decoder) {
        m_decoder->setTelemetry(telemetry);
        m_decoder->setLatencyCallback([telemetry](const qint64 ns) {
            if (telemetry) {
                telemetry->recordDecoded(ns);
            }
        });
    }
}

void VideoLink::onFrameSkipped()
{
    qWarning() << "Jitter buffer skipped an incomplete frame; requesting keyframe";
    Q_EMIT keyframeRequested();
}

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{
    m_vlink.setTelemetry(&m_telemetry);
    QObject::connect(&m_vlink, &VideoLink::activity, this, &NetworkLink::markActivity, Qt::QueuedConnection);
    QObject::connect(&m_vlink, &VideoLink::keyframeRequested, this, [this]() {
        static constexpr Packets::RequestKeyframe pkt{};
        write(Packets::Writer::generate(pkt));
    });
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
    qDebug() << "Got audio source:" << QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())) << ':'
             << src.port.data;

    const auto addr = src.isV4.data ? QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData()))
                                    : QHostAddress(*reinterpret_cast<const quint8 *>(src.ip.data.constData()));

    const auto sock = m_alink.socket();
    if (sock ? (sock->peerPort() != src.port.data || sock->peerAddress() != addr) : true) {
        m_waitedForAudioSource = true;

        m_alink.close();
        m_alink.connectToMulticast(addr, src.port.data);
    }
}

void NetworkLink::processPacket(const Packets::VideoSource &src)
{
    qDebug() << "Got video source:" << QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())) << ':'
             << src.port.data;

    const auto addr = src.isV4.data ? QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData()))
                                    : QHostAddress(*reinterpret_cast<const quint8 *>(src.ip.data.constData()));

    const auto sock = m_vlink.socket();
    if (sock ? (sock->peerPort() != src.port.data || sock->peerAddress() != addr) : true) {
        m_waitedForVideoSource = true;

        m_vlink.close();
        m_vlink.connectToMulticast(addr, src.port.data);
    }
}

} // namespace Platform
