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
    if (m_freeDisplays.isEmpty()) {
        if (Parameters::instance.servedScreens > Settings::maximumDisplayCount) {
            return false;
        }

        const auto instance = Parameters::instance.targetScreen + "_" + QString(Settings::maximumDisplayCountLength - number.size(), '0') + number;
        const DispSetup setup(instance);
        if (!setup.isSetup()) {
            return false;
        }

        Parameters::instance.servedScreens++;
        m_freeDisplays.enqueue(new DisplayThread(generateNewDisplay(setup.virtualConnectorName(), this), this));
    }

    const auto thread = m_freeDisplays.dequeue();

    const auto digest = client->digest();
    if (!m_usedDisplays.contains(digest)) {
        m_usedDisplays.insert(digest, thread);
        thread->start();
    }

    thread->addClient(client);

    connect(
        thread,
        &DisplayThread::nowFree,
        this,
        [this](DisplayThread *thread) {
            thread->terminate();
            m_freeDisplays.enqueue(thread);
        },
        Qt::QueuedConnection);

    return true;
}
