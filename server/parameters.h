#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QHostAddress>
#include <QNetworkInterface>
#include <QObject>

namespace Opts {

Q_NAMESPACE
enum DisplayStreamType {
    jpg,
    png,
    webp,
    h265,
};

Q_ENUM_NS(DisplayStreamType);

} // namespace Opts

struct Parameters
{
    QString targetScreen;
    QString serviceName;

    QHostAddress serviceIp;
    QHostAddress serviceAudioIp;
    QHostAddress serviceVideoIp;
    QNetworkInterface serviceIface;

    int port;
    int audioPort;
    int videoPort;

    int qualityLevel;
    int servedScreens = 0;
    bool advertise;

    QString trustedCertsPath;
    QString serverCertPath;
    QString serverKeyPath;

    Opts::DisplayStreamType streamFormat;

    static Parameters instance;
};

#endif // PARAMETERS_H
