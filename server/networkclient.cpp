#include "networkclient.h"

#include <QDtls>
#include <QSslConfiguration>
#include <QTimer>
#include <QDebug>

#include "display.h"

void NetworkClient::processPacket(const Packets::HeartBeat &)
{
    notifyHeartBeat();
}

void NetworkClient::processPacket(const Packets::Reinit &)
{
    m_display->reinit();
}

NetworkClientDtls::NetworkClientDtls(QDtls *dtls, const QHostAddress &address, quint16 port,
                                     QUdpSocket *sharedSocket, QObject *parent)
    : NetworkClient(parent)
    , m_dtls(dtls)
    , m_address(address)
    , m_port(port)
    , m_sharedSocket(sharedSocket)
    , m_timer(new QTimer(this))
{
    m_dtls->setParent(this); // take ownership

    m_timer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &NetworkClientDtls::checkHeartBeat);
    m_timer->setInterval(Settings::inactivityTimeout);
}

NetworkClientDtls::~NetworkClientDtls()
{
    // dtls deleted automatically via parent
}

void NetworkClientDtls::incomingEncryptedData(const QByteArray &encrypted)
{
    // Decrypt using the peer address/port – QDtls::decryptDatagram overload
    const auto plaintext = m_dtls->decryptDatagram(m_sharedSocket, encrypted);
    if (plaintext.isEmpty()) {
        if (m_dtls->dtlsError() != QDtlsError::NoError) {
            qWarning() << "DTLS decrypt error:" << m_dtls->dtlsErrorString();
            // If fatal, close connection
            if (m_dtls->dtlsError() == QDtlsError::RemoteClosedConnectionError) {
                close();
            }
        }

        return;
    }

    addData(plaintext);
}

qint64 NetworkClientDtls::write(const QByteArray &data)
{
    if (!m_dtls || !m_sharedSocket) {
        return -1;
    }

    qsizetype offset = 0;
    while (offset < data.size()) {
        const auto chunk = data.mid(offset, Settings::dtlsChunkSize);
        if (m_dtls->writeDatagramEncrypted(m_sharedSocket, chunk) < 0) {
            const auto err = m_dtls->dtlsError();
            if (err == QDtlsError::RemoteClosedConnectionError) {
                close();
            } else if (err != QDtlsError::NoError && err != QDtlsError::UnderlyingSocketError) {
                qWarning() << "DTLS write error:" << m_dtls->dtlsErrorString();
            }

            return -1;
        }
        offset += chunk.size();
    }
    return offset;
}

void NetworkClientDtls::close()
{
    // Notify server that we are closing; the server will remove us.
    Q_EMIT disconnected();
}

QAbstractSocket::SocketState NetworkClientDtls::state() const
{
    if (!m_dtls)
        return QAbstractSocket::UnconnectedState;
    return m_dtls->isConnectionEncrypted() ? QAbstractSocket::ConnectedState : QAbstractSocket::ConnectingState;
}

QByteArray NetworkClientDtls::digest() const
{
    if (!m_dtls)
        return {};
    return m_dtls->dtlsConfiguration().peerCertificate().digest();
}

void NetworkClientDtls::notifyHeartBeat()
{
    m_timer->start();
    m_lastHeartBeat = std::chrono::system_clock::now();
}

void NetworkClientDtls::checkHeartBeat()
{
    if ((std::chrono::system_clock::now() - m_lastHeartBeat) > systemTimeout) {
        m_timer->stop();
        // Heartbeat timeout – close connection
        close();
    }
}

NetworkClientSsl::NetworkClientSsl(QSslSocket *socket, QObject *parent)
    : NetworkClient(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);
    connect(m_socket, &QSslSocket::readyRead, this, [this]() {
        addData(m_socket->readAll());
    });
    connect(m_socket, &QSslSocket::disconnected, this, &NetworkClientSsl::disconnected);
}

qint64 NetworkClientSsl::write(const QByteArray &data)
{
    if (!m_socket) {
        return -1;
    }

    const auto ret = m_socket->write(data);
    m_socket->flush();
    return ret;
}

QByteArray NetworkClientSsl::digest() const
{
    if (!m_socket) {
        return {};
    }

    return m_socket->peerCertificate().digest();
}
