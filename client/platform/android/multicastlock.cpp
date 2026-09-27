#include "multicastlock.h"

#include "native.h"

namespace Platform {

MulticastLock::MulticastLock(QObject *parent)
    : QObject(parent)
{
    if (!createNativeObject_MulticastLockHelper(m_javaHelper)) {
        return; // [TODO] Generate an exception
    }
}

void MulticastLock::lock()
{
    m_javaHelper.callMethod<void>("lock");
}

void MulticastLock::release()
{
    if (m_javaHelper.isValid()) {
        m_javaHelper.callMethod<void>("release");
    }
}

} // namespace Platform
