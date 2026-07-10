#ifndef NETWORKSTATEPLATFORM_H
#define NETWORKSTATEPLATFORM_H

#include "../../networkstatus.h"

namespace Platform {

class NetworkState : public ::NetworkState
{
    Q_OBJECT

public:
    explicit NetworkState(QObject *parent = nullptr);
};

} // namespace Platform

#endif // NETWORKSTATEPLATFORM_H
