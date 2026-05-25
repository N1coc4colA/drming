#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "declarative/videoframeitem.h"
#include "fileprovider.h"
#include "mdns.h"
#include "models/servicesmodel.h"
#include "networklink.h"
#include "networkstatus.h"
#include "palette.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QQuickStyle::setFallbackStyle("Basic");

    // Create and expose the C++ components.
    qmlRegisterType<VideoFrameItem>("VideoStream", 1, 0, "VideoFrame");

    auto mndsInst = Mdns::instance();
    auto netInst = NetworkState::instance();
    auto link = new NetworkLink(&app);
    auto *servicesModel = new ServicesModel(&app);

    QObject::connect(qApp, &QGuiApplication::aboutToQuit, mndsInst, &Mdns::stopDiscovery);
    QObject::connect(qApp, &QGuiApplication::aboutToQuit, link, &NetworkLink::close);
    QObject::connect(netInst, &NetworkState::connectivityChanged, [servicesModel](bool connected) {
        if (!connected) {
            servicesModel->clear();
        }
    });

    QQmlApplicationEngine engine{};
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.rootContext()->setContextProperty("mdnsManager", mndsInst);
    engine.rootContext()->setContextProperty("networkState", netInst);
    engine.rootContext()->setContextProperty("servicesModel", servicesModel);
    engine.rootContext()->setContextProperty("networkLink", link);
    engine.rootContext()->setContextProperty("fileProvider", FileProvider::instance());

    engine.loadFromModule("drming", "Main");
    app.setPalette(readPalette(":/assets/palette.data"));

    return app.exec();
}
