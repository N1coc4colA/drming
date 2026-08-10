#include "displaythread.h"

DisplayThread::DisplayThread(Display *display, QObject *parent)
    : QThread(parent)
    , m_display(display)
{
    m_display->setParent(this);

    connect(m_display, &Display::nowFree, this, [this]() { Q_EMIT nowFree(this); }, Qt::QueuedConnection);
}

void DisplayThread::addClient(NetworkClient *client)
{
    qDebug() << "Adding client to thread";
    m_display->addClient(client);
}
