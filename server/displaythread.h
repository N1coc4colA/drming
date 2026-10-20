#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

#include <QThread>

#include "display.h"

class StreamSocket;

/**
 * Owns a Display and serializes capture work away from the server thread.
 *
 * A thread announces when its display becomes available so DisplayManager can
 * assign another client without directly manipulating thread affinity.
 */
class DisplayThread : public QThread
{
    Q_OBJECT

public:
    explicit DisplayThread(Display *display, QObject *parent = nullptr);

    /// Queues a client assignment on the display's owning thread.
    void addClient(NetworkClient *client);

Q_SIGNALS:
    void nowFree(DisplayThread *);

private:
    Display *m_display = nullptr;
};

#endif // DISPLAYTHREAD_H
