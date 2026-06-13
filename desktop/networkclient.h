#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QAbstractSocket>
#include <QHostAddress>

class NetworkClient : public QObject
{
    Q_OBJECT
public:
    explicit NetworkClient(QObject *parent = nullptr) : QObject(parent) {}
    ~NetworkClient() override = default;

    // Write bytes to the client. Returns number of bytes written or -1 on error.
    virtual qint64 write(const QByteArray &data) = 0;
    virtual QAbstractSocket::SocketState state() const = 0;
    virtual QHostAddress peerAddress() const { return QHostAddress(); }
    virtual quint16 peerPort() const { return 0; }

Q_SIGNALS:
    void disconnected();
};

#endif // NETWORKCLIENT_H
