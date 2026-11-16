#ifndef MULTICASTLOCKPLATFORM_H
#define MULTICASTLOCKPLATFORM_H

#include <QJniObject>
#include <QObject>

namespace Platform {

/// Controls the Android multicast lock.
class MulticastLock : public QObject
{
    Q_OBJECT

public:
    explicit MulticastLock(QObject *parent = nullptr);

    /// Acquires the multicast lock.
    Q_INVOKABLE void lock();
    /// Releases the multicast lock.
    Q_INVOKABLE void release();

private:
    /// JNI wrapper for the MulticastLockHelper Java helper.
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // MULTICASTLOCKPLATFORM_H
