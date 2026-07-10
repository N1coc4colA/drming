#include "mdns.h"

#include "native.h"

namespace Platform {

Mdns::Mdns(QObject *parent)
    : ::Mdns(parent)
{
    if (!createNativeObject_MdnsHelper(m_javaHelper)) {
        return; // [TODO] Generate an exception
    }
}

void Mdns::startDiscovery()
{
    m_javaHelper.callMethod<void>("startDiscovery");
}

void Mdns::stopDiscovery()
{
    if (m_javaHelper.isValid()) {
        m_javaHelper.callMethod<void>("stopDiscovery");
    }
}

} // namespace Platform
