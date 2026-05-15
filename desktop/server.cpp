#include "server.h"

Server::Server(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
    connect(&m_server, &QTcpServer::acceptError, [](const QAbstractSocket::SocketError socketError) {
        qInfo() << "Acceptation error occured: " << socketError;
    });
}

bool Server::listen(const QHostAddress &address, const quint16 port)
{
    const bool success = m_server.listen(address, port);
    if (!success) {
        qCritical() << "Failed to listen: " << m_server.errorString();
    } else {
        qInfo() << "Exposing service on: " << address.toString() << ':' << port;
    }

    return success;
}

void Server::close()
{
    for (QTcpSocket *client : m_clients) {
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
    for (QTcpSocket *client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
        }
    }
}

void Server::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *client = m_server.nextPendingConnection();
        if (!client) {
            continue;
        }

        m_clients.append(client);
        if (m_clients.size() == 1) {
            Q_EMIT clientConnected();
        }

        connect(client, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);
        connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);

        client->setSocketOption(QAbstractSocket::LowDelayOption, 1);  // disables Nagle
        client->setSocketOption(QAbstractSocket::KeepAliveOption, 1); // optional: enable keepalive
    }
}

void Server::onClientDisconnected()
{
    auto *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) {
        return;
    }

    m_clients.removeAll(client);
    if (m_clients.isEmpty()) {
        Q_EMIT noClient();
    }
}
