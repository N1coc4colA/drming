#include "server.h"

#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>

#include "../certificatesupport.h"
#include "parameters.h"

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
    const auto serverCert = openCertificate(Parameters::instance.serverCertPath);
    const auto serverKey = openKey(Parameters::instance.serverKeyPath);
    if (serverKey.isNull() || serverCert.isNull()) {
        return false;
    }

    auto conf = QSslConfiguration::defaultConfiguration();
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

    connect(socket, &QSslSocket::disconnected, this, &Server::onClientDisconnected);
    connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QSslSocket::sslErrors, [socket, this](const QList<QSslError> &errors) { onSslErrors(socket, errors); });
    connect(socket, &QSslSocket::encrypted, [socket, this]() {
        qInfo() << "SSL encrypted with" << socket->peerAddress().toString() << socket->peerPort();
    });

    m_clients.append(socket);
    Q_EMIT clientConnected(socket);
}

void Server::onSslErrors(QSslSocket *socket, const QList<QSslError> &errors)
{
    const auto serverCert = socket->peerCertificate();
    if (serverCert.isNull()) {
        qWarning() << "Server provided no certificate, aborting.";
        socket->abort();
        return;
    }

    const auto trustedCerts = QSslCertificate::fromPath(Parameters::instance.trustedCertsPath, QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard);
    if (trustedCerts.isEmpty()) {
        qWarning() << "No trusted certificates found.";
    } else {
        qInfo() << "Loaded" << trustedCerts.size() << "trusted certificate(s).";
    }

    if (!trustedCerts.contains(serverCert)) {
        qWarning() << "Server certificate is not in the trusted list, aborting.";
        socket->abort();
        return;
    }

    // Cert is in our allowlist — we only tolerate hostname mismatch errors.
    // Chain/expiry/revocation errors are still fatal.
    QList<QSslError> ignorable{};
    for (const QSslError &e : errors) {
        if (e.error() == QSslError::HostNameMismatch) {
            ignorable.append(e);
        } else {
            qWarning() << "Unacceptable SSL error:" << e.errorString();
        }
    }

    if (ignorable.size() == errors.size()) {
        socket->ignoreSslErrors(ignorable);
    } else {
        socket->abort();
    }
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
