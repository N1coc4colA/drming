#ifndef MULTICASTLOCKPLATFORM_H
#define MULTICASTLOCKPLATFORM_H

#include <QJniObject>
#include <QObject>

namespace Platform {

class MulticastLock : public QObject
{
    Q_OBJECT

public:
    explicit MulticastLock(QObject *parent = nullptr);

    Q_INVOKABLE void lock();
    Q_INVOKABLE void release();

private:
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // MULTICASTLOCKPLATFORM_H
