#include "server.h"

#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QMap>
#include <QHostAddress>
#include <QDebug>
#include <QDtls>
#include <algorithm>

#include "../certificatesupport.h"

#include "parameters.h"

// Map key helper (string form address:port)
static QString keyFor(const QHostAddress &a, const quint16 p) { return QStringLiteral("%1:%2").arg(a.toString()).arg(p); }

Server::Server(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &Server::onDatagramReceived);
}

bool Server::loadServerSslConfig(QSslConfiguration &outConfig)
{
    const auto serverCert = openCertificate(Parameters::instance.serverCertPath);
    const auto serverKey = openKey(Parameters::instance.serverKeyPath);
    if (serverKey.isNull() || serverCert.isNull()) {
        [[unlikely]];

        return false;
    }

    auto conf = QSslConfiguration::defaultDtlsConfiguration();
    conf.setLocalCertificate(serverCert);
    conf.setPrivateKey(serverKey);
    conf.setDtlsCookieVerificationEnabled(false);
    outConfig = conf;

    return true;
}

bool Server::listen(const QHostAddress &address, const quint16 port)
{
    QSslConfiguration sslConfig{};
    if (!loadServerSslConfig(sslConfig)) {
        qCritical() << "SSL config not loaded; DTLS connections may not use server certificate.";
        return false;
    }

    if (!m_socket.bind(address, port)) {
        [[unlikely]];

        qCritical() << "Failed to bind UDP socket:" << m_socket.errorString();
        return false;
    }

    qInfo() << "Exposing DTLS service on:" << address.toString() << ':' << port;
    return true;
}

void Server::close()
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

    m_socket.close();
}

void Server::broadcast(const QByteArray &data)
{
    // Send encrypted datagram to all known DTLS associations
    // We keep track of associations via m_clients list and rely on the DtlsNetworkClient to do the encryption.
    for (const auto client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
        }
    }
}

void Server::onDatagramReceived()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray dgram(m_socket.pendingDatagramSize(), Qt::Uninitialized);
        QHostAddress sender;
        quint16 senderPort = 0;
        const qint64 read = m_socket.readDatagram(dgram.data(), dgram.size(), &sender, &senderPort);
        if (read <= 0) {
            continue;
        }

        dgram.resize(read);

        const auto k = keyFor(sender, senderPort);

        // Create or look up a QDtls association for this peer
        QDtls *dtls = m_dtlsMap.value(k, nullptr);

        if (!dtls) {
            [[unlikely]];

            // Create new server-side DTLS object
            dtls = new QDtls(QSslSocket::SslServerMode, this);
            QSslConfiguration conf = QSslConfiguration::defaultDtlsConfiguration();
            if (loadServerSslConfig(conf)) {
                [[likely]];

                dtls->setDtlsConfiguration(conf);
            }

            dtls->setMtuHint(1200);
            dtls->setPeer(sender, senderPort);
            m_dtlsMap.insert(k, dtls);
        }

        if (dtls->handshakeState() == QDtls::HandshakeComplete) {
            const QByteArray plain = dtls->decryptDatagram(&m_socket, dgram);
            if (!plain.isEmpty()) {
                // Server currently does not consume client application data.
                continue;
            }

            if (dtls->dtlsError() == QDtlsError::RemoteClosedConnectionError) {
                [[unlikely]];

                qInfo() << "DTLS client closed:" << sender.toString() << senderPort;

                auto client = std::ranges::find_if(m_clients, [&](const NetworkClient *c) {
                    return c && c->peerAddress() == sender && c->peerPort() == senderPort;
                });
                if (client != m_clients.end()) {
                    Q_EMIT (*client)->disconnected();
                    (*client)->deleteLater();
                    m_clients.erase(client);
                }

                m_dtlsMap.remove(k);
                delete dtls;

                if (m_clients.isEmpty()) {
                    Q_EMIT noClient();
                }
            } else {
                qWarning() << "Unexpected DTLS datagram from" << sender.toString() << senderPort;
            }
            continue;
        }

        // Continue or start handshake
        if (!dtls->doHandshake(&m_socket, dgram)) {
            [[unlikely]];

            if (dtls->dtlsError() == QDtlsError::RemoteClosedConnectionError) {
                qInfo() << "DTLS handshake aborted by peer:" << sender.toString() << senderPort;
            } else {
                qWarning() << "DTLS handshake error from" << sender.toString() << senderPort << ":" << dtls->dtlsErrorString();
            }

            m_dtlsMap.remove(k);
            delete dtls;
            continue;
        }

        if (dtls->isConnectionEncrypted()) {
            bool alreadyRegistered = false;
            for (const auto c : m_clients) {
                if (c && c->peerAddress() == sender && c->peerPort() == senderPort) {
                    alreadyRegistered = true;
                    break;
                }
            }

            if (!alreadyRegistered) {
                auto *wrapper = new NetworkClientDtls(dtls, &m_socket, this);
                m_clients.append(wrapper);
                Q_EMIT clientConnected(wrapper);
            }
        }
    }
}
