#include "displaymanager.h"

#include <QDebug>

#include "../settings.h"

#include "displaythread.h"
#include "dispsetup.h"
#include "parameters.h"

DisplayManager::DisplayManager(QObject *parent)
    : QObject(parent)
{}

DisplayManager::~DisplayManager()
{
    // Let Qt ownership take care of the children.
}

bool DisplayManager::registerClient(NetworkClient *client)
{
    client->setParent(this);

    const auto number = QString::number(Parameters::instance.servedScreens);
    if (!m_thread) {
        qDebug() << "Creating new display";

        // [TODO] Move this somewhere else, should be once the identity's checked.
        if (Parameters::instance.servedScreens > Settings::maximumDisplayCount) {
            qDebug() << "Too many screens generated, rejecting" << client->peerAddress() << client->peerPort();
            return false;
        }

        const auto instance = Parameters::instance.targetScreen + "_" + QString(Settings::maximumDisplayCountLength - number.size(), '0') + number;
        const DispSetup setup(instance);
        if (!setup.isSetup()) {
            qDebug() << "Failed to setup screen, rejecting" << client->peerAddress() << client->peerPort();
            return false;
        }

        m_thread = new DisplayThread(generateNewDisplay(setup.virtualConnectorName(), this), this);
    }

    if (!m_thread->isRunning()) {
        m_thread->start();
    }

    m_thread->addClient(client);
    return true;
}
