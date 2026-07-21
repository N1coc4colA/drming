#ifndef DTLSSERVER_H
#define DTLSSERVER_H

#include <QMap>
#include <QUdpSocket>

#include "server.h"

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

private:
    QUdpSocket m_socket{};
    QSet<QPair<QHostAddress, quint16>> m_peerSockets{};
    QMap<QString, QDtls *> m_dtlsMap{};
};

#endif
