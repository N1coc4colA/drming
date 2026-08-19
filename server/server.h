#ifndef SERVER_H
#define SERVER_H

#include <QObject>

#include "networkclient.h"

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);

    virtual bool listen(const QHostAddress &address4, const QHostAddress &address6, quint16 port = 0) = 0;
    virtual void close() = 0;

    [[nodiscard]] virtual bool hasClient() const { return !m_clients.isEmpty(); }

Q_SIGNALS:
    void noClient();
    void clientConnected(NetworkClient *client);

public Q_SLOTS:
    virtual void broadcast(const QByteArray &data) = 0;

protected:
    QList<NetworkClient *> m_clients{};

    static bool loadServerCertsConfig(QSslConfiguration &outConfig, const QString &protocol);
};

#endif // SERVER_H
