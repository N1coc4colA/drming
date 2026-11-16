#ifndef NETWORKSTATEPLATFORM_H
#define NETWORKSTATEPLATFORM_H

#include <QJniObject>

#include "../../networkstatus.h"

namespace Platform {

/// Tracks whether the network is currently usable.
class NetworkState : public ::NetworkState
{
    Q_OBJECT

public:
    explicit NetworkState(QObject *parent = nullptr);

private:
    /// JNI wrapper for the NetworkHelper Java helper.
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // NETWORKSTATEPLATFORM_H
