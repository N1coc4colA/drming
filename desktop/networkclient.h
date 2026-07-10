#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QHostAddress>

class QUdpSocket;
class QDtls;

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    NetworkClient(QHostAddress addr, quint16 port, QDtls *dtls, QUdpSocket *socket, QObject *parent = nullptr);
    ~NetworkClient() override = default;

    qint64 write(const QByteArray &data);

    [[nodiscard]] QAbstractSocket::SocketState state() const { return QAbstractSocket::ConnectedState; }
    [[nodiscard]] QHostAddress peerAddress() const { return m_addr; }
    [[nodiscard]] quint16 peerPort() const { return m_port; }
    QDtls *dtls() { return m_dtls; }

Q_SIGNALS:
    void disconnected();

private:
    QHostAddress m_addr{};
    quint16 m_port = 0;
    QDtls *m_dtls = nullptr;
    QUdpSocket *m_socket = nullptr;
};

#endif // NETWORKCLIENT_H
