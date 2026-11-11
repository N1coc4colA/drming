#include "displaythread.h"

#include <QPointer>

DisplayThread::DisplayThread(Display *display, QObject *parent)
    : QThread(parent)
    , m_display(display)
{
    connect(m_display, &Display::nowFree, this, [this]() { Q_EMIT nowFree(this); }, Qt::DirectConnection);
}

DisplayThread::~DisplayThread()
{
    quit();
    wait();
    delete m_display;
}

void DisplayThread::addClient(NetworkClient *client)
{
    qDebug() << "Adding client to thread";
    const QPointer<NetworkClient> guardedClient(client);
    QMetaObject::invokeMethod(
        m_display,
        [display = m_display, guardedClient]() {
            if (guardedClient) {
                display->addClient(guardedClient);
            }
        },
        Qt::QueuedConnection
    );
}
