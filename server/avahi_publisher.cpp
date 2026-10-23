#include "avahi_publisher.h"

#include <QCoreApplication>
#include <QDebug>

#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>

#include <net/if.h>

void AvahiPublisher::group_callback(const AvahiEntryGroup *g, const AvahiEntryGroupState state, const AvahiPublisher *c)
{
    Q_UNUSED(g);

    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED: {
        qCritical() << "Service established";
        break;
    }
    case AVAHI_ENTRY_GROUP_FAILURE: {
        qCritical() << "Group failure";
        avahi_threaded_poll_quit(c->m_poll);
        break;
    }
    case AVAHI_ENTRY_GROUP_UNCOMMITED: {
        qDebug() << "Avahi group is uncommitted.";
        break;
    }
    case AVAHI_ENTRY_GROUP_REGISTERING: {
        qDebug() << "Avahi group is registering.";
        break;
    }
    case AVAHI_ENTRY_GROUP_COLLISION: {
        qDebug() << "Avahi group collided.";
        break;
    }
    default: {
        break;
    }
    }
}

void AvahiPublisher::client_callback(AvahiClient *client, const AvahiClientState state, AvahiPublisher *c)
{
    if (state != AVAHI_CLIENT_S_RUNNING) {
        return;
    }

    const auto ifaceName = c->m_iface.name().toLocal8Bit();
    auto iface = ifaceName.isEmpty() ? AVAHI_IF_UNSPEC : if_nametoindex(ifaceName.constData());
    if (!ifaceName.isEmpty() && iface == 0) {
        qCritical() << "Interface not found for mDNS:" << c->m_iface.name();
        return;
    }

    c->m_group = avahi_entry_group_new(client, reinterpret_cast<AvahiEntryGroupCallback>(&group_callback), c);
    const auto ret = avahi_entry_group_add_service(
        c->m_group,
        iface,
        c->m_isIpV6 ? AVAHI_PROTO_INET6 : AVAHI_PROTO_INET,
        (AvahiPublishFlags) 0,
        c->m_serviceName.toLocal8Bit(),
        c->m_protocol.toLocal8Bit(),
        nullptr,
        nullptr,
        c->m_port,
        nullptr);

    if (ret < 0) {
        qCritical() << "Avahi group creation error: " << avahi_strerror(ret);
    } else {
        avahi_entry_group_commit(c->m_group);
    }
}

AvahiPublisher::AvahiPublisher(const QNetworkInterface &iface, const bool isIpV6, QString serviceName, QString protocol, const uint16_t port, QObject *parent)
    : QObject(parent)
    , m_iface(iface)
    , m_isIpV6(isIpV6)
    , m_serviceName(std::move(serviceName))
    , m_protocol(std::move(protocol))
    , m_port(port)
{
    connect(this, &AvahiPublisher::started, [] { qInfo() << "Service publishing started."; });
    connect(this, &AvahiPublisher::stopped, [] { qInfo() << "Service publishing stopped."; });
    connect(this, &AvahiPublisher::dispatchStopped, this, &AvahiPublisher::stopped, Qt::QueuedConnection);
    connect(this, &AvahiPublisher::dispatchStarted, this, &AvahiPublisher::started, Qt::QueuedConnection);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &AvahiPublisher::stop);
}

AvahiPublisher::~AvahiPublisher()
{
    stop();
}

void AvahiPublisher::start()
{
    if (m_running.exchange(true)) {
        return;
    }

    m_poll = avahi_threaded_poll_new();
    if (!m_poll) {
        qCritical() << "Avahi threaded poll creation failed.";
        m_running = false;
        return;
    }

    int error = 0;
    m_client = avahi_client_new(
        avahi_threaded_poll_get(m_poll),
        (AvahiClientFlags) 0,
        reinterpret_cast<AvahiClientCallback>(&AvahiPublisher::client_callback),
        this,
        &error);
    if (!m_client) {
        qCritical() << "Avahi Client creation error: " << avahi_strerror(error);
        avahi_threaded_poll_free(m_poll);
        m_poll = nullptr;
        m_running = false;
        return;
    }

    if (avahi_threaded_poll_start(m_poll) < 0) {
        qCritical() << "Failed to start Avahi threaded poll.";
        avahi_client_free(m_client);
        m_client = nullptr;
        avahi_threaded_poll_free(m_poll);
        m_poll = nullptr;
        m_running = false;
        return;
    }

    Q_EMIT dispatchStarted();
}

void AvahiPublisher::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }

    // Stop the poll loop first, thread-safe.
    if (m_poll) {
        avahi_threaded_poll_stop(m_poll);
    }

    // Free resources.
    if (m_group) {
        avahi_entry_group_free(m_group);
        m_group = nullptr;
    }
    if (m_client) {
        avahi_client_free(m_client);
        m_client = nullptr;
    }
    if (m_poll) {
        avahi_threaded_poll_free(m_poll);
        m_poll = nullptr;
    }

    Q_EMIT dispatchStopped();
}
