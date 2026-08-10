#ifndef DTLSSERVER_H
#define DTLSSERVER_H

#include <QMap>
#include <QUdpSocket>
#include <QHostAddress>

#include "server.h"
#include "networkclient.h"

class QDtls;

class DtlsServer : public Server
{
    Q_OBJECT

public:
    explicit DtlsServer(QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port) override;
    void close() override;

    void broadcast(const QByteArray &data) override;

private Q_SLOTS:
    void onDatagramReceived();
    void onClientDisconnected();

private:
    QUdpSocket *m_socket = nullptr;
    QHostAddress m_address;
    quint16 m_port;

    // Active clients: peer key -> NetworkClientDtls*
    QMap<QString, NetworkClientDtls*> m_clients;
    // Pending handshakes: peer key -> QDtls*
    QMap<QString, QDtls*> m_pendingHandshakes;

    QString peerKey(const QHostAddress &addr, quint16 port) const;
    void removeClient(NetworkClientDtls *client);
};

#endif // DTLSSERVER_H
