#ifndef MDNSPLATFORM_H
#define MDNSPLATFORM_H

#include <QJniObject>

#include "../../mdns.h"

namespace Platform {

/// Coordinates multicast DNS service discovery.
class Mdns : public ::Mdns
{
    Q_OBJECT

public:
    explicit Mdns(QObject *parent = nullptr);

    /// Starts service discovery.
    Q_INVOKABLE void startDiscovery() override;
    /// Stops service discovery and releases its listeners.
    Q_INVOKABLE void stopDiscovery() override;

private:
    /// JNI wrapper for the MdnsHelper Java helper.
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // MDNSPLATFORM_H
