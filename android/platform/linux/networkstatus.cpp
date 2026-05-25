#include "networkstatus.h"

namespace Platform {

NetworkState::NetworkState(QObject *parent)
    : ::NetworkState(parent)
{
    m_connected = true;
}

} // namespace Platform
