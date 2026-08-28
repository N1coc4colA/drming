#ifndef SERVER_H
#define SERVER_H

#include <QHostAddress>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QUdpSocket>

#include "networkclient.h"

class QDtls;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

    bool listen(const QHostAddress &address4, const QHostAddress &address6, quint16 port = 0);
    void close();

    [[nodiscard]] virtual bool hasClient() const { return !m_clients.isEmpty(); }

Q_SIGNALS:
    void noClient();
    void clientConnected(NetworkClient *client);

public Q_SLOTS:
    void broadcast(const QByteArray &data);

private Q_SLOTS:
    void onDatagramReceived4();
    void onDatagramReceived6();
    void onClientDisconnected();

private:
    QPair<QUdpSocket, QMutex> m_socket4{};
    QPair<QUdpSocket, QMutex> m_socket6{};

    // Active clients: peer key -> NetworkClientDtls*
    QMap<QString, NetworkClient *> m_clients;
    // Pending handshakes: peer key -> QDtls*
    QMap<QString, QDtls *> m_pendingHandshakes;

    QString peerKey(const QHostAddress &addr, quint16 port) const;
    void removeClient(NetworkClient *client);

    void processSocketPendings(QPair<QUdpSocket, QMutex> &pair);
};

#endif // SERVER_H
