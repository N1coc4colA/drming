#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "mdnsmanager.h"
#include "networklink.h"
#include "servicesmodel.h"
#include "videoframeitem.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Create and expose the C++ components.
    qmlRegisterType<VideoFrameItem>("VideoStream", 1, 0, "VideoFrame");
    auto mdnsInst = MdnsManager();
    auto link = new NetworkLink(&app);
    auto *servicesModel = new ServicesModel(&app);

    QObject::connect(qApp, &QGuiApplication::aboutToQuit, &mdnsInst, &MdnsManager::stopDiscovery);
    QObject::connect(qApp, &QGuiApplication::aboutToQuit, link, &NetworkLink::close);
    QObject::connect(&mdnsInst.networkState(), &NetworkState::connectivityChanged, [servicesModel](bool connected) {
        if (!connected) {
            servicesModel->clear();
        }
    });

    QQmlApplicationEngine engine{};
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.rootContext()->setContextProperty("mdnsManager", &mdnsInst);
    engine.rootContext()->setContextProperty("networkState", &mdnsInst.networkState());
    engine.rootContext()->setContextProperty("servicesModel", servicesModel);
    engine.rootContext()->setContextProperty("networkLink", link);

    engine.loadFromModule("drming", "Main");

    return app.exec();
}
