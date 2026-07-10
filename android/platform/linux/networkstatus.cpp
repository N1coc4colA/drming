#include "networkstatus.h"

#include <ctrack.hpp>

namespace Platform {

NetworkState::NetworkState(QObject *parent)
    : ::NetworkState(parent)
{
    CTRACK;

    m_connected = true;
}

} // namespace Platform
