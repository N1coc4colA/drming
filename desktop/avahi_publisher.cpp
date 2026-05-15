#include "avahi_publisher.h"

#include <QDebug>

#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>

void AvahiPublisher::group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AvahiPublisher *c)
{
    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED: {
        qCritical() << "Service established";
        break;
    }
    case AVAHI_ENTRY_GROUP_FAILURE: {
        qCritical() << "Group failure";
        avahi_simple_poll_quit(c->m_poll);
        break;
    }
    default: {
        break;
    }
    }
}

void AvahiPublisher::client_callback(AvahiClient *client, AvahiClientState state, AvahiPublisher *c)
{
    if (state != AVAHI_CLIENT_S_RUNNING) {
        return;
    }

    c->m_group = avahi_entry_group_new(client, (AvahiEntryGroupCallback) &group_callback, c);
    const int ret = avahi_entry_group_add_service(c->m_group,
                                                  AVAHI_IF_UNSPEC,
                                                  AVAHI_PROTO_UNSPEC,
                                                  (AvahiPublishFlags) 0,
                                                  c->m_serviceName.toLocal8Bit(),
                                                  c->m_protocol.toLocal8Bit(),
                                                  nullptr,
                                                  nullptr,
                                                  c->m_port,
                                                  nullptr);

    if (ret < 0) {
        qCritical() << "Add error: " << avahi_strerror(ret);
    } else {
        avahi_entry_group_commit(c->m_group);
    }
}

AvahiPublisher::AvahiPublisher(const QString &serviceName, const QString &protocol, const uint16_t port, QObject *parent)
    : QObject(parent)
    , m_serviceName(serviceName)
    , m_protocol(protocol)
    , m_port(port)
{
    connect(this, &AvahiPublisher::dispatchStopped, this, &AvahiPublisher::stopped, Qt::QueuedConnection);
    connect(this, &AvahiPublisher::dispatchStarted, this, &AvahiPublisher::started, Qt::QueuedConnection);
}

AvahiPublisher::~AvahiPublisher()
{
    stop();
}

void AvahiPublisher::start()
{
    if (m_running) {
        return;
    }

    m_thread = std::thread([this]() {
        m_poll = avahi_simple_poll_new();
        int error = 0;
        AvahiClient *client = avahi_client_new(avahi_simple_poll_get(m_poll),
                                               (AvahiClientFlags) 0,
                                               (AvahiClientCallback) &client_callback,
                                               this,
                                               &error);
        if (!client) {
            qCritical() << "Client error: " << avahi_strerror(error);
            return;
        }

        m_running = true;
        Q_EMIT dispatchStarted();
        avahi_simple_poll_loop(m_poll);
    });
}

void AvahiPublisher::stop()
{
    if (!m_running) {
        return;
    }

    if (m_poll) {
        avahi_simple_poll_quit(m_poll);
        m_poll = nullptr;
    }

    if (m_thread.joinable() && std::this_thread::get_id() != m_thread.get_id()) {
        m_thread.join();
    }

    m_running = false;
    Q_EMIT dispatchStopped();
}
