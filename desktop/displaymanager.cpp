#include "displaymanager.h"

#include <QDebug>

#include "display.h"
#include "dispsetup.h"
#include "parameters.h"
#include "settings.h"

DisplayManager::DisplayManager(QObject *parent)
    : QObject(parent)
{}

DisplayManager::~DisplayManager()
{
    qDeleteAll(m_freeDisplays);
    m_freeDisplays.clear();

    qDeleteAll(m_usedDisplays);
    m_usedDisplays.clear();
}

bool DisplayManager::registerClient(QTcpSocket *client)
{
    const auto number = QString::number(Parameters::instance.servedScreens);
    if (m_freeDisplays.isEmpty()) {
        if (Parameters::instance.servedScreens > Settings::maximumDisplayCount) {
            return false;
        }

        const auto instance = Parameters::instance.targetScreen + "_" + QString(Settings::maximumDisplayCountLength - number.size(), '0') + number;
        DispSetup setup(instance);
        if (!setup.isSetup()) {
            return false;
        }

        Parameters::instance.servedScreens++;
        m_freeDisplays.enqueue(new Display(setup.virtualConnectorName(), this));
    }

    auto disp = m_freeDisplays.dequeue();
    m_usedDisplays.insert(disp);
    disp->setClient(client);

    connect(disp, &Display::nowFree, this, [this](Display *disp) { m_freeDisplays.enqueue(disp); });

    return true;
}
