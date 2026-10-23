#ifndef AVAHI_PUBLISHER_H
#define AVAHI_PUBLISHER_H

#include <QNetworkInterface>

#include <atomic>

#include <avahi-client/client.h>
#include <avahi-common/thread-watch.h>

typedef struct AvahiEntryGroup AvahiEntryGroup;
typedef struct AvahiSimplePoll AvahiSimplePoll;

/**
 * Publishes the server's stream endpoint through Avahi in a worker thread.
 *
 * The Qt signals report readiness while the Avahi poll loop remains isolated
 * from the main event loop.
 */
class AvahiPublisher : public QObject
{
    Q_OBJECT

public:
    explicit AvahiPublisher(const QNetworkInterface &iface, bool isIpV6, QString serviceName, QString protocol, uint16_t port, QObject *parent = nullptr);
    ~AvahiPublisher() override;

    /// Starts the Avahi client and advertises the configured service.
    void start();
    /// Stops the poll loop and removes the service advertisement.
    void stop();

Q_SIGNALS:
    void stopped();
    void started();

private:
    QNetworkInterface m_iface;
    const bool m_isIpV6;
    QString m_serviceName{};
    QString m_protocol{};
    AvahiEntryGroup *m_group = nullptr;
    AvahiThreadedPoll *m_poll = nullptr;
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
