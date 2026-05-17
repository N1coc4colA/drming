#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QHostAddress>
#include <QString>

struct Parameters
{
    QString targetScreen;
    QString serviceName;
    QString serviceIp;
    QHostAddress serviceHostIp;
    int jpegCompression;
    int servedScreens = 0;
    int port;
    bool advertise;

    static Parameters instance;
};

#endif // PARAMETERS_H
