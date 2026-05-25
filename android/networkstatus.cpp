#include "networkstatus.h"

#include <QCoreApplication>

#ifdef Q_OS_ANDROID
#include "platform/android/networkstatus.h"
#else
#include "platform/linux/networkstatus.h"
#endif

NetworkState *NetworkState::m_instance = nullptr;

NetworkState *NetworkState::instance()
{
    if (!m_instance) {
        m_instance = new Platform::NetworkState(qApp);
    }

    return m_instance;
}

NetworkState::NetworkState(QObject *parent)
    : QObject{parent}
{
    assert(!m_instance);
    m_instance = this;
}
