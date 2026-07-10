#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QQuickStyle>

#include "fileprovider.h"
#include "mdns.h"
#include "networklink.h"
#include "networkstatus.h"
#include "palette.h"

#include "models/servicesmodel.h"

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int &argc, char **argv, const int flags = ApplicationFlags)
        : QApplication(argc, argv, flags)
    {
        QQuickStyle::setFallbackStyle("Basic");
        QApplication::setPalette(readPalette(":/assets/palette.data"));
        QObject::connect(qApp, &QGuiApplication::aboutToQuit, [this] {
            if (m_networkLink) {
                m_networkLink->close();
            }
            Mdns::instance()->stopDiscovery();
        });
    }

    static Application *instance() { return qobject_cast<Application *>(qApp); }

    Q_INVOKABLE Mdns *mdnsManager() const { return Mdns::instance(); }
    Q_INVOKABLE NetworkState *networkState() const { return NetworkState::instance(); }
    Q_INVOKABLE FileProvider *fileProvider() const { return FileProvider::instance(); }
    Q_INVOKABLE NetworkLink *networkLink() { return m_networkLink ? m_networkLink : m_networkLink = new NetworkLink(this); }
    Q_INVOKABLE ServicesModel *servicesModel() { return m_servicesModel ? m_servicesModel : m_servicesModel = new ServicesModel(this); }

private:
    NetworkLink *m_networkLink = nullptr;
    ServicesModel *m_servicesModel = nullptr;
};

#endif // APPLICATION_H
