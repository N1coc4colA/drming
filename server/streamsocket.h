#ifndef STREAMSOCKET_H
#define STREAMSOCKET_H

#include "../net/udp.h"

/**
 * UDP stream endpoint shared by displays.
 *
 * The socket handles packet transport and encryption; Display supplies the
 * encoded frame payloads through the inherited stream interface.
 */
class StreamSocket : public UdpStreamSocket
{
    Q_OBJECT

public:
    inline explicit StreamSocket(QObject *parent = nullptr)
        : UdpStreamSocket(parent)
    {}

    inline void addData(QByteArray) override
    {}
};

#endif // STREAMSOCKET_H
