#include "networkclient.h"

#include <QDebug>
#include <QDtls>
#include <QSslConfiguration>
#include <QTimer>
#include <qendian.h>

#include "display.h"

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
    m_display->requireKeyInformation();
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
