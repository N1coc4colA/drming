#include "networkclient.h"

#include <QDebug>
#include <QDtls>
#include <QSslConfiguration>
#include <QTimer>
#include <qendian.h>

#include "display.h"
#include "parameters.h"
#include "streamsocket.h"

void NetworkClient::processPacket(const Packets::HeartBeat &)
{
    notifyHeartBeat();
}

void NetworkClient::processPacket(const Packets::Reinit &)
{
    if (m_display) [[likely]] {
        m_display->reinit();
    }
}

void NetworkClient::processPacket(const Packets::RequestClientResolution &)
{
    if (m_display) [[likely]] {
        m_display->requireResolutionInformation();
    }
}

void NetworkClient::processPacket(const Packets::RequestKey &)
{
    notifyKey();
}

void NetworkClient::processPacket(const Packets::RequestAudioSource &)
{
    Packets::AudioSource src{};

    const auto ip = Parameters::instance.serviceAudioIp;
    const auto v4 = ip.toIPv4Address();
    const auto v6 = ip.toIPv6Address();

    const auto isV4 = ip.protocol() == QAbstractSocket::IPv4Protocol;
    const auto size = isV4 ? 4 : 16;

    src.isV4.data = isV4;
    src.ip.data.resize(size);
    std::memcpy(src.ip.data.data(), isV4 ? static_cast<const void *>(&v4) : static_cast<const void *>(&v6), size);
    src.port.data = Parameters::instance.audioPort;

    write(std::move(Packets::Writer::generate(src)));
}

void NetworkClient::processPacket(const Packets::RequestVideoSource &)
{
    Packets::VideoSource src{};

    const auto ip = Parameters::instance.serviceVideoIp;
    const auto v4 = ip.toIPv4Address();
    const auto v6 = ip.toIPv6Address();

    const auto isV4 = ip.protocol() == QAbstractSocket::IPv4Protocol;
    const auto size = isV4 ? 4 : 16;

    src.isV4.data = isV4;
    src.ip.data.resize(size);
    std::memcpy(src.ip.data.data(), isV4 ? static_cast<const void *>(&v4) : static_cast<const void *>(&v6), size);
    src.port.data = Parameters::instance.videoPort;

    write(std::move(Packets::Writer::generate(src)));
}

void NetworkClient::processPacket(const Packets::ClientTelemetry &t)
{
    // Telemetry is advisory; we don't need to keep the whole packet, just
    // pass it to the display currently owning this client.

#ifdef TELEMETRY_OUTPUT
    qDebug() << "Telemetry from" << m_address << ":" << m_port
             << "fps=" << (t.fpsX10.data / 10.0)
             << "avg=" << t.latencyMsAvg.data << "ms"
             << "p95=" << t.latencyMsP95.data << "ms"
             << "drops=" << t.droppedFrames.data
             << "kbps=" << t.recvRateKbps.data
             << "dQ=" << t.decodeQueueDepth.data
             << "rQ=" << t.renderQueueDepth.data;
#endif

    if (m_display) {
        m_display->applyClientTelemetry(t);
    }
}

NetworkClient::NetworkClient(QDtls *dtls, const QHostAddress &address, const quint16 port, QPair<QUdpSocket, QMutex> &sharedSocket, QObject *parent)
    : QObject(parent)
    , Packets::Parser<NetworkClient>(*this)
    , m_address(address)
    , m_port(port)
    , m_rw(*this, dtls, &sharedSocket.first, &sharedSocket.second)
    , m_timer(new QTimer(this))
{
    assert(dtls);

    dtls->setParent(this); // take ownership

    connect(m_timer, &QTimer::timeout, this, &NetworkClient::checkHeartBeat);

    m_timer->setTimerType(Qt::VeryCoarseTimer);
    m_timer->setInterval(Settings::inactivityTimeout);
}

NetworkClient::~NetworkClient()
{
    // dtls deleted automatically via parent
}

void NetworkClient::incomingEncryptedData(const QByteArray &encrypted)
{
    m_rw.processDtls(encrypted);
}

qint64 NetworkClient::write(const QByteArray &data)
{
    return m_rw.writeDtls(data);
}

void NetworkClient::close()
{
    // Notify server that we are closing; the server will remove us.
    Q_EMIT disconnected();
}

QAbstractSocket::SocketState NetworkClient::state() const
{
    return m_rw.dtls()->isConnectionEncrypted() ? QAbstractSocket::ConnectedState : QAbstractSocket::ConnectingState;
}

QByteArray NetworkClient::digest() const
{
    return m_rw.dtls()->dtlsConfiguration().peerCertificate().digest();
}

void NetworkClient::notifyHeartBeat()
{
    m_timer->start();
    m_lastHeartBeat = std::chrono::system_clock::now();
}

void NetworkClient::checkHeartBeat()
{
    if ((std::chrono::system_clock::now() - m_lastHeartBeat) > systemTimeout) {
        m_timer->stop();
        // Heartbeat timeout – close connection
        close();
    }
}

void NetworkClient::notifyKey()
{
    auto &ss = m_display->streamSocket();

    Packets::KeyUpdate ku{};
    ku.key.data = ss.getKey();
    ku.keyStamp.data = ss.getKeystamp();

    qDebug() << "Sending key" << ku.keyStamp.data << ku.key.data;

    write(std::move(Packets::Writer::generate(ku)));
}

void NetworkClient::setDisplay(Display *disp)
{
    if (m_display) {
        disconnect(&m_display->streamSocket(), &StreamSocket::keyChanged, this, &NetworkClient::notifyKey);
    }

    m_display = disp;
    connect(&m_display->streamSocket(), &StreamSocket::keyChanged, this, &NetworkClient::notifyKey);
}
