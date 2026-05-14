#include "networkstatus.h"

#include <QtSystemDetection>

#include "native.h"

NetworkState::NetworkState(QObject *parent)
    : QObject{parent}
{
#ifdef Q_OS_ANDROID
    if (!createNativeObject_NetworkHelper(m_javaHelper)) {
        return;
    }
#else
    m_connected = true;
#endif
}
