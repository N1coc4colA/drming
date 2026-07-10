#include "fileprovider.h"

#include <QCoreApplication>

#ifdef Q_OS_ANDROID
#include "platform/android/fileprovider.h"
#else
#include "platform/linux/fileprovider.h"
#endif

FileProvider *FileProvider::m_instance = nullptr;

FileProvider *FileProvider::instance()
{
    if (!m_instance) {
        m_instance = new Platform::FileProvider(qApp);
    }

    return m_instance;
}

FileProvider::FileProvider(QObject *parent)
    : QObject(parent)
    , m_clientFiles(new FilesModel(this))
    , m_serverFiles(new FilesModel(this))
{
    assert(!m_instance);
    m_instance = this;
}
