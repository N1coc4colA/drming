#ifndef SERVER_H
#define SERVER_H

#include <QList>
#include <QObject>
#include <QUdpSocket>
#include <QSslConfiguration>
#include <QHostAddress>
#include <QMap>
#include <QPair>

#include "networkclient.h"

class QDtls;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

    bool listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);
    void close();

    inline bool hasClient() const { return !m_clients.isEmpty(); }

Q_SIGNALS:
    void noClient();
    void clientConnected(NetworkClient *client);

public Q_SLOTS:
    void broadcast(const QByteArray &data);

private Q_SLOTS:
    void onDatagramReceived();

private:
    QUdpSocket m_socket{};
    QList<NetworkClient *> m_clients{};
    QMap<QString, QDtls *> m_dtlsMap{};

    static bool loadServerSslConfig(QSslConfiguration &outConfig);
};

#endif
