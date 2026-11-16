#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QQuickStyle>

#include "fileprovider.h"
#include "mdns.h"
#include "networklink.h"
#include "networkstatus.h"

#include "models/servicesmodel.h"

/**
 * Owns client-wide services exposed to QML and coordinates their shutdown.
 *
 * Platform implementations are created lazily so startup does not initialize
 * networking, discovery, or file storage before the UI needs them.
 */
class Application : public QGuiApplication
{
    Q_OBJECT

public:
    Application(int &argc, char **argv, const int flags = ApplicationFlags)
        : QGuiApplication(argc, argv, flags)
    {
        QQuickStyle::setFallbackStyle("Basic");

        QObject::connect(qApp, &Application::aboutToQuit, [this] {
            if (m_networkLink) {
                m_networkLink->close();
            }
            Mdns::instance()->stopDiscovery();
        });
    }

    /// Returns the active application instance.
    static Application *instance()
    {
        return qobject_cast<Application *>(qApp);
    }

    /// Returns the platform mDNS service registry.
    Q_INVOKABLE static Mdns *mdnsManager()
    {
        return Mdns::instance();
    }
    /// Returns the process-wide network connectivity state.
    Q_INVOKABLE static NetworkState *networkState()
    {
        return NetworkState::instance();
    }
    /// Returns the platform certificate and key store.
    Q_INVOKABLE static FileProvider *fileProvider()
    {
        return FileProvider::instance();
    }
    /// Creates the platform transport on first use and reuses it thereafter.
    Q_INVOKABLE NetworkLink *networkLink()
    {
        return m_networkLink ? m_networkLink : m_networkLink = NetworkLink::createForPlatform(this);
    }
    /// Returns the model populated by the mDNS manager.
    Q_INVOKABLE ServicesModel *servicesModel()
    {
        return m_servicesModel ? m_servicesModel : m_servicesModel = new ServicesModel(this);
    }

    Q_INVOKABLE static QStringList getAvailableInterfaces();

private:
    /// Transport retained for the active server connection, when created.
    NetworkLink *m_networkLink = nullptr;
    /// Model retained for discovered services, when requested by QML.
    ServicesModel *m_servicesModel = nullptr;
};

#endif // APPLICATION_H
