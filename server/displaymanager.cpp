#include "displaymanager.h"

#include <QDebug>

#include "../settings.h"

#include "displaythread.h"
#include "dispsetup.h"
#include "parameters.h"

DisplayManager::DisplayManager(StreamSocket &ss, QObject *parent)
    : QObject(parent)
    , m_ss(ss)
{}

DisplayManager::~DisplayManager()
{
    // Let Qt ownership take care of the children.
    if (m_thread) {
        m_thread->exit();
        m_thread->wait();
    }

    if (m_setup) {
        delete m_setup;
    }
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

        const auto instance = Parameters::instance.targetScreen + "_"
            + QString(Settings::maximumDisplayCountLength - number.size(), '0') + number;

        if (!m_setup) {
            m_setup = new DispSetup(instance);
        }

        if (!m_setup->isSetup()) {
            qDebug() << "Failed to setup screen, rejecting" << client->peerAddress() << client->peerPort();
            return false;
        }

        m_thread = new DisplayThread(generateNewDisplay(m_setup->virtualConnectorName(), m_ss, this), this);
    }

    if (!m_thread->isRunning()) {
        m_thread->start();
    }

    m_thread->addClient(client);

    return true;
}
