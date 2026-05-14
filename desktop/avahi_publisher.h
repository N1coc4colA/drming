#ifndef AVAHI_PUBLISHER_H
#define AVAHI_PUBLISHER_H

#include <QObject>

#include <atomic>
#include <thread>

#include <avahi-client/client.h>

typedef struct AvahiEntryGroup AvahiEntryGroup;
typedef struct AvahiSimplePoll AvahiSimplePoll;

class AvahiPublisher : public QObject
{
    Q_OBJECT

public:
    explicit AvahiPublisher(const QString &serviceName, const QString &protocol, const uint16_t port, QObject *parent = nullptr);
    ~AvahiPublisher();

    void start();
    void stop();

Q_SIGNALS:
    void stopped();
    void started();

private:
    QString m_serviceName;
    QString m_protocol;
    std::thread m_thread{};
    AvahiEntryGroup *m_group = nullptr;
    AvahiSimplePoll *m_poll = nullptr;
    AvahiClient *m_client = nullptr;
    std::atomic<bool> m_running = false;
    const uint16_t m_port;

    static void group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AvahiPublisher *c);
    static void client_callback(AvahiClient *client, AvahiClientState state, AvahiPublisher *c);

Q_SIGNALS:
    void dispatchStopped();
    void dispatchStarted();
};

#endif // AVAHI_PUBLISHER_H
