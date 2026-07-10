#ifndef MDNSPLATFORM_H
#define MDNSPLATFORM_H

#include <QJniObject>

#include "../../mdns.h"

namespace Platform {

class Mdns : public ::Mdns
{
    Q_OBJECT

public:
    explicit Mdns(QObject *parent = nullptr);

    Q_INVOKABLE void startDiscovery() override;
    Q_INVOKABLE void stopDiscovery() override;

private:
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // MDNSPLATFORM_H
