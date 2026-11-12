#ifndef MDNSPLATFORM_H
#define MDNSPLATFORM_H

#include "../../mdns.h"

namespace Platform {

class AvahiDiscoverer;

/// Coordinates multicast DNS service discovery.
class Mdns : public ::Mdns
{
    Q_OBJECT

public:
    explicit Mdns(QObject *parent = nullptr);
    ~Mdns() override;

    /// Starts service discovery.
    void startDiscovery() override;
    /// Stops service discovery and releases its listeners.
    void stopDiscovery() override;

private:
    /// Avahi discovery backend implementation.
    AvahiDiscoverer *m_avahi = nullptr;

Q_SIGNALS:
    void dispatchServiceFound(const QString &name, const QString &type);
    void dispatchServiceLost(const QString &name, const QString &ip);
    void dispatchServiceResolved(const QString &name, const QString &host, const QString &ip, int port);

    friend class AvahiDiscoverer;
};

} // namespace Platform

#endif // MDNSPLATFORM_H
