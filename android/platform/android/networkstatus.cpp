#include "networkstatus.h"

#include "native.h"

namespace Platform {

NetworkState::NetworkState(QObject *parent)
    : ::NetworkState(parent)
{
    if (!createNativeObject_NetworkHelper(m_javaHelper)) {
        return; // [TODO] Generate exception
    }
}

} // namespace Platform
