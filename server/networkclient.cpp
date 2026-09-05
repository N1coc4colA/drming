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
    m_display->reinit();
}

void NetworkClient::processPacket(const Packets::RequestClientResolution &)
{
    m_display->requireResolutionInformation();
}

void NetworkClient::processPacket(const Packets::RequestKey &)
{
    auto &ss = m_display->streamSocket();

    Packets::KeyUpdate ku{};
    ku.key.data = ss.getKey();
    ku.keyStamp.data = ss.getKeystamp();

    write(std::move(Packets::Writer::generate(ku)));
}

void NetworkClient::processPacket(const Packets::RequestAudioSource &)
{
    Packets::AudioSource src{};

    const auto v4 = Parameters::instance.serviceAudioIp4.toIPv4Address();
    const auto v6 = Parameters::instance.serviceAudioIp6.toIPv6Address();

    const auto isV4 = m_address.protocol() == QAbstractSocket::IPv4Protocol;
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

    const auto v4 = Parameters::instance.serviceVideoIp4.toIPv4Address();
    const auto v6 = Parameters::instance.serviceVideoIp6.toIPv6Address();

    const auto isV4 = m_address.protocol() == QAbstractSocket::IPv4Protocol;
    const auto size = isV4 ? 4 : 16;

    src.isV4.data = isV4;
    src.ip.data.resize(size);
    std::memcpy(src.ip.data.data(), isV4 ? static_cast<const void *>(&v4) : static_cast<const void *>(&v6), size);
    src.port.data = Parameters::instance.videoPort;

    write(std::move(Packets::Writer::generate(src)));
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

    m_timer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &NetworkClient::checkHeartBeat);
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
