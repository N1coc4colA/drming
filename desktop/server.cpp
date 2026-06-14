#include "server.h"

#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QMap>
#include <QPair>
#include <QHostAddress>
#include <QDebug>
#include <QDtls>
#include <algorithm>

#include "../certificatesupport.h"
#include "parameters.h"

// Internal DTLS-backed NetworkClient implementation
class DtlsNetworkClient : public NetworkClient
{
public:
    DtlsNetworkClient(const QHostAddress &addr, quint16 port, QDtls *dtls, QUdpSocket *socket, QObject *parent = nullptr)
        : NetworkClient(parent)
        , m_addr(addr)
        , m_port(port)
        , m_dtls(dtls)
        , m_socket(socket)
    {}

    ~DtlsNetworkClient() override {
        // Server owns the QDtls instances (stored in Server::m_dtlsMap). Do not delete here.
    }

    qint64 write(const QByteArray &data) override {
        if (!m_dtls || !m_socket) return -1;
        return m_dtls->writeDatagramEncrypted(m_socket, data);
    }

    QAbstractSocket::SocketState state() const override { return QAbstractSocket::ConnectedState; }
    QHostAddress peerAddress() const override { return m_addr; }
    quint16 peerPort() const override { return m_port; }

private:
    QHostAddress m_addr{};
    quint16 m_port = 0;
    QDtls *m_dtls = nullptr;
    QUdpSocket *m_socket = nullptr;
};

// Map key helper (string form address:port)
static inline QString keyFor(const QHostAddress &a, quint16 p) { return QStringLiteral("%1:%2").arg(a.toString()).arg(p); }

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
        qWarning() << "SSL config not loaded; DTLS connections may not use server certificate.";
    }

    if (!m_socket.bind(address, port)) {
        qCritical() << "Failed to bind UDP socket:" << m_socket.errorString();
        return false;
    }

    qInfo() << "Exposing DTLS service on:" << address.toString() << ':' << port;
    return true;
}

void Server::close()
{
    for (auto client : m_clients) {
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
    for (auto client : m_clients) {
        if (!client) continue;
        if (client->state() == QAbstractSocket::ConnectedState) {
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
        qint64 read = m_socket.readDatagram(dgram.data(), dgram.size(), &sender, &senderPort);
        if (read <= 0) continue;
        dgram.resize(read);

        const auto k = keyFor(sender, senderPort);

        // Create or look up a QDtls association for this peer
        QDtls *dtls = m_dtlsMap.value(k, nullptr);

        if (!dtls) {
            // Create new server-side DTLS object
            dtls = new QDtls(QSslSocket::SslServerMode, this);
            QSslConfiguration conf = QSslConfiguration::defaultDtlsConfiguration();
            if (loadServerSslConfig(conf)) {
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
                qInfo() << "DTLS client closed:" << sender.toString() << senderPort;

                auto client = std::find_if(m_clients.begin(), m_clients.end(), [&](NetworkClient *c) {
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
            for (auto c : m_clients) {
                if (c && c->peerAddress() == sender && c->peerPort() == senderPort) {
                    alreadyRegistered = true;
                    break;
                }
            }

            if (!alreadyRegistered) {
                auto *wrapper = new DtlsNetworkClient(sender, senderPort, dtls, &m_socket, this);
                m_clients.append(wrapper);
                Q_EMIT clientConnected(wrapper);
            }
        }
    }
}
