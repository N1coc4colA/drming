#include "mdnsmanager.h"

#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QJniObject>

#include "native.h"
#else
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <set>
#include <thread>
#endif

#ifndef Q_OS_ANDROID
class AvahiDiscoverer
{
public:
    explicit AvahiDiscoverer(MdnsManager &manager)
        : m_manager(manager)
    {
        QObject::connect(&m_manager, &MdnsManager::dispatchServiceFound, &m_manager, &MdnsManager::onServiceFound, Qt::QueuedConnection);
        QObject::connect(&m_manager, &MdnsManager::dispatchServiceLost, &m_manager, &MdnsManager::onServiceLost, Qt::QueuedConnection);
        QObject::connect(&m_manager, &MdnsManager::dispatchServiceResolved, &m_manager, &MdnsManager::onServiceResolved, Qt::QueuedConnection);
    }

    void start()
    {
        if (m_running) {
            return;
        }

        m_running = true;

        m_thread = std::thread([this]() {
            // All Avahi objects created here, on the poll thread
            if (!(m_poll = avahi_simple_poll_new())) {
                qCritical() << "Failed to create simple poll object.";
                m_running = false;
                return;
            }

            int error = 0;
            m_client = avahi_client_new(avahi_simple_poll_get(m_poll),
                                        static_cast<AvahiClientFlags>(0),
                                        (AvahiClientCallback) client_callback,
                                        this,
                                        &error);
            if (!m_client) {
                qWarning() << "Failed to create client: " << avahi_strerror(error);
                avahi_simple_poll_free(m_poll);
                m_poll = nullptr;
                m_running = false;
                return;
            }

            m_sb = avahi_service_browser_new(m_client,
                                             AVAHI_IF_UNSPEC,
                                             AVAHI_PROTO_UNSPEC,
                                             "_drming._tcp",
                                             nullptr,
                                             static_cast<AvahiLookupFlags>(0),
                                             (AvahiServiceBrowserCallback) browse_callback,
                                             this); // pass 'this', not m_client
            if (!m_sb) {
                qCritical() << "Failed to create service browser: " << avahi_strerror(avahi_client_errno(m_client));
                avahi_client_free(m_client);
                m_client = nullptr;
                avahi_simple_poll_free(m_poll);
                m_poll = nullptr;
                m_running = false;
                return;
            }

            avahi_simple_poll_loop(m_poll);

            avahi_service_browser_free(m_sb);
            m_sb = nullptr;
            avahi_client_free(m_client);
            m_client = nullptr;
            avahi_simple_poll_free(m_poll);
            m_poll = nullptr;
            m_running = false;
        });
    }

    void stop()
    {
        if (!m_running || !m_poll) {
            return;
        }

        avahi_simple_poll_quit(m_poll);
        if (m_thread.joinable() && std::this_thread::get_id() != m_thread.get_id()) {
            m_thread.join();
        }
    }

    ~AvahiDiscoverer() { stop(); }

private:
    std::set<AvahiServiceResolver *> m_pendingResolvers{};
    std::thread m_thread{};
    MdnsManager &m_manager;
    AvahiSimplePoll *m_poll = nullptr;
    AvahiClient *m_client = nullptr;
    AvahiServiceBrowser *m_sb = nullptr;
    std::atomic<bool> m_running = false;
    bool m_ready = false;

    static void resolve_callback(AvahiServiceResolver *r,
                                 AvahiIfIndex interface,
                                 AvahiProtocol protocol,
                                 AvahiResolverEvent event,
                                 const char *name,
                                 const char *type,
                                 const char *domain,
                                 const char *host_name,
                                 const AvahiAddress *address,
                                 uint16_t port,
                                 AvahiStringList *txt,
                                 AvahiLookupResultFlags flags,
                                 AvahiDiscoverer *c)
    {
        AvahiClient *client = avahi_service_resolver_get_client(r);

        switch (event) {
        case AVAHI_RESOLVER_FAILURE: {
            qWarning() << "(Resolver) Failed to resolve service '" << name << "' of type '" << type << "' in domain '" << domain
                       << "': " << avahi_strerror(avahi_client_errno(client));
            break;
        }
        case AVAHI_RESOLVER_FOUND: {
            std::array<char, AVAHI_ADDRESS_STR_MAX> a{};
            avahi_address_snprint(a.data(), a.size(), address);
            Q_EMIT c->m_manager.dispatchServiceResolved(name, host_name, QString(a.data()), port);
            break;
        }
        default: {
            break;
        }
        }
        c->m_pendingResolvers.erase(r);
        avahi_service_resolver_free(r);
    }

    static void browse_callback(AvahiServiceBrowser *b,
                                AvahiIfIndex interface,
                                AvahiProtocol protocol,
                                AvahiBrowserEvent event,
                                const char *name,
                                const char *type,
                                const char *domain,
                                AvahiLookupResultFlags flags,
                                AvahiDiscoverer *c)
    {
        switch (event) {
        case AVAHI_BROWSER_FAILURE: {
            qCritical() << "(Browser) " << avahi_strerror(avahi_client_errno(c->m_client));
            return;
        }
        case AVAHI_BROWSER_NEW: {
            Q_EMIT c->m_manager.dispatchServiceFound(name, type);

            auto resolver = avahi_service_resolver_new(c->m_client,
                                                       interface,
                                                       protocol,
                                                       name,
                                                       type,
                                                       domain,
                                                       AVAHI_PROTO_UNSPEC,
                                                       static_cast<AvahiLookupFlags>(0),
                                                       (AvahiServiceResolverCallback) resolve_callback,
                                                       c);

            /* Resolve the newly discovered service */
            if (resolver) {
                c->m_pendingResolvers.insert(resolver);
            } else {
                qWarning() << "Failed to resolve service '" << name << "': " << avahi_strerror(avahi_client_errno(c->m_client));
            }
            break;
        }
        case AVAHI_BROWSER_REMOVE: {
            Q_EMIT c->m_manager.dispatchServiceLost(name);
            break;
        }
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED: {
            qInfo() << "(Browser) " << (event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
        }
        }
    }

    static void client_callback(AvahiClient *client, AvahiClientState state, AvahiDiscoverer *c)
    {
        if (state == AVAHI_CLIENT_FAILURE) {
            qCritical() << "Server connection failure: " << avahi_strerror(avahi_client_errno(client));
            //avahi_simple_poll_quit(c->m_poll);
        }
    }
};
#endif

MdnsManager::MdnsManager(QObject *parent)
    : QObject(parent)
#ifndef Q_OS_ANDROID
    , m_avahi(new AvahiDiscoverer(*this))
#endif
{
    assert(!m_instance);

    m_instance = this;
}

#ifndef Q_OS_ANDROID
MdnsManager::~MdnsManager()
{
    delete m_avahi;
}
#endif

MdnsManager *MdnsManager::m_instance = nullptr;

MdnsManager &MdnsManager::instance()
{
    return *m_instance;
}

void MdnsManager::startDiscovery()
{
#ifdef Q_OS_ANDROID
    if (!createNativeObject_MdnsHelper(m_javaHelper)) {
        return;
    }

    m_javaHelper.callMethod<void>("startDiscovery");
#else
    m_avahi->start();
#endif
}

void MdnsManager::stopDiscovery()
{
#ifdef Q_OS_ANDROID
    if (m_javaHelper.isValid()) {
        m_javaHelper.callMethod<void>("stopDiscovery");
    }
#else
    m_avahi->stop();
#endif
}

void MdnsManager::onServiceFound(const QString &name, const QString &type)
{
    qDebug() << "Found:" << name << type;

    const ServiceInfo info{name, type, "", "", -1};
    m_services[name] = info;

    Q_EMIT serviceFound(info);
    Q_EMIT countChanged(count());
}

void MdnsManager::onServiceLost(const QString &name)
{
    qDebug() << "Lost:" << name;

    m_services.remove(name);

    Q_EMIT serviceLost(name);
    Q_EMIT countChanged(count());
}

void MdnsManager::onServiceResolved(const QString &name, const QString &host, const QString &ip, const int port)
{
    qDebug() << "Resolved:" << name << host << ip << port;

    if (m_services.contains(name)) {
        m_services[name].host = host;
        m_services[name].ip = ip;
        m_services[name].port = port;

        Q_EMIT serviceResolved(m_services[name]);
    }
}
