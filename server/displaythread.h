#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

#include <QThread>

#include "display.h"

class StreamSocket;

class DisplayThread : public QThread
{
    Q_OBJECT

public:
    explicit DisplayThread(Display *display, QObject *parent = nullptr);

    void addClient(NetworkClient *client);

Q_SIGNALS:
    void nowFree(DisplayThread *);

private:
    Display *m_display = nullptr;
};

#endif // DISPLAYTHREAD_H
