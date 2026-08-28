#include "server.h"

#include <QDebug>
#include <QDtls>
#include <QSslConfiguration>

#include "parameters.h"

#include "../net/certificatesupport.h"
#include "../settings.h"

static QString keyFor(const QHostAddress &a, quint16 p)
{
    return QStringLiteral("%1:%2").arg(a.toString()).arg(p);
}

Server::Server(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket4.first, &QUdpSocket::readyRead, this, &Server::onDatagramReceived4);
    connect(&m_socket6.first, &QUdpSocket::readyRead, this, &Server::onDatagramReceived6);
}

bool Server::listen(const QHostAddress &address4, const QHostAddress &address6, quint16 port)
{
    QSslConfiguration sslConfig;
    if (!loadServerCertsConfig(sslConfig, "dtls", Parameters::instance.serverCertPath, Parameters::instance.serverKeyPath)) {
        qCritical() << "SSL config not loaded; DTLS connections may not use server certificate.";
        return false;
    }

    if (!address4.isNull()) {
        if (!m_socket4.first.bind(address4, port, QAbstractSocket::ShareAddress)) {
            qCritical() << "Failed to bind UDP socket:" << m_socket4.first.errorString();
            return false;
        }
        qInfo() << "Exposing DTLS service on:" << address4.toString() << ':' << port;
    }

    if (!address6.isNull()) {
        if (!m_socket6.first.bind(address6, port, QAbstractSocket::ShareAddress)) {
            qCritical() << "Failed to bind UDP socket:" << m_socket6.first.errorString();
            return false;
        }
        qInfo() << "Exposing DTLS service on:" << address6.toString() << ':' << port;
    }

    return true;
}

void Server::close()
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

    m_socket4.first.close();
    m_socket6.first.close();
}

void Server::broadcast(const QByteArray &data)
{
    for (auto *client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
        }
    }
}

void Server::onDatagramReceived4()
{
    processSocketPendings(m_socket4);
}

void Server::onDatagramReceived6()
{
    processSocketPendings(m_socket6);
}

void Server::processSocketPendings(QPair<QUdpSocket, QMutex> &pair)
{
    auto &socket = pair.first;

    while (socket.hasPendingDatagrams()) {
        QByteArray dgram(socket.pendingDatagramSize(), Qt::Uninitialized);
        QHostAddress sender;
        quint16 senderPort;

        {
            const auto read = socket.readDatagram(dgram.data(), dgram.size(), &sender, &senderPort);
            if (read <= 0) {
                continue;
            }
            dgram.resize(read);
            QMutexLocker lock(&pair.second);
        }

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
            if (!dtls->doHandshake(&socket, dgram)) {
                qWarning() << "DTLS handshake error for" << sender << senderPort << ":" << dtls->dtlsErrorString();
                m_pendingHandshakes.remove(key);
                delete dtls;
                continue;
            }

            if (dtls->isConnectionEncrypted()) {
                // Handshake completed – create client
                qDebug() << "DTLS encryption established for" << sender << senderPort;
                auto client = new NetworkClient(dtls, sender, senderPort, pair, this);
                m_pendingHandshakes.remove(key);
                m_clients.insert(key, client);
                connect(client, &NetworkClient::disconnected, this, &Server::onClientDisconnected);
                Q_EMIT clientConnected(client);
            }
            continue;
        }

        // 3. New peer – start handshake
        qDebug() << "New DTLS handshake from" << sender << senderPort;
        auto dtls = new QDtls(QSslSocket::SslServerMode, this);
        auto conf = QSslConfiguration::defaultDtlsConfiguration();
        if (loadServerCertsConfig(conf, "dtls", Parameters::instance.serverCertPath, Parameters::instance.serverKeyPath)) {
            dtls->setDtlsConfiguration(conf);
        }
        dtls->setMtuHint(Settings::dtlsChunkSize);
        dtls->setPeer(sender, senderPort);

        if (!dtls->doHandshake(&socket, dgram)) {
            qWarning() << "Failed to start DTLS handshake for" << sender << senderPort << ":" << dtls->dtlsErrorString();
            delete dtls;
            continue;
        }

        // If handshake completed immediately (rare), create client; otherwise store pending
        if (dtls->isConnectionEncrypted()) {
            qDebug() << "DTLS encryption established immediately for" << sender << senderPort;
            auto client = new NetworkClient(dtls, sender, senderPort, pair, this);
            m_clients.insert(key, client);
            connect(client, &NetworkClient::disconnected, this, &Server::onClientDisconnected);
            Q_EMIT clientConnected(client);
        } else {
            m_pendingHandshakes.insert(key, dtls);
        }
    }
}

void Server::onClientDisconnected()
{
    if (auto client = qobject_cast<NetworkClient *>(sender())) {
        removeClient(client);
    }
}

void Server::removeClient(NetworkClient *client)
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
