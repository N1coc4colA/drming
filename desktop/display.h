#ifndef DISPLAY_H
#define DISPLAY_H

#include <QObject>
#include <QTimer>

#include "displayreader.h"

class QTcpSocket;

class Display : public QObject
{
    Q_OBJECT

public:
    explicit Display(const QString &connectorName, QObject *parent = nullptr);

public Q_SLOTS:
    void setClient(QTcpSocket *client);

Q_SIGNALS:
    void nowFree(Display *);

private:
    DisplayReader m_reader;
    QTimer m_timer{};
    QTcpSocket *m_client = nullptr;
    bool primaryFailureNotice = false;

private Q_SLOTS:
    void forward();
    void onConnected();
    void onDisconnected();
};

#endif // DISPLAY_H
