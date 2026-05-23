#ifndef SERVER_H
#define SERVER_H

#include <QList>
#include <QObject>
#include <QSslServer>
#include <QSslSocket>

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
    void clientConnected(QSslSocket *client);

public Q_SLOTS:
    void broadcast(const QByteArray &data);

private Q_SLOTS:
    void onNewConnection(QSslSocket *socket);
    void onClientDisconnected();

private:
    QSslServer m_server{};
    QList<QSslSocket *> m_clients{};

    static bool loadServerSslConfig(QSslConfiguration &outConfig);
};

#endif
