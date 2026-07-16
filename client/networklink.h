#ifndef NETWORKLINK_H
#define NETWORKLINK_H

#include <QDtls>
#include <QImage>
#include <QSslSocket>
#include <QTimer>
#include <QUdpSocket>

#include "../parser.h"

class FfmpegDecoder;

class NetworkLink : public QObject, Packets::Parser<NetworkLink>
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent = nullptr);
    ~NetworkLink() override;

    void processPacket(const Packets::ServerImage &img);
    void processPacket(const Packets::ClientResolution &res) { Q_UNUSED(res); }
    void processPacket(const Packets::ServerBrightness &brightness);
    void processPacket(const Packets::ServerStream &img);

Q_SIGNALS:
    void error(const QString &explanation);
    void opened();
    void closed();
    void imageReady(QImage);

public Q_SLOTS:
    void close();
    void connect(const QString &address, int port, const QString &clientName, const QString &protocolName);

private Q_SLOTS:
    void onConnected();
    void onDtlsDisconnected();
    void onSslDisconnected();
    void onDtlsDataAvailable();
    void onSslDataAvailable();
    void onConnectionTimeout();
    void onError(QAbstractSocket::SocketError error);
    void onSslError(const QSslError &error);

private:
    QString m_protocol;
    QByteArray m_buffer{};
    QSslSocket *m_sslSocket = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QDtls *m_dtls = nullptr;
    FfmpegDecoder *m_decoder = nullptr;
    QTimer m_inactivityTimer{};
    quint16 m_format = 0;
    quint32 m_width = 0;
    quint32 m_height = 0;
    quint32 m_stride = 0;
    qsizetype m_imageSize = 0;

    bool m_connectionReady = false;

    void connectDtls(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, int port);
    void connectSsl(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, int port);

    void checkSSLState();
};

#endif // NETWORKLINK_H
