#include "mdns.h"

#include <QCoreApplication>
#include <QDebug>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>

#include <set>
#include <thread>

#include <ctrack.hpp>

namespace Platform {

class AvahiDiscoverer
{
    struct AvahiDiscovererArg
    {
        AvahiDiscoverer *discoverer;
        QString protocol;
    };

public:
    explicit AvahiDiscoverer(Mdns &manager)
        : m_manager(manager)
    {
        QObject::connect(&m_manager, &Mdns::dispatchServiceFound, &m_manager, &Mdns::onServiceFound, Qt::QueuedConnection);
        QObject::connect(&m_manager, &Mdns::dispatchServiceLost, &m_manager, &Mdns::onServiceLost, Qt::QueuedConnection);
        QObject::connect(&m_manager, &Mdns::dispatchServiceResolved, &m_manager, &Mdns::onServiceResolved, Qt::QueuedConnection);
    }

    void start()
    {
        if (m_running) {
            return;
        }

        m_running = true;

        m_thread = std::thread([this] {
            // All Avahi objects created here, on the poll thread
            if (!((m_poll = avahi_simple_poll_new()))) {
                qCritical() << "Failed to create simple poll object.";
                m_running = false;
                return;
            }

            int error = 0;
            m_client = avahi_client_new(avahi_simple_poll_get(m_poll),
                                        static_cast<AvahiClientFlags>(0),
                                        reinterpret_cast<AvahiClientCallback>(&AvahiDiscoverer::client_callback),
                                        this,
                                        &error);
            if (!m_client) {
                qWarning() << "Failed to create client: " << avahi_strerror(error);
                avahi_simple_poll_free(m_poll);
                m_poll = nullptr;
                m_running = false;
                return;
            }

            m_sbDtls = createBrowser(&dtlsArg, "_drming._udp");
            m_sbSsl = createBrowser(&sslArg, "_drming._tcp");

            if (m_sbDtls && m_sbSsl) {
                avahi_simple_poll_loop(m_poll);
            }

            avahi_service_browser_free(m_sbDtls);
            avahi_service_browser_free(m_sbSsl);
            m_sbDtls = nullptr;
            m_sbSsl = nullptr;
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
    AvahiDiscovererArg dtlsArg{this, "dtls"};
    AvahiDiscovererArg sslArg{this, "ssl"};

    std::set<AvahiServiceResolver *> m_pendingResolvers{};
    std::thread m_thread{};
    Mdns &m_manager;
    AvahiSimplePoll *m_poll = nullptr;
    AvahiClient *m_client = nullptr;
    AvahiServiceBrowser *m_sbSsl = nullptr;
    AvahiServiceBrowser *m_sbDtls = nullptr;
    std::atomic<bool> m_running = false;

    AvahiServiceBrowser *createBrowser(AvahiDiscovererArg *serviceArg, const char *advertisedProtocol)
    {
        return avahi_service_browser_new(m_client,
                                         AVAHI_IF_UNSPEC,
                                         AVAHI_PROTO_UNSPEC,
                                         advertisedProtocol,
                                         nullptr,
                                         static_cast<AvahiLookupFlags>(0),
                                         reinterpret_cast<AvahiServiceBrowserCallback>(&AvahiDiscoverer::browse_callback),
                                         serviceArg);
    }

    static void resolve_callback(AvahiServiceResolver *r,
                                 const AvahiIfIndex interface,
                                 const AvahiProtocol protocol,
                                 const AvahiResolverEvent event,
                                 const char *name,
                                 const char *type,
                                 const char *domain,
                                 const char *host_name,
                                 const AvahiAddress *address,
                                 const uint16_t port,
                                 const AvahiStringList *txt,
                                 const AvahiLookupResultFlags flags,
                                 AvahiDiscovererArg *arg)
    {
        Q_UNUSED(interface);
        Q_UNUSED(protocol);
        Q_UNUSED(txt);
        Q_UNUSED(flags);

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
            Q_EMIT arg->discoverer->m_manager.dispatchServiceResolved(name, host_name, QString(a.data()), port, arg->protocol);
            break;
        }
        default: {
            break;
        }
        }
        arg->discoverer->m_pendingResolvers.erase(r);
        avahi_service_resolver_free(r);
    }

    static void browse_callback(const AvahiServiceBrowser *b,
                                const AvahiIfIndex interface,
                                const AvahiProtocol protocol,
                                const AvahiBrowserEvent event,
                                const char *name,
                                const char *type,
                                const char *domain,
                                const AvahiLookupResultFlags flags,
                                AvahiDiscovererArg *arg)
    {
        Q_UNUSED(flags);
        Q_UNUSED(b);

        switch (event) {
        case AVAHI_BROWSER_FAILURE: {
            qCritical() << "(Browser) " << avahi_strerror(avahi_client_errno(arg->discoverer->m_client));
            return;
        }
        case AVAHI_BROWSER_NEW: {
            Q_EMIT arg->discoverer->m_manager.dispatchServiceFound(name, type, arg->protocol);

            const auto resolver = avahi_service_resolver_new(arg->discoverer->m_client,
                                                             interface,
                                                             protocol,
                                                             name,
                                                             type,
                                                             domain,
                                                             AVAHI_PROTO_UNSPEC,
                                                             static_cast<AvahiLookupFlags>(0),
                                                             reinterpret_cast<AvahiServiceResolverCallback>(&AvahiDiscoverer::resolve_callback),
                                                             arg);

            /* Resolve the newly discovered service */
            if (resolver) {
                arg->discoverer->m_pendingResolvers.insert(resolver);
            } else {
                qWarning() << "Failed to resolve service '" << name << "': " << avahi_strerror(avahi_client_errno(arg->discoverer->m_client));
            }
            break;
        }
        case AVAHI_BROWSER_REMOVE: {
            Q_EMIT arg->discoverer->m_manager.dispatchServiceLost(name, domain, arg->protocol);
            break;
        }
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED: {
            qInfo() << "(Browser) " << (event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
        }
        }
    }

    static void client_callback(AvahiClient *client, const AvahiClientState state, const AvahiDiscoverer *c)
    {
        Q_UNUSED(c);

        if (state == AVAHI_CLIENT_FAILURE) {
            qCritical() << "Server connection failure: " << avahi_strerror(avahi_client_errno(client));
            // [TODO] Maybe use stopDiscovery on owner with thread dispatch
            //avahi_simple_poll_quit(c->m_poll);
        }
    }
};

Mdns::Mdns(QObject *parent)
    : ::Mdns(parent)
    , m_avahi(new AvahiDiscoverer(*this))
{
    CTRACK;
}

Mdns::~Mdns()
{
    delete m_avahi;
}

void Mdns::startDiscovery()
{
    m_avahi->start();
}

void Mdns::stopDiscovery()
{
    m_avahi->stop();
}

} // namespace Platform
