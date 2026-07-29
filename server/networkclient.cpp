#include "networkclient.h"

#include <QDtls>
#include <QSslConfiguration>
#include <QTimer>

#include "display.h"

NetworkClient::NetworkClient(QAbstractSocket *socket, QObject *parent)
    : QObject(parent)
    , Packets::Parser<NetworkClient>(*this)
    , m_socket(socket)
{
    m_socket->setParent(this);

    connect(m_socket, &QAbstractSocket::disconnected, this, &NetworkClient::disconnected);
}

void NetworkClient::readData()
{
    qDebug() << "Reading";
    addData(m_socket->readAll());
}

void NetworkClient::processPacket(const Packets::HeartBeat &)
{
    notifyHeartBeat();
}

void NetworkClient::processPacket(const Packets::Reinit &)
{
    m_display->reinit();
}

NetworkClientDtls::NetworkClientDtls(QDtls *dtls, QUdpSocket *socket, const QHostAddress &address, quint16 port, QObject *parent)
    : NetworkClient(socket, parent)
    , m_address(address)
    , m_port(port)
    , m_dtls(dtls)
    , m_timer(new QTimer(this))
{
    connect(m_socket, &QAbstractSocket::readyRead, this, &NetworkClientDtls::readData);

    m_timer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &NetworkClientDtls::checkHeartBeat);
    m_timer->setInterval(Settings::inactivityTimeout);
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
        m_socket->close();
    }
}

void NetworkClientDtls::readData()
{
    qDebug() << "Reading";

    auto socket = qobject_cast<QUdpSocket *>(m_socket);

    // Read all pending datagrams
    while (socket->hasPendingDatagrams()) {
        QByteArray encrypted = socket->readAll(); // or readDatagram
        // QDtls::decryptDatagram expects a QByteArrayView; it returns the plaintext if successful.
        const auto plaintext = m_dtls->decryptDatagram(socket, encrypted);
        if (plaintext.isEmpty()) {
            // DTLS error or incomplete – maybe handshake or reorder; log if needed.
            // If the error is fatal, you may want to close the connection.
            if (m_dtls->dtlsError() != QDtlsError::NoError) {
                qWarning() << "DTLS decrypt error:" << m_dtls->dtlsErrorString();
                // Optionally close socket on persistent errors
            }

            continue;
        }

        // Pass decrypted plaintext to the parser
        addData(plaintext);
    }
}

NetworkClientSsl::NetworkClientSsl(QSslSocket *socket, QObject *parent)
    : NetworkClient(socket, parent)
{
    connect(m_socket, &QAbstractSocket::readyRead, this, &NetworkClientSsl::readData);
}

qint64 NetworkClientDtls::write(const QByteArray &data)
{
    auto socket = qobject_cast<QUdpSocket *>(m_socket);
    if (!m_dtls || !socket) [[unlikely]] {
        return -1;
    }

    qsizetype offset = 0;
    while (offset < data.size()) {
        const auto chunk = data.mid(offset, Settings::dtlsChunkSize);

        if (m_dtls->writeDatagramEncrypted(socket, chunk) < 0) [[unlikely]] {
            const auto err = m_dtls->dtlsError();
            if (err == QDtlsError::RemoteClosedConnectionError) {
                m_socket->close();
            } else if (err != QDtlsError::NoError && err != QDtlsError::UnderlyingSocketError) {
                qWarning() << "DTLS Error" << static_cast<int>(err) << ":" << m_dtls->dtlsErrorString();
            }

            return -1;
        }

        offset += chunk.size();
    }

    return offset;
}

qint64 NetworkClientSsl::write(const QByteArray &data)
{
    if (!m_socket) [[unlikely]] {
        return -1;
    }

    // [TODO] Handle this stuff more gracefully. Would need a loop to ensure everything's been written 'til the end.
    const auto ret = m_socket->write(data);

    m_socket->waitForBytesWritten();

    return ret;
}

QByteArray NetworkClientDtls::digest() const
{
    return m_dtls->dtlsConfiguration().peerCertificate().digest();
}

QByteArray NetworkClientSsl::digest() const
{
    return qobject_cast<QSslSocket *>(m_socket)->peerCertificate().digest();
}
