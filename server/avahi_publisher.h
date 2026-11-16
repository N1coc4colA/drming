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
    explicit AvahiPublisher(
        const QNetworkInterface &iface,
        bool isIpV6,
        QString serviceName,
        uint16_t port,
        QObject *parent = nullptr
    );
    ~AvahiPublisher() override;

    /// Starts the Avahi client and advertises the configured service.
    void start();
    /// Stops the poll loop and removes the service advertisement.
    void stop();

Q_SIGNALS:
    /// Emitted when mDNS service advertising has stopped.
    void stopped();
    /// Emitted when mDNS service advertising has successfully started.
    void started();

private:
    /// Network interface on which the service is advertised.
    QNetworkInterface m_iface;
    /// Whether the advertised address is IPv6.
    const bool m_isIpV6;
    /// Advertised service name.
    QString m_serviceName{};
    AvahiEntryGroup *m_group = nullptr;
    AvahiThreadedPoll *m_poll = nullptr;
    AvahiClient *m_client = nullptr;
    /// Whether the publisher thread is currently active.
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_ready = false;

    /// Network port on which the service is listening.
    const uint16_t m_port;

    static void group_callback(const AvahiEntryGroup *g, AvahiEntryGroupState state, const AvahiPublisher *c);
    static void client_callback(AvahiClient *client, AvahiClientState state, AvahiPublisher *c);

Q_SIGNALS:
    /// Internal cross-thread signal dispatched when advertising stops.
    void dispatchStopped();
    /// Internal cross-thread signal dispatched when advertising starts.
    void dispatchStarted();
};

#endif // AVAHI_PUBLISHER_H
