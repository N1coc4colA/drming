#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QtEndian>
#include <QHostAddress>

#include "fileprovider.h"

NetworkLink::NetworkLink(QObject *parent)
    : QObject{parent}
    , Packets::Parser<NetworkLink>(*this)
    , m_udpSocket(new QUdpSocket(this))
    , m_dtls(new QDtls(QSslSocket::SslClientMode, this))
{
    QAbstractSocket::connect(m_udpSocket, &QUdpSocket::connected, this, &NetworkLink::onConnected);
    QAbstractSocket::connect(m_udpSocket, &QUdpSocket::disconnected, this, &NetworkLink::onDisconnected);
    QAbstractSocket::connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkLink::onDataAvailable);
    QAbstractSocket::connect(m_udpSocket, &QUdpSocket::errorOccurred, this, &NetworkLink::onError);

    QAbstractSocket::connect(m_dtls, &QDtls::handshakeTimeout, this, [this]() { qWarning() << "DTLS handshake timeout"; });

    m_inactivityTimer.setInterval(5000);
    m_inactivityTimer.setSingleShot(true);
    QAbstractSocket::connect(&m_inactivityTimer, &QTimer::timeout, this, &NetworkLink::onConnectionTimeout);

    // No UDP-specific socket options to set here.
}

NetworkLink::~NetworkLink()
{
    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }
    m_udpSocket->close();
}

void NetworkLink::close()
{
    if (!m_udpSocket || m_udpSocket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }

    m_inactivityTimer.stop();
    m_udpSocket->close();
}

void NetworkLink::connect(const QString &address, const int port, const QString &clientName)
{
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    qInfo() << "Connecting to (DTLS):" << address << port << "using client" << clientName;

    // Use encrypted connection
    const auto clientData = FileProvider::instance()->clientData(clientName);
    if (clientData.first.isNull()) {
        Q_EMIT NetworkLink::error(tr("The certificate of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }
    if (clientData.second.isNull()) {
        Q_EMIT NetworkLink::error(tr("The key of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }

    // Disable all default CA verification — we do our own allowlist check
    auto sslConf = QSslConfiguration::defaultDtlsConfiguration();
    sslConf.setCaCertificates({});
    sslConf.setPeerVerifyMode(QSslSocket::VerifyNone); // Avoid chain validation
    sslConf.setLocalCertificate(clientData.first);
    sslConf.setPrivateKey(clientData.second);

    m_dtls->setDtlsConfiguration(sslConf);
    m_dtls->setMtuHint(1200);

    const QHostAddress hostAddress(address);
    m_dtls->setPeer(hostAddress, static_cast<quint16>(port));

    // Bind ephemeral local port
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, 0)) {
        qWarning() << "Failed to bind UDP socket:" << m_udpSocket->errorString();
        Q_EMIT NetworkLink::error(m_udpSocket->errorString());
        return;
    }

    // Connect the UDP socket to the remote peer so readDatagram() only yields packets from peer
    m_udpSocket->connectToHost(address, static_cast<quint16>(port));

    if (!m_dtls->doHandshake(m_udpSocket)) {
        Q_EMIT NetworkLink::error(tr("Failed to start DTLS handshake: %1").arg(m_dtls->dtlsErrorString()));
        return;
    }

    qInfo() << "DTLS handshake started";
}

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    qWarning() << "Connection error occurred: " << error;
    if (m_udpSocket) {
        Q_EMIT NetworkLink::error(m_udpSocket->errorString());
    } else {
        Q_EMIT NetworkLink::error(tr("A network error occurred."));
    }
}

void NetworkLink::onConnected()
{
    qInfo() << "UDP socket connected";
}

void NetworkLink::onDisconnected()
{
    m_udpSocket->close();
    m_buffer.clear();
    m_inactivityTimer.stop();
}

void NetworkLink::onDataAvailable()
{
    m_inactivityTimer.start();

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray dgram(m_udpSocket->pendingDatagramSize(), Qt::Uninitialized);
        const qint64 bytesRead = m_udpSocket->readDatagram(dgram.data(), dgram.size());
        if (bytesRead <= 0) {
            qWarning() << "Spurious UDP read";
            return;
        }
        dgram.resize(bytesRead);

        if (!m_dtls->isConnectionEncrypted()) {
            // Continue handshake with incoming datagram
            if (!m_dtls->doHandshake(m_udpSocket, dgram)) {
                if (m_dtls->dtlsError() == QDtlsError::PeerVerificationError) {
                    Q_EMIT NetworkLink::error(tr("DTLS handshake error: %1").arg(m_dtls->dtlsErrorString()));
                } else {
                    Q_EMIT NetworkLink::error(tr("DTLS handshake error: %1").arg(m_dtls->dtlsErrorString()));
                }
                return;
            }

            if (m_dtls->isConnectionEncrypted()) {
                qInfo() << "DTLS encrypted connection established";
                Q_EMIT opened();
            }

            continue;
        }

        // Decrypt application datagram
        const QByteArray plain = m_dtls->decryptDatagram(m_udpSocket, dgram);
        if (!plain.isEmpty()) {
            addData(plain);
            continue;
        }

        // If plain is empty, check for shutdown/remote-close
        if (m_dtls->dtlsError() == QDtlsError::RemoteClosedConnectionError) {
            qWarning() << "DTLS shutdown received";
            m_udpSocket->close();
            Q_EMIT closed();
            return;
        }

        qWarning() << "Received zero-length DTLS plaintext or unexpected datagram";
    }
}

void NetworkLink::onConnectionTimeout()
{
    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        Q_EMIT error(tr("Connection timed out."));
        close();
    }
}

void NetworkLink::processPacket(const Packets::ServerImage &srvImg)
{
    const auto img = QImage::fromData(srvImg.data, "WEBP");

    if (!img.isNull()) {
        [[likely]];
        Q_EMIT imageReady(img);
    }
}

void NetworkLink::processPacket(const Packets::ServerBrightness &brightness)
{
    Q_UNUSED(brightness);
}
