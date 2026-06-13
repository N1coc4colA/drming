#ifndef DISPLAY_H
#define DISPLAY_H

#include <QObject>
#include <QTimer>

#include "displayreader.h"
#include "networkclient.h"

class Display : public QObject
{
    Q_OBJECT

public:
    explicit Display(const QString &connectorName, QObject *parent = nullptr);

Q_SIGNALS:
    void nowFree(Display *);

public Q_SLOTS:
    void setClient(NetworkClient *client);

private:
    DisplayReader m_reader;
    QTimer m_timer{};
    NetworkClient *m_client = nullptr;
    bool primaryFailureNotice = false;

private Q_SLOTS:
    void forward();
    void onConnected();
    void onDisconnected();
};

#endif // DISPLAY_H
