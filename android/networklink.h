#ifndef NETWORKLINK_H
#define NETWORKLINK_H

#include <QImage>
#include <QTimer>
#include <QUdpSocket>
#include <QDtls>

#include "../parser.h"

class NetworkLink : public QObject, Packets::Parser<NetworkLink>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent = nullptr);
    ~NetworkLink() override;

    void processPacket(const Packets::ServerImage &img);
    void processPacket(const Packets::ClientResolution &res) { Q_UNUSED(res); }
    void processPacket(const Packets::ServerBrightness &brightness);

Q_SIGNALS:
    void error(const QString &explanation);
    void opened();
    void closed();
    void imageReady(QImage);

public Q_SLOTS:
    void close();
    void connect(const QString &address, int port, const QString &clientName);

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onDataAvailable();
    void onConnectionTimeout();

private:
    QByteArray m_buffer{};
    QUdpSocket *m_udpSocket = nullptr;
    QDtls *m_dtls = nullptr;
    QTimer m_inactivityTimer{};
    quint16 m_format = 0;
    quint32 m_width = 0;
    quint32 m_height = 0;
    quint32 m_stride = 0;
    qsizetype m_imageSize = 0;

    bool m_connectionReady = false;
};

#endif // NETWORKLINK_H
