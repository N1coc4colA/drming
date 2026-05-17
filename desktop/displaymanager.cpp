#include "displaymanager.h"

#include <QDebug>

#include "display.h"
#include "dispsetup.h"
#include "parameters.h"

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

void DisplayManager::registerClient(QTcpSocket *client)
{
    if (m_freeDisplays.isEmpty()) {
        DispSetup setup(Parameters::instance.targetScreen + "_" + QString::number(Parameters::instance.servedScreens));
        if (!setup.isSetup()) {
            return;
        }

        qInfo() << "Setup display on connector " << setup.virtualConnectorName();
        Parameters::instance.servedScreens++;
        m_freeDisplays.enqueue(new Display(setup.virtualConnectorName(), this));
    }

    auto disp = m_freeDisplays.dequeue();
    m_usedDisplays.insert(disp);
    disp->setClient(client);

    connect(disp, &Display::nowFree, this, [this](Display *disp) { m_freeDisplays.enqueue(disp); });
}
