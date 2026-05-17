#ifndef SERVER_H
#define SERVER_H

#include <QList>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

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
    void clientConnected(QTcpSocket *client);

public Q_SLOTS:
    void broadcast(const QByteArray &data);

private Q_SLOTS:
    void onNewConnection();
    void onClientDisconnected();

private:
    QTcpServer m_server{};
    QList<QTcpSocket *> m_clients{};
};

#endif // SERVER_H
