#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QMap>
#include <QObject>
#include <QQueue>
#include <QSet>

class NetworkClient;
class DisplayThread;
class StreamSocket;

/**
 * Schedules clients onto available virtual displays.
 *
 * A display thread is kept separated from the server event loop so capture and
 * encoding work cannot block connection management.
 */
class DisplayManager : public QObject
{
public:
    explicit DisplayManager(StreamSocket &ss, QObject *parent = nullptr);
    ~DisplayManager() override;

    /// Assigns a client to an existing display or creates a new display slot.
    bool registerClient(NetworkClient *client);

private:
    DisplayThread *m_thread = nullptr;
    StreamSocket &m_ss;
};

#endif // DISPLAYMANAGER_H
