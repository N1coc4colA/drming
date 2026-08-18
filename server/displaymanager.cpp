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

        Parameters::instance.servedScreens++;
        m_freeDisplays.enqueue(new DisplayThread(generateNewDisplay(setup.virtualConnectorName(), this), this));
    }

    const auto thread = m_freeDisplays.dequeue();

    const auto digest = client->digest();
    if (!m_usedDisplays.contains(digest)) {
        qDebug() << "Screen not already available for client" << client->peerAddress() << client->peerPort();

        m_count++;
        if (m_count == 1) {
            Q_EMIT firstClientConnected();
        }

        connect(
            thread,
            &DisplayThread::nowFree,
            this,
            [this](DisplayThread *thread) {
                // Remove any digest entries that pointed to this thread so it may be reused.
                m_usedDisplays.apply([this, thread](auto &it) {
                    if (it.value() == thread) {
                        it = m_usedDisplays.eraseUnsafe(it);
                    } else {
                        ++it;
                    }
                });

                m_freeDisplays.enqueue(thread);

                m_count--;
                if (m_count == 0) {
                    Q_EMIT noMoreClients();
                }
            },
            Qt::QueuedConnection);

        m_usedDisplays.insert(digest, thread);
        if (!thread->isRunning()) {
            thread->start();
        }
    }

    thread->addClient(client);

    return true;
}
