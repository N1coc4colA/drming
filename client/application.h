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

    static Application *instance()
    {
        return qobject_cast<Application *>(qApp);
    }

    /// Returns the platform mDNS service registry.
    Q_INVOKABLE Mdns *mdnsManager() const
    {
        return Mdns::instance();
    }
    /// Returns the process-wide network connectivity state.
    Q_INVOKABLE NetworkState *networkState() const
    {
        return NetworkState::instance();
    }
    /// Returns the platform certificate and key store.
    Q_INVOKABLE FileProvider *fileProvider() const
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

private:
    NetworkLink *m_networkLink = nullptr;
    ServicesModel *m_servicesModel = nullptr;
};

#endif // APPLICATION_H
