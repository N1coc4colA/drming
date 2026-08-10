#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QtEndian>

#include "../settings.h"

#include "fileprovider.h"

#ifdef Q_OS_ANDROID
#include "platform/android/networklink.h"
#else
#include "platform/linux/networklink.h"
#endif

NetworkLink::NetworkLink(QObject *parent)
    : QObject(parent)
    , m_sslSocket(new QSslSocket(this))
    , m_udpSocket(new QUdpSocket(this))
    , m_dtls(new QDtls(QSslSocket::SslClientMode, this))
{
    QObject::connect(m_udpSocket, &QUdpSocket::connected, this, &NetworkLink::onConnected);
    QObject::connect(m_udpSocket, &QUdpSocket::disconnected, this, &NetworkLink::onDtlsDisconnected);
    QObject::connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkLink::onDtlsDataAvailable);
    QObject::connect(m_udpSocket, &QUdpSocket::errorOccurred, this, &NetworkLink::onError);

    QObject::connect(m_sslSocket, &QSslSocket::connected, this, &NetworkLink::onConnected);
    QObject::connect(m_sslSocket, &QSslSocket::disconnected, this, &NetworkLink::onSslDisconnected);
    QObject::connect(m_sslSocket, &QSslSocket::readyRead, this, &NetworkLink::onSslDataAvailable);
    QObject::connect(m_sslSocket, &QSslSocket::handshakeInterruptedOnError, this, &NetworkLink::onSslError);
    QObject::connect(m_sslSocket, &QSslSocket::errorOccurred, this, &NetworkLink::onError);
    QObject::connect(m_sslSocket, &QSslSocket::peerVerifyError, [](const QSslError &error) { qWarning() << "Peer verification error:" << error; });
    QObject::connect(m_sslSocket, &QSslSocket::encrypted, []() { qInfo() << "SSL connection established"; });
    QObject::connect(m_sslSocket, &QSslSocket::modeChanged, [](const QSslSocket::SslMode newMode) { qInfo() << "SSL mode changed:" << newMode; });

    QObject::connect(m_dtls, &QDtls::handshakeTimeout, this, [this] {
        if (m_inactivityTimer.remainingTime() > 0) {
            m_dtls->handleTimeout(m_udpSocket);
        } else {
            qWarning() << "DTLS handshake timeout";
        }
    });

    QObject::connect(&m_inactivityTimer, &QTimer::timeout, this, &NetworkLink::onConnectionTimeout);
    m_inactivityTimer.setTimerType(Qt::VeryCoarseTimer);
    m_inactivityTimer.setSingleShot(true);
    m_inactivityTimer.setInterval(Settings::inactivityTimeout + 2000);
    m_inactivityTimer.stop();
}

NetworkLink::~NetworkLink()
{
    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }

    m_inactivityTimer.stop();
    m_udpSocket->close();
    m_sslSocket->close();
}

NetworkLink *NetworkLink::createForPlatform(QObject *parent)
{
    return new Platform::NetworkLink(parent);
}

void NetworkLink::close()
{
    if (!m_sslSocket && (!m_udpSocket || m_udpSocket->state() == QAbstractSocket::UnconnectedState)) {
        return;
    }

    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }

    m_inactivityTimer.stop();
    m_udpSocket->close();
    m_sslSocket->close();
}

void NetworkLink::connect(const QString &address, const int port, const QString &clientName, const QString &protocolName)
{
    m_sslSocket->close();
    m_udpSocket->close();

    if (m_sslSocket->state() != QAbstractSocket::UnconnectedState || m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    qInfo() << "Connecting to:" << address << port << "using client" << clientName << "with protocol" << protocolName;

    // Use encrypted connection
    const auto clientData = FileProvider::instance()->clientData(clientName);
    if (clientData.first.isNull()) {
        Q_EMIT error(tr("The certificate of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }
    if (clientData.second.isNull()) {
        Q_EMIT error(tr("The key of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }

    // Disable all default CA verification — we do our own allowlist check
    auto sslConf = protocolName == "dtls" ? QSslConfiguration::defaultDtlsConfiguration() : QSslConfiguration::defaultConfiguration();
    sslConf.setCaCertificates({});
    sslConf.setPeerVerifyMode(QSslSocket::VerifyNone); // Avoid chain validation
    sslConf.setLocalCertificate(clientData.first);
    sslConf.setPrivateKey(clientData.second);

    const QHostAddress hostAddress(address);

    if (protocolName == "dtls") {
        connectDtls(sslConf, hostAddress, port);
    } else if (protocolName == "ssl") {
        connectSsl(sslConf, hostAddress, port);
    } else {
        qWarning() << "Unknown protocol name requested to connect to server:" << protocolName;
        Q_EMIT error(tr("The protocol is invalid: %1").arg(clientName));
    }

    m_inactivityTimer.start();
}

void NetworkLink::connectDtls(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, const int port)
{
    m_protocol = "dtls";

    m_dtls->setDtlsConfiguration(sslConf);
    m_dtls->setMtuHint(Settings::dtlsChunkSize);

    m_dtls->setPeer(hostAddress, static_cast<quint16>(port));

    // Bind ephemeral local port
    // [TODO] Should automatically switch between IPv4 & v6.
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, 0)) {
        qWarning() << "Failed to bind UDP socket:" << m_udpSocket->errorString();
        Q_EMIT error(m_udpSocket->errorString());
        return;
    }

    // Connect the UDP socket to the remote peer so readDatagram() only yields packets from peer
    m_udpSocket->connectToHost(hostAddress, static_cast<quint16>(port));

    if (!m_dtls->doHandshake(m_udpSocket)) {
        Q_EMIT error(tr("Failed to start DTLS handshake: %1").arg(m_dtls->dtlsErrorString()));
        return;
    }

    qInfo() << "DTLS handshake started";
}

void NetworkLink::connectSsl(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, const int port)
{
    m_protocol = "ssl";

    // Apply configuration before starting the handshake
    m_sslSocket->setSslConfiguration(sslConf);

    QObject::connect(m_sslSocket, &QSslSocket::encrypted, this, [this]() { qInfo() << "SSL handshake completed"; });
    QObject::connect(m_sslSocket, qOverload<QAbstractSocket::SocketError>(&QSslSocket::errorOccurred), this, &NetworkLink::onError);

    m_sslSocket->connectToHost(hostAddress.toString(), static_cast<quint16>(port));

    if (!m_sslSocket->waitForConnected()) {
        Q_EMIT error(tr("Failed to connect TCP socket: %1").arg(m_sslSocket->errorString()));
        return;
    }

    // Starts the client-side SSL handshake after TCP connection is established.
    m_sslSocket->startClientEncryption();

    if (!m_sslSocket->waitForEncrypted()) {
        Q_EMIT error(tr("Failed to connect TCP socket: %1").arg(m_sslSocket->errorString()));
        return;
    }
}

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    qWarning() << "Connection error occurred: " << error;

    if (m_protocol == "dtls" && m_udpSocket) {
        Q_EMIT NetworkLink::error(m_udpSocket->errorString());
    } else if (m_protocol == "ssl" && m_sslSocket) {
        Q_EMIT NetworkLink::error(m_sslSocket->errorString());
    } else {
        Q_EMIT NetworkLink::error(tr("A network error occurred."));
    }
}

void NetworkLink::onSslError(const QSslError &error)
{
    qWarning() << "SSL Connection error occurred: " << error;
}

void NetworkLink::onConnected()
{
    qInfo() << "Socket connected";
    Q_EMIT connectionInitialised();
}

void NetworkLink::onDtlsDisconnected()
{
    m_udpSocket->close();
    m_buffer.clear();
    m_inactivityTimer.stop();
}

void NetworkLink::onSslDisconnected()
{
    m_sslSocket->close();
}

void NetworkLink::onDtlsDataAvailable()
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
                if (m_dtls->dtlsError() != QDtlsError::NoError) {
                    Q_EMIT error(tr("DTLS handshake error: %1").arg(m_dtls->dtlsErrorString()));
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
            addData(std::move(plain));
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

void NetworkLink::onSslDataAvailable()
{
    m_inactivityTimer.start();

    const QByteArray plain = m_sslSocket->readAll();
    if (!plain.isEmpty()) {
        addData(std::move(plain));
        return;
    }

    // If the peer has closed the connection, QSslSocket will eventually
    // emit disconnected(); treat that as the shutdown path.
    if (m_sslSocket->state() == QAbstractSocket::UnconnectedState) {
        qWarning() << "SSL shutdown received";
        Q_EMIT closed();
        return;
    }

    qWarning() << "Received empty SSL payload";
}

void NetworkLink::onConnectionTimeout()
{
    if (m_sslSocket || (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete)) {
        Q_EMIT error(tr("Connection timed out."));
        close();
    }
}

void NetworkLink::write(const QByteArray &data)
{
    if (m_udpSocket->state() == QAbstractSocket::ConnectedState) {
        writeDtls(data);
    } else if (m_sslSocket->state() == QAbstractSocket::ConnectedState) {
        writeSsl(data);
    }
}

qint64 NetworkLink::writeDtls(const QByteArray &data)
{
    if (m_udpSocket->state() != QAbstractSocket::ConnectedState) [[unlikely]] {
        return -1;
    }

    qsizetype offset = 0;
    while (offset < data.size()) {
        const auto chunk = data.mid(offset, Settings::dtlsChunkSize);

        if (m_dtls->writeDatagramEncrypted(m_udpSocket, chunk) < 0) [[unlikely]] {
            const auto err = m_dtls->dtlsError();
            if (err != QDtlsError::NoError && err != QDtlsError::UnderlyingSocketError) {
                qWarning() << "DTLS Error" << static_cast<int>(err) << ":" << m_dtls->dtlsErrorString();
            }

            return -1;
        }

        offset += chunk.size();
    }

    return offset;
}

qint64 NetworkLink::writeSsl(const QByteArray &data)
{
    // [TODO] Handle this stuff more gracefully. Would need a loop to ensure everything's been written 'til the end.
    const auto ret = m_sslSocket->write(data);

    m_sslSocket->waitForBytesWritten();

    return ret;
}
