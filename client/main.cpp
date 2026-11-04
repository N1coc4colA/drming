#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include "application.h"
#include "palette.h"

#ifdef Q_OS_ANDROID
#include "platform/android/native.h"
#include "platform/android/videoframeitem.h"
#else
#include "platform/linux/videoframeitem.h"
#endif

int main(int argc, char *argv[])
{
    // The QApplication owns the services exposed to QML and closes transports
    // and discovery before the event loop is torn down.
    Application app(argc, argv);

#ifdef Q_OS_ANDROID
    QQuickWindow::setSceneGraphBackend("opengl");
    registerNativeMethods_DrmingActivity();
#endif

    // VideoFrame is implemented separately because Android and Linux use
    // different rendering paths.
    qmlRegisterType<Platform::VideoFrameItem>("VideoStream", 1, 0, "VideoFrame");

    QQmlApplicationEngine engine(&app);
    engine.rootContext()->setContextObject(&app);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        qApp,
        [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.loadFromModule("drming", "Main");

    QApplication::setPalette(readPalette(":/assets/palette.data"));

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() { qDebug().noquote() << ctrack::result_as_string(); });

    return QApplication::exec();
}
