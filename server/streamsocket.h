#ifndef STREAMSOCKET_H
#define STREAMSOCKET_H

#include "../net/udp.h"

class StreamSocket : public UdpStreamSocket
{
    Q_OBJECT

public:
    inline explicit StreamSocket(QObject *parent = nullptr)
        : UdpStreamSocket(parent)
    {}

    inline void addData(QByteArray) override {}
};

#endif // STREAMSOCKET_H
