#ifndef ANDROIDUIPLATFORM_H
#define ANDROIDUIPLATFORM_H

#include <QJniObject>
#include <QObject>

namespace Platform {

/// Bridges Qt UI calls to the Android activity.
class AndroidUi : public QObject
{
public:
    explicit AndroidUi(QObject *parent = nullptr);

    /// Hides system UI and enters immersive full-screen mode.
    Q_INVOKABLE void enterFullScreen();
    /// Restores the system UI after immersive mode.
    Q_INVOKABLE void leaveFullScreen();

private:
    /// JNI wrapper for the AndroidUi Java helper.
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // ANDROIDUIPLATFORM_H
