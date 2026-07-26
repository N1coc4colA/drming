#include "dtlsserver.h"

#include <QDebug>
#include <QDtls>
#include <QHostAddress>
#include <QMap>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>

#include "../settings.h"

// Map key helper (string form address:port)
static QString keyFor(const QHostAddress &a, const quint16 p) { return QStringLiteral("%1:%2").arg(a.toString()).arg(p); }

DtlsServer::DtlsServer(QObject *parent)
    : Server(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &DtlsServer::onDatagramReceived);
}

bool DtlsServer::listen(const QHostAddress &address, const quint16 port)
{
    QSslConfiguration sslConfig{};
    if (!loadServerCertsConfig(sslConfig, "dtls")) {
        qCritical() << "SSL config not loaded; DTLS connections may not use server certificate.";
        return false;
    }

    m_address = address;
    m_port = port;

    if (!m_socket->bind(m_address, m_port, QAbstractSocket::ShareAddress)) [[unlikely]] {
        qCritical() << "Failed to bind UDP socket:" << m_socket->errorString();
        return false;
    }

    qInfo() << "Exposing DTLS service on:" << address.toString() << ':' << port;
    return true;
}

void DtlsServer::close()
{
    for (const auto client : m_clients) {
        if (client) {
            client->deleteLater();
        }
    }

    m_clients.clear();

    // Clean up DTLS associations
    for (auto it = m_dtlsMap.begin(); it != m_dtlsMap.end(); ++it) {
        delete it.value();
    }
    m_dtlsMap.clear();

    m_socket->close();
}

void DtlsServer::broadcast(const QByteArray &data)
{
    // Send encrypted datagram to all known DTLS associations
    // We keep track of associations via m_clients list and rely on the DtlsNetworkClient to do the encryption.
    for (const auto client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
        }
    }
}

void DtlsServer::onDatagramReceived()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray dgram(m_socket->pendingDatagramSize(), Qt::Uninitialized);
        QHostAddress sender;
        quint16 senderPort;

        const auto read = m_socket->readDatagram(dgram.data(), dgram.size(), &sender, &senderPort);
        if (read <= 0) {
            continue;
        }
        dgram.resize(read);

        // Check if we already have a dedicated socket for this peer
        if (m_peerSockets.find({sender, senderPort}) != m_peerSockets.end()) {
            qWarning() << "Received packets on main DTLS socket, which should not happen.";
            continue;
        }

        // Otherwise, it's a new handshake or a stale packet
        const auto key = keyFor(sender, senderPort);
        auto dtls = m_dtlsMap.value(key, nullptr);
        if (!dtls) {
            dtls = new QDtls(QSslSocket::SslServerMode, this);
            auto conf = QSslConfiguration::defaultDtlsConfiguration();
            if (loadServerCertsConfig(conf, "dtls")) [[likely]] {
                dtls->setDtlsConfiguration(conf);
            }

            dtls->setMtuHint(Settings::dtlsChunkSize);
            dtls->setPeer(sender, senderPort);
            m_dtlsMap.insert(key, dtls);
        }

        // Process handshake
        if (!dtls->doHandshake(m_socket, dgram)) {
            // [TODO] Generate handshake error message.
            m_dtlsMap.remove(key);
            delete dtls;
            continue;
        }

        if (dtls->isConnectionEncrypted()) {
            // Handshake finished – create a dedicated socket for this peer
            auto dedicated = m_socket;
            disconnect(m_socket, &QUdpSocket::readyRead, this, &DtlsServer::onDatagramReceived);

            m_socket = new QUdpSocket(this);
            if (!m_socket->bind(m_address, m_port, QUdpSocket::ShareAddress)) [[unlikely]] {
                // [TODO] Properly handle error.
                qWarning() << "Failed to bind dedicated UDP socket";
                delete dedicated;
                return;
            }

            // Create a NetworkClientDtls that holds the QDtls and the dedicated socket
            auto wrapper = new NetworkClientDtls(dtls, dedicated, this);

            // Remove from m_dtlsMap and add to m_clients
            m_dtlsMap.remove(keyFor(sender, senderPort));
            m_clients.append(wrapper);
            Q_EMIT clientConnected(wrapper);
        }
    }
}
