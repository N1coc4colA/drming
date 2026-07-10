#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

#include "application.h"
#include "declarative/videoframeitem.h"
#include "palette.h"

int main(int argc, char *argv[])
{
    Application app(argc, argv);

    qmlRegisterType<VideoFrameItem>("VideoStream", 1, 0, "VideoFrame");

    QQmlApplicationEngine engine(&app);
    engine.rootContext()->setContextObject(&app);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, qApp, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("drming", "Main");

    QApplication::setPalette(readPalette(":/assets/palette.data"));

    return QApplication::exec();
}
