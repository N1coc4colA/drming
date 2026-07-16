#ifndef SSLSERVER_H
#define SSLSERVER_H

#include <QSslConfiguration>
#include <QSslSocket>

#include "server.h"

class SslServerImpl;

class SslServer : public Server
{
    Q_OBJECT

public:
    explicit SslServer(QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port) override;
    void close() override;

    void broadcast(const QByteArray &data) override;

private Q_SLOTS:
    void onClientDisconnected();
    void onClientReadyRead();
    void onClientEncrypted();
    void onClientSslErrors(const QList<QSslError> &errors);

private:
    QSslConfiguration m_sslConfig{};
    QList<QSslSocket *> m_sockets{};
    SslServerImpl *m_server = nullptr;

    friend class SslServerImpl;
};

#endif
