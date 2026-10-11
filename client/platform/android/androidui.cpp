#include "androidui.h"

#include "native.h"

namespace Platform {

AndroidUi::AndroidUi(QObject *parent)
    : QObject(parent)
{
    if (!createNativeObject_AndroidUi(m_javaHelper)) {
        return; // [TODO] Generate an exception
    }
}

void AndroidUi::enterFullScreen()
{
    m_javaHelper.callMethod<void>("enterFullScreen");
}

void AndroidUi::leaveFullScreen()
{
    m_javaHelper.callMethod<void>("leaveFullScreen");
}

} // namespace Platform
