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
    ~DisplayThread() override;

    /// Queues a client assignment on the display's owning thread.
    /// This method must be called using QMetaObject::invokeMethod to make it
    /// thread-safe.
    void addClient(NetworkClient *client);

Q_SIGNALS:
    /// Emitted when all clients have disconnected and the thread is free.
    void nowFree(DisplayThread *);

private:
    Display *m_display = nullptr;
};

#endif // DISPLAYTHREAD_H
