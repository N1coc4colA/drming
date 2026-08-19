#include "sslserver.h"

#include <QDebug>
#include <QSslCertificate>
#include <QSslError>
#include <QTcpServer>
#include <QTimer>

class SslServerImpl : public QTcpServer
{
public:
    explicit SslServerImpl(SslServer *server)
        : QTcpServer(server)
        , m_server(server)
    {}

protected:
    void incomingConnection(const qintptr socketDescriptor) override
    {
        // Create SSL socket for this connection
        QSslSocket *sslSocket = new QSslSocket(this);

        // Load SSL configuration
        sslSocket->setSslConfiguration(m_server->m_sslConfig);

        // Set the socket descriptor
        if (!sslSocket->setSocketDescriptor(socketDescriptor)) {
            qWarning() << "Failed to set socket descriptor:" << sslSocket->errorString();
            delete sslSocket;
            return;
        }

        // Connect signals
        QObject::connect(sslSocket, &QSslSocket::encrypted, m_server, &SslServer::onClientEncrypted);
        QObject::connect(sslSocket, &QSslSocket::readyRead, m_server, &SslServer::onClientReadyRead);
        QObject::connect(sslSocket, &QSslSocket::disconnected, m_server, &SslServer::onClientDisconnected);
        QObject::connect(sslSocket, &QSslSocket::sslErrors, m_server, &SslServer::onClientSslErrors);

        // Add to our list
        m_server->m_sockets.append(sslSocket);

        // Start SSL handshake
        sslSocket->startServerEncryption();
    }

private:
    SslServer *m_server = nullptr;
};

SslServer::SslServer(QObject *parent)
    : Server(parent)
    , m_server4(new SslServerImpl(this))
    , m_server6(new SslServerImpl(this))
{
    const auto warn = [](const QAbstractSocket::SocketError socketError) { qWarning() << "SSL acceptation error occurred:" << socketError; };
    connect(m_server4, &QTcpServer::acceptError, warn);
    connect(m_server6, &QTcpServer::acceptError, warn);
}

bool SslServer::listen(const QHostAddress &address4, const QHostAddress &address6, const quint16 port)
{
    if (!loadServerCertsConfig(m_sslConfig, "ssl")) {
        qCritical() << "SSL config not loaded; SSL connections may not use server certificate.";
        return false;
    }

    // Store SSL config for use in incomingConnection
    // We'll set it on each new socket

    if (!address4.isNull()) {
        if (!m_server4->listen(address4, port)) {
            qCritical() << "Failed to bind TCP socket:" << m_server4->errorString();
            return false;
        }
        qInfo() << "Exposing SSL service on:" << address4.toString() << ':' << port;
    }

    if (!address6.isNull()) {
        if (!m_server6->listen(address6, port)) {
            qCritical() << "Failed to bind TCP socket:" << m_server6->errorString();
            return false;
        }
        qInfo() << "Exposing SSL service on:" << address6.toString() << ':' << port;
    }

    return true;
}

void SslServer::close()
{
    // Close all client connections
    for (const auto socket : m_sockets) {
        if (socket) {
            socket->close();
            socket->deleteLater();
        }
    }
    m_sockets.clear();

    // Clean up NetworkClient wrappers
    for (const auto client : m_clients) {
        if (client) {
            client->deleteLater();
        }
    }
    m_clients.clear();

    m_server4->close();
    m_server6->close();
}

void SslServer::broadcast(const QByteArray &data)
{
    // Send data to all connected and encrypted clients
    for (const auto client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
        }
    }
}

void SslServer::onClientEncrypted()
{
    QSslSocket *socket = qobject_cast<QSslSocket *>(sender());
    if (!socket) {
        return;
    }

    // Check if this client is already wrapped
    bool alreadyRegistered = false;
    for (const auto client : m_clients) {
        if (client && client->socket() == socket) {
            alreadyRegistered = true;
            break;
        }
    }

    if (!alreadyRegistered) {
        // Create NetworkClient wrapper
        auto *client = new NetworkClientSsl(socket, this);
        m_clients.append(client);
        Q_EMIT clientConnected(client);
    }
}

void SslServer::onClientDisconnected()
{
    QSslSocket *socket = qobject_cast<QSslSocket *>(sender());
    if (!socket) {
        return;
    }

    // Remove from sockets list
    m_sockets.removeAll(socket);

    // Find and remove the associated NetworkClient
    const auto it = std::find_if(m_clients.begin(), m_clients.end(), [socket](NetworkClient *client) { return client && client->socket() == socket; });
    if (it != m_clients.end()) {
        Q_EMIT(*it)->disconnected();
        (*it)->deleteLater();
        m_clients.erase(it);
    }

    // Clean up socket
    socket->deleteLater();

    if (m_clients.isEmpty()) {
        Q_EMIT noClient();
    }
}

void SslServer::onClientReadyRead()
{
    QSslSocket *socket = qobject_cast<QSslSocket *>(sender());
    if (!socket) {
        return;
    }

    // Find the associated NetworkClient and let it handle the data
    for (const auto client : m_clients) {
        if (client && client->socket() == socket) {
            // The NetworkClient's readyRead signal will handle the data
            // Or you can process it here and emit appropriate signals
            break;
        }
    }
}

void SslServer::onClientSslErrors(const QList<QSslError> &errors)
{
    QSslSocket *socket = qobject_cast<QSslSocket *>(sender());
    if (!socket) {
        return;
    }

    bool hasSeriousIssues = false;

    // Log SSL errors
    for (const auto &error : errors) {
        const auto err = error.error();
        // [TODO] Remove in the future
        if (err != QSslError::SelfSignedCertificate && err != QSslError::SelfSignedCertificateInChain) {
            qWarning() << "SSL error:" << error.errorString();
            hasSeriousIssues = true;
        }
    }

    if (!hasSeriousIssues) {
        socket->ignoreSslErrors();
    }
}
