#ifndef NETWORKSTATEPLATFORM_H
#define NETWORKSTATEPLATFORM_H

#include <QJniObject>

#include "../../networkstatus.h"

namespace Platform {

class NetworkState : public ::NetworkState
{
    Q_OBJECT

public:
    explicit NetworkState(QObject *parent = nullptr);

private:
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // NETWORKSTATEPLATFORM_H
