#ifndef MDNSPLATFORM_H
#define MDNSPLATFORM_H

#include "../../mdns.h"

namespace Platform {

class AvahiDiscoverer;

class Mdns : public ::Mdns
{
    Q_OBJECT

public:
    explicit Mdns(QObject *parent = nullptr);
    ~Mdns() override;

    void startDiscovery() override;
    void stopDiscovery() override;

private:
    AvahiDiscoverer *m_avahi = nullptr;

Q_SIGNALS:
    void dispatchServiceFound(const QString &name, const QString &type, const QString &protocol);
    void dispatchServiceLost(const QString &name, const QString &ip, const QString &protocol);
    void dispatchServiceResolved(const QString &name, const QString &host, const QString &ip, int port, const QString &protocol);

    friend class AvahiDiscoverer;
};

} // namespace Platform

#endif // MDNSPLATFORM_H
