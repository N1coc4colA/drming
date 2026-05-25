#include "server.h"

#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>

Server::Server(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QSslServer::pendingConnectionAvailable, this, [this]() {
        while (m_server.hasPendingConnections()) {
            auto *socket = qobject_cast<QSslSocket *>(m_server.nextPendingConnection());
            if (!socket)
                continue;
            onNewConnection(socket);
        }
    });
    connect(&m_server, &QSslServer::acceptError, [](const QAbstractSocket::SocketError socketError) {
        qWarning() << "Accept error occurred:" << socketError;
    });
    connect(&m_server, &QSslServer::sslErrors, [](const QSslSocket *socket, const QList<QSslError> &errors) {
        qWarning() << "SSL errors from" << socket->peerAddress().toString();
        for (const QSslError &e : errors)
            qWarning() << "  " << e.errorString();
    });
}

bool Server::loadServerSslConfig(QSslConfiguration &outConfig)
{
    const QString certPath = QStringLiteral("./certs/server.crt");
    const QString keyPath = QStringLiteral("./certs/server.key");

    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open server certificate:" << certPath;
        return false;
    }
    const QByteArray certData = certFile.readAll();

    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open server private key:" << keyPath;
        return false;
    }
    const QByteArray keyData = keyFile.readAll();

    const QSslCertificate serverCert(certData);
    if (serverCert.isNull()) {
        qCritical() << "Invalid certificate:" << certPath;
        return false;
    }
    if (serverCert.isBlacklisted()) {
        qCritical() << "Blacklisted certificate:" << certPath;
        return false;
    }

    QSslKey serverKey(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (serverKey.isNull())
        serverKey = QSslKey(keyData, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
    if (serverKey.isNull()) {
        qCritical() << "Failed to parse server private key:" << keyPath;
        return false;
    }

    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setLocalCertificate(serverCert);
    conf.setPrivateKey(serverKey);
    outConfig = conf;
    return true;
}

bool Server::listen(const QHostAddress &address, const quint16 port)
{
    QSslConfiguration sslConfig;
    if (loadServerSslConfig(sslConfig)) {
        m_server.setSslConfiguration(sslConfig);
    } else {
        qWarning() << "SSL config not loaded; connections will not be encrypted.";
    }

    const bool success = m_server.listen(address, port);
    if (!success)
        qCritical() << "Failed to listen:" << m_server.errorString();
    else
        qInfo() << "Exposing service on:" << address.toString() << ':' << port;

    return success;
}

void Server::close()
{
    for (QSslSocket *client : m_clients) {
        if (client) {
            client->disconnectFromHost();
            client->deleteLater();
        }
    }
    m_clients.clear();
    m_server.close();
}

void Server::broadcast(const QByteArray &data)
{
    for (QSslSocket *client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState)
            client->write(data);
    }
}

void Server::onNewConnection(QSslSocket *socket)
{
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    connect(socket, &QSslSocket::encrypted, [socket]() { qInfo() << "SSL encrypted with" << socket->peerAddress().toString() << socket->peerPort(); });
    connect(socket, &QSslSocket::disconnected, this, &Server::onClientDisconnected);
    connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);

    m_clients.append(socket);
    Q_EMIT clientConnected(socket);
}

void Server::onClientDisconnected()
{
    auto *client = qobject_cast<QSslSocket *>(sender());
    if (!client)
        return;

    m_clients.removeAll(client);
    if (m_clients.isEmpty())
        Q_EMIT noClient();
}
