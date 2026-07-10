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
    explicit AvahiPublisher(QString serviceName, QString protocol, uint16_t port, QObject *parent = nullptr);
    ~AvahiPublisher() override;

    void start();
    void stop();

Q_SIGNALS:
    void stopped();
    void started();

private:
    QString m_serviceName{};
    QString m_protocol{};
    std::thread m_thread{};
    AvahiEntryGroup *m_group = nullptr;
    AvahiSimplePoll *m_poll = nullptr;
    AvahiClient *m_client = nullptr;
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_ready = false;

    const uint16_t m_port = -1;

    static void group_callback(const AvahiEntryGroup *g, AvahiEntryGroupState state, const AvahiPublisher *c);
    static void client_callback(AvahiClient *client, AvahiClientState state, AvahiPublisher *c);

Q_SIGNALS:
    void dispatchStopped();
    void dispatchStarted();
};

#endif // AVAHI_PUBLISHER_H
