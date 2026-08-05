#include "dtlsserver.h"

#include <QDtls>
#include <QSslConfiguration>
#include <QDebug>

#include "../settings.h"
#include "../certificatesupport.h"

static QString keyFor(const QHostAddress &a, quint16 p)
{
    return QStringLiteral("%1:%2").arg(a.toString()).arg(p);
}

DtlsServer::DtlsServer(QObject *parent)
    : Server(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &DtlsServer::onDatagramReceived);
}

bool DtlsServer::listen(const QHostAddress &address, quint16 port)
{
    QSslConfiguration sslConfig;
    if (!loadServerCertsConfig(sslConfig, "dtls")) {
        qCritical() << "SSL config not loaded; DTLS connections may not use server certificate.";
        return false;
    }

    m_address = address;
    m_port = port;

    if (!m_socket->bind(m_address, m_port, QAbstractSocket::ShareAddress)) {
        qCritical() << "Failed to bind UDP socket:" << m_socket->errorString();
        return false;
    }

    qInfo() << "Exposing DTLS service on:" << address.toString() << ':' << port;
    return true;
}

void DtlsServer::close()
{
    // Delete all clients
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        it.value()->deleteLater();
    }
    m_clients.clear();

    // Delete pending handshakes
    for (auto it = m_pendingHandshakes.begin(); it != m_pendingHandshakes.end(); ++it) {
        delete it.value();
    }
    m_pendingHandshakes.clear();

    m_socket->close();
}

void DtlsServer::broadcast(const QByteArray &data)
{
    for (auto *client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
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

        const auto key = keyFor(sender, senderPort);

        // 1. Check if we already have an active client for this peer
        if (auto client = m_clients.value(key, nullptr)) {
            client->incomingEncryptedData(dgram);
            continue;
        }

        // 2. Check if a handshake is in progress
        if (auto dtls = m_pendingHandshakes.value(key, nullptr)) {
            qDebug() << "Pending DTLS already present";

            // Process handshake message
            if (!dtls->doHandshake(m_socket, dgram)) {
                qWarning() << "DTLS handshake error for" << sender << senderPort << ":" << dtls->dtlsErrorString();
                m_pendingHandshakes.remove(key);
                delete dtls;
                continue;
            }

            if (dtls->isConnectionEncrypted()) {
                // Handshake completed – create client
                qDebug() << "DTLS encryption established for" << sender << senderPort;
                auto client = new NetworkClientDtls(dtls, sender, senderPort, m_socket, this);
                m_pendingHandshakes.remove(key);
                m_clients.insert(key, client);
                connect(client, &NetworkClient::disconnected, this, &DtlsServer::onClientDisconnected);
                Q_EMIT clientConnected(client);
            }
            continue;
        }

        // 3. New peer – start handshake
        qDebug() << "New DTLS handshake from" << sender << senderPort;
        auto dtls = new QDtls(QSslSocket::SslServerMode, this);
        auto conf = QSslConfiguration::defaultDtlsConfiguration();
        if (loadServerCertsConfig(conf, "dtls")) {
            dtls->setDtlsConfiguration(conf);
        }
        dtls->setMtuHint(Settings::dtlsChunkSize);
        dtls->setPeer(sender, senderPort);

        if (!dtls->doHandshake(m_socket, dgram)) {
            qWarning() << "Failed to start DTLS handshake for" << sender << senderPort << ":" << dtls->dtlsErrorString();
            delete dtls;
            continue;
        }

        // If handshake completed immediately (rare), create client; otherwise store pending
        if (dtls->isConnectionEncrypted()) {
            qDebug() << "DTLS encryption established immediately for" << sender << senderPort;
            auto client = new NetworkClientDtls(dtls, sender, senderPort, m_socket, this);
            m_clients.insert(key, client);
            connect(client, &NetworkClient::disconnected, this, &DtlsServer::onClientDisconnected);
            Q_EMIT clientConnected(client);
        } else {
            m_pendingHandshakes.insert(key, dtls);
        }
    }
}

void DtlsServer::onClientDisconnected()
{
    if (auto client = qobject_cast<NetworkClientDtls *>(sender())) {
        removeClient(client);
    }
}

void DtlsServer::removeClient(NetworkClientDtls *client)
{
    const auto key = keyFor(client->peerAddress(), client->peerPort());
    if (m_clients.remove(key) > 0) {
        client->deleteLater();
        qDebug() << "Client removed" << client->peerAddress() << client->peerPort();
        if (m_clients.isEmpty()) {
            Q_EMIT noClient();
        }
    }
}
