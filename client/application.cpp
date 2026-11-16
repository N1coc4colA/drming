#include "application.h"

#include "../net/udp.h"

QStringList Application::getAvailableInterfaces()
{
    return UdpStreamSocket::interfaces();
}
