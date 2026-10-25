#ifndef UDP_H
#define UDP_H

#include <QDtls>
#include <QMutex>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QUdpSocket>
#include <QtEndian>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>

#include "qaesencryption.h"

#include "parser.h"

#include "../settings.h"

#ifndef Q_MOC_RUN
#include <ctrack.hpp>
#endif

using Endpoint = QPair<QHostAddress, quint16>;

// #define ENABLE_ENCRYPT

class UdpStreamSocket : public QObject
{
    Q_OBJECT
    const bool NeedsPacketPrint = false;

public:
    explicit UdpStreamSocket(QObject *parent = nullptr, const bool needsPacketPrint = false)
        : QObject(parent)
        , NeedsPacketPrint(needsPacketPrint)
        , m_socket(this)
    {
        connect(&m_socket, &QUdpSocket::connected, this, &UdpStreamSocket::connected);
        connect(&m_socket, &QUdpSocket::disconnected, this, &UdpStreamSocket::disconnected);
        connect(&m_socket, &QUdpSocket::errorOccurred, this, &UdpStreamSocket::errorOccurred);
        connect(&m_socket, &QUdpSocket::readyRead, this, &UdpStreamSocket::onReadyRead4);

        const auto bufferSize = 128 * 1024 * 1024;
        m_socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, bufferSize);

        rotateKey();
    }

    inline Packets::Keystamp getKeystamp() const
    {
        return m_ks;
    }
    inline QByteArray getKey() const
    {
        return m_key;
    }

    inline auto ipv4Address() const
    {
        return m_ipAddress;
    }

    inline auto socket()
    {
        return &m_socket;
    }

    inline auto clearJitterBuffer()
    {
        m_jitterBuffer.clear();
    };

Q_SIGNALS:
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError error);
    void keyChanged();

public Q_SLOTS:
    void connectToHost(const QHostAddress &address, const quint16 port)
    {
        m_socket.connectToHost(address, port);
    }

    void connectToMulticast(const QHostAddress &address, const quint16 port)
    {
        const auto proto = address.protocol();

        if (proto == QAbstractSocket::IPv4Protocol || proto == QAbstractSocket::IPv6Protocol) {
            if (!m_socket.bind(
                    proto == QAbstractSocket::IPv4Protocol ? QHostAddress::AnyIPv4 : QHostAddress::AnyIPv6,
                    port,
                    QAbstractSocket::ReuseAddressHint | QAbstractSocket::ShareAddress
                )) {
                qDebug() << "Failed to bind for multicast:" << address << ':' << port << m_socket.errorString();
                // [TODO] Propagate the error
                return;
            }
        } else {
            qWarning() << "Invalid protocol:" << proto;
            // [TOOD] Propagate the error
            return;
        }

        bool joined = false;
        for (const auto &iface : QNetworkInterface::allInterfaces()) {
            const auto flags = iface.flags();
            const auto name = iface.name();

            if (!flags.testFlags(
                    QNetworkInterface::CanMulticast | QNetworkInterface::IsUp | QNetworkInterface::IsLoopBack
                )) {
                continue;
            }

            if (name.startsWith("tailscale") || name.startsWith("docker") || name.startsWith("tun")
                || name.startsWith("tap")) {
                continue;
            }

            if (iface.isValid()) {
                m_socket.setMulticastInterface(iface);
                if (m_socket.joinMulticastGroup(address, iface)) {
                    qDebug() << "Joined multicast group" << address << "on interface" << iface.name();
                    joined = true;
                }
            }
        }

        if (!joined) {
            // Fallback to default interface
            if (!m_socket.joinMulticastGroup(address)) {
                qDebug() << "Failed to join multicast group:" << address << m_socket.errorString();
            }
        }
    }

    void connectToMulticast(const QHostAddress &address, const quint16 port, const QNetworkInterface &iface)
    {
        const auto proto = address.protocol();
        if (proto == QAbstractSocket::IPv4Protocol || proto == QAbstractSocket::IPv6Protocol) {
            if (!m_socket.bind(
                    proto == QAbstractSocket::IPv4Protocol ? QHostAddress::AnyIPv4 : QHostAddress::AnyIPv6,
                    port,
                    QAbstractSocket::ReuseAddressHint | QAbstractSocket::ShareAddress
                )) {
                qDebug() << "Failed to bind for multicast:" << address << ':' << port << m_socket.errorString();
                // [TODO] Propagate the error
                return;
            }
        } else {
            qWarning() << "Invalid protocol:" << proto;
            // [TOOD] Propagate the error
            return;
        }

        m_socket.setMulticastInterface(iface);

        if (!m_socket.joinMulticastGroup(address)) {
            qDebug() << "Failed to join multicast group:" << address << m_socket.errorString();
        }
    }

    bool listen(const QHostAddress &address, const quint16 port, const QNetworkInterface &iface)
    {
        if (!address.isNull() && !listen(m_socket, QHostAddress::AnyIPv4, address, port, iface)) {
            return false;
        }

        m_ipAddress = address;

        return true;
    }

    void close()
    {
        m_socket.close();
    }

    bool write(const QByteArray &data)
    {
        if (m_socket.state() == QAbstractSocket::BoundState
            && !write(data, m_socket, m_ipAddress, m_socket.localPort())) {
            return false;
        }

        return true;
    }

    void onReadyRead4()
    {
        onReadyRead(m_socket);
    }

    void setKey(const QByteArray &key, const Packets::Keystamp ks)
    {
        if (key == m_key && ks == m_ks) {
            return;
        }

        m_key = key;
        m_ks = ks;
        Q_EMIT keyChanged();
    }

    void rotateKey()
    {
        m_keyCount++;
        m_keyCount %= std::numeric_limits<Packets::Keystamp>::max() - 1;

        setKey(
            QPasswordDigestor::deriveKeyPbkdf2(
                QCryptographicHash::Sha256,
                generate(sizeof(quint32) * 10),
                generate(Settings::saltLen),
                Settings::keyIterations,
                Settings::dkLen
            ),
            m_keyCount
        );
    }

protected:
    virtual void addData(QByteArray additional) = 0;
    /**
     * Called when the jitter buffer abandons an incomplete timestamp because
     * a chunk was lost in transit. Subclasses use this to request an IDR.
     */
    virtual void onFrameSkipped() {};

private:
    QHostAddress m_ipAddress{};

    QUdpSocket m_socket{ this };

    Packets::JitterBuffer<> m_jitterBuffer;
    Packets::Timestamp m_ts = 0;

    Packets::Keystamp m_keyCount = 0;
    Packets::Keystamp m_ks = 0;
    QByteArray m_key;
    QAESEncryption m_aes = { QAESEncryption::AES_256, QAESEncryption::CBC, QAESEncryption::PKCS7 };

    static QByteArray generate(const qsizetype len)
    {
        assert(len % sizeof(quint32) == 0);

        QByteArray d(len, Qt::Uninitialized);

#ifdef ENABLE_ENCRYPT
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(d.data()), d.size() / sizeof(quint32));
#endif

        return d;
    }

    static inline QByteArray generateIV()
    {
        return generate(Settings::ivSize);
    }

    bool listen(
        QUdpSocket &socket,
        const QHostAddress &bindAddr,
        const QHostAddress &address,
        const quint16 port,
        const QNetworkInterface &iface
    )
    {
        if (!socket.bind(bindAddr, port, QAbstractSocket::ReuseAddressHint | QAbstractSocket::ShareAddress)) {
            qCritical() << "Failed to bind UDP socket" << address << ':' << port << ':' << socket.errorString();
            return false;
        }

        const bool isValid = iface.isValid();
        if (isValid) {
            socket.setMulticastInterface(iface);
        }

        if (isValid ? !socket.joinMulticastGroup(address, iface) : !socket.joinMulticastGroup(address)) {
            qCritical() << "Failed to join multicast" << iface.name() << ':' << address.toString() << ':' << port
                        << socket.errorString();
            return false;
        }

        // Set TTL directly on the socket instead of using Qt's datagram classes and repeatedly set it.
        const int sockFd = static_cast<int>(socket.socketDescriptor());
        constexpr int ttl = 5;
        setsockopt(sockFd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
        setsockopt(sockFd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
        setsockopt(sockFd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl));

        qDebug() << "Joined multicast" << address << ':' << port;
        return true;
    }

    void processPlain(
        QByteArray plain,
        const Packets::Timestamp ts,
        const Packets::Ordering order,
        const Packets::Ordering sizing
    )
    {
        CTRACK;

        if (plain.isEmpty()) {
            qDebug() << "invalid plain size:" << plain.size();
            return;
        }

        m_jitterBuffer.pushChunk(ts, order, sizing, std::move(plain));

        bool droppedAny = false;
        while (const auto v = m_jitterBuffer.pullComplete(droppedAny)) {
            if (droppedAny) {
                onFrameSkipped();
                droppedAny = false;
            }

            addData(std::move(*v));
        }
    }

    void onReadyRead(QUdpSocket &socket)
    {
        static constexpr qsizetype headerSize = sizeof(Packets::Keystamp) + Settings::ivSize
            + sizeof(Packets::Timestamp) + sizeof(Packets::Ordering) * 2;

        while (socket.hasPendingDatagrams()) {
            CTRACK;

            QByteArray dgram(socket.pendingDatagramSize(), Qt::Uninitialized);
            const auto bytesRead = socket.readDatagram(dgram.data(), dgram.size());
            if (bytesRead <= headerSize) {
                qWarning() << "Spurious UDP read";
                continue;
            }

            dgram.resize(bytesRead);

            if (NeedsPacketPrint) {
                qDebug() << "Received packet";
            }

            qsizetype offset = 0;

            const auto order = qFromBigEndian(*reinterpret_cast<const Packets::Ordering *>(dgram.constData() + offset));
            offset += sizeof(Packets::Ordering);

            const auto sizing = qFromBigEndian(
                *reinterpret_cast<const Packets::Ordering *>(dgram.constData() + offset)
            );
            offset += sizeof(Packets::Ordering);

            const auto ts = qFromBigEndian(*reinterpret_cast<const Packets::Timestamp *>(dgram.constData() + offset));
            offset += sizeof(Packets::Timestamp);

#ifdef ENABLE_ENCRYPT
            const auto iv = dgram.mid(offset, Settings::ivSize);
#endif
            offset += Settings::ivSize;

            const auto ks = qFromBigEndian(*reinterpret_cast<const Packets::Keystamp *>(dgram.constData() + offset));
            offset += sizeof(Packets::Keystamp);

            if (ks != m_ks) {
                qDebug() << "Ks is wrong:" << ks << m_ks;
                continue;
            }

#ifdef ENABLE_ENCRYPT
            bool ok = false;
            const auto plain = m_aes.decode(dgram.sliced(offset), m_key, iv, &ok);
            if (!ok) {
                qDebug() << "Failed to decode";
                continue;
            }
#else
            auto plain = dgram.sliced(offset);
#endif

            processPlain(std::move(plain), ts, order, sizing);
        }
    }

    bool write(const QByteArray &data, QUdpSocket &socket, const QHostAddress &address, const quint16 port)
    {
        if (data.isEmpty()) [[unlikely]] {
            return true;
        }

        CTRACK;

        static constexpr qsizetype aesBlock = 16;
        static constexpr auto innerSize = Settings::dtlsChunkSize - Settings::ivSize - sizeof(Packets::Keystamp)
            - sizeof(Packets::Ordering) * 2 - sizeof(Packets::Timestamp) - aesBlock;
        static constexpr auto headerSize = sizeof(Packets::Ordering) * 2 + sizeof(Packets::Timestamp) + Settings::ivSize
            + sizeof(Packets::Keystamp);

        const int fd = static_cast<int>(socket.socketDescriptor());
        if (fd < 0) [[unlikely]] {
            return false;
        }

        // Destination sockaddr, filled once per write() call & reused by all datagrams in the batch.
        sockaddr_storage dst{};
        socklen_t dstLen = 0;

        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            auto *sin = reinterpret_cast<sockaddr_in *>(&dst);
            sin->sin_family = AF_INET;
            sin->sin_port = htons(port);
            sin->sin_addr.s_addr = htonl(address.toIPv4Address());
            dstLen = sizeof(sockaddr_in);
        } else if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            auto *sin6 = reinterpret_cast<sockaddr_in6 *>(&dst);
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = htons(port);
            const auto v6 = address.toIPv6Address();
            std::memcpy(&sin6->sin6_addr, v6.c, sizeof(sin6->sin6_addr));
            dstLen = sizeof(sockaddr_in6);
        } else [[unlikely]] {
            return false;
        }

        const auto size = data.size();
        const Packets::Ordering sizing = size / innerSize + (size % innerSize ? 1 : 0);
        const auto iv = generateIV();
        const auto ks_be = qToBigEndian(m_keyCount);
        const auto ts_be = qToBigEndian(m_ts);
        const auto sizing_be = qToBigEndian(sizing);

        // Batch 32 datagrams per syscall. Everything below is stack-allocated
        // so there's no per-write heap churn beyond the payload buffers.
        static constexpr int kBatch = 32;
        struct mmsghdr mmsgs[kBatch];
        struct iovec iovs[kBatch];
        QByteArray buffers[kBatch];
        int inFlight = 0;

        auto flush = [&]() -> bool {
            if (inFlight == 0)
                return true;
            int sent = 0;
            while (sent < inFlight) {
                const int n = ::sendmmsg(fd, mmsgs + sent, inFlight - sent, 0);
                if (n < 0) {
                    if (errno == EINTR)
                        continue;
                    qWarning() << "sendmmsg:" << strerror(errno);
                    inFlight = 0;
                    return false;
                }
                sent += n;
            }
            inFlight = 0;
            return true;
        };

        Packets::Ordering order = 0;
        qsizetype offset = 0;
        while (offset < size) {
            const auto order_be = qToBigEndian(order);
            const auto chunkSize = std::min<qsizetype>(innerSize, size - offset);

            QByteArray ciphertext{};
            const char *payload = nullptr;
            qsizetype payloadSize = 0;

#ifdef ENABLE_ENCRYPT
            {
                bool ok = false;
                ciphertext = m_aes.encode(data.sliced(offset, chunkSize), m_key, iv, &ok);
                if (!ok) {
                    qDebug() << "Failed to encode.";
                    return false;
                }
                payload = ciphertext.constData();
                payloadSize = ciphertext.size();
            }
#else
            payload = data.constData() + offset;
            payloadSize = chunkSize;
#endif

            QByteArray &chunk = buffers[inFlight];
            chunk = QByteArray(headerSize + payloadSize, Qt::Uninitialized);
            char *p = chunk.data();
            std::memcpy(p, &order_be, sizeof(order_be));
            p += sizeof(order_be);
            std::memcpy(p, &sizing_be, sizeof(sizing_be));
            p += sizeof(sizing_be);
            std::memcpy(p, &ts_be, sizeof(ts_be));
            p += sizeof(ts_be);
            std::memcpy(p, iv.constData(), Settings::ivSize);
            p += Settings::ivSize;
            std::memcpy(p, &ks_be, sizeof(ks_be));
            p += sizeof(ks_be);
            std::memcpy(p, payload, payloadSize);

            iovs[inFlight].iov_base = chunk.data();
            iovs[inFlight].iov_len = static_cast<size_t>(chunk.size());

            std::memset(&mmsgs[inFlight], 0, sizeof(mmsghdr));
            mmsgs[inFlight].msg_hdr.msg_name = &dst;
            mmsgs[inFlight].msg_hdr.msg_namelen = dstLen;
            mmsgs[inFlight].msg_hdr.msg_iov = &iovs[inFlight];
            mmsgs[inFlight].msg_hdr.msg_iovlen = 1;

            ++inFlight;
            if (inFlight == kBatch) {
                if (!flush()) {
                    m_ts++;
                    return false;
                }
            }

            offset += chunkSize;
            order++;
        }

        if (!flush()) {
            m_ts++;
            return false;
        }

        m_ts++;
        return true;
    }
};

template<typename T>
class DtlsCommon
{
public:
    explicit DtlsCommon(T &owner, QDtls *dtls, QUdpSocket *socket, QMutex *mtx = nullptr)
        : m_owner(owner)
        , m_dtls(dtls)
        , m_socket(socket)
        , m_mtx(mtx)
    {}

    void setDtls(QDtls *dtls)
    {
        m_dtls = dtls;
    }
    void setSocket(QUdpSocket *socket)
    {
        m_socket = socket;
    }
    void setMutex(QMutex *mtx)
    {
        m_mtx = mtx;
    }

    QDtls *dtls()
    {
        return m_dtls;
    }
    const QDtls *dtls() const
    {
        return m_dtls;
    }
    QUdpSocket *socket()
    {
        return m_socket;
    }
    const QUdpSocket *socket() const
    {
        return m_socket;
    }
    QMutex *mutex()
    {
        return m_mtx;
    }
    const QMutex *mutex() const
    {
        return m_mtx;
    }

    T &m_owner;
    QDtls *m_dtls;
    QUdpSocket *m_socket;
    QMutex *m_mtx;
};

template<typename T, typename Storage = DtlsCommon<T>>
class DtlsWriterImpl
{
public:
    explicit DtlsWriterImpl(Storage &storage)
        : m_storage(storage)
    {}

    inline qint64 writeDtls(const QByteArray &data)
    {
        CTRACK;

        static constexpr auto innerSize = Settings::dtlsChunkSize - sizeof(Packets::Ordering) * 2
            - sizeof(Packets::Timestamp);
        static constexpr auto headerSize = sizeof(Packets::Timestamp) + sizeof(Packets::Ordering) * 2;

        const auto size = data.size();
        const auto sizing = Packets::Ordering(size / innerSize + (size % innerSize ? 1 : 0));
        const auto sizing_be = qToBigEndian(sizing);

        Packets::Ordering order = 0;
        qsizetype offset = 0;
        while (offset < size) {
            const auto chunkSize = std::min<qsizetype>(innerSize, size - offset);
            const auto ts_be = qToBigEndian(m_ts);
            const auto order_be = qToBigEndian(order);

            QByteArray chunk(headerSize + chunkSize, Qt::Uninitialized);
            char *p = chunk.data();
            std::memcpy(p, &ts_be, sizeof(ts_be));
            p += sizeof(ts_be);
            std::memcpy(p, &sizing_be, sizeof(sizing_be));
            p += sizeof(sizing_be);
            std::memcpy(p, &order_be, sizeof(order_be));
            p += sizeof(order_be);
            std::memcpy(p, data.constData() + offset, chunkSize);

            qint64 written;
            {
                QMutexLocker lock(m_storage.m_mtx);
                written = m_storage.m_dtls->writeDatagramEncrypted(m_storage.m_socket, chunk);
            }

            if (written < 0) {
                const auto err = m_storage.m_dtls->dtlsError();
                switch (err) {
                case QDtlsError::RemoteClosedConnectionError: {
                    qDebug() << "Remote closed";
                    m_storage.m_owner.close();
                    break;
                }
                case QDtlsError::UnderlyingSocketError:
                case QDtlsError::NoError: {
                    continue;
                }
                default: {
                    qWarning() << "DTLS write error on" << m_ts << order << static_cast<int>(err)
                               << m_storage.m_dtls->dtlsErrorString();
                }
                }

                m_ts++;
                return -1;
            }

            offset += chunkSize;
            order++;
        }

        m_ts++;
        return offset;
    }

private:
    Storage &m_storage;
    Packets::Timestamp m_ts = 0;
};

template<typename T, typename Storage = DtlsCommon<T>>
class DtlsReaderImpl
{
public:
    explicit DtlsReaderImpl(Storage &storage)
        : m_storage(storage)
    {}

    inline void processDtls(const QByteArray &dgram)
    {
        CTRACK;

        // Decrypt application datagram
        QByteArray plain;
        {
            QMutexLocker lock(m_storage.m_mtx);
            plain = m_storage.m_dtls->decryptDatagram(m_storage.m_socket, dgram);
        }

        if (!plain.isEmpty()) {
            if (plain.size() < static_cast<qsizetype>(sizeof(Packets::Timestamp) + sizeof(Packets::Ordering) * 2)) {
                qWarning() << "Invalid DTLS decrypted packet size has been received:" << plain.size();
                return;
            }

            // Layout produced by DtlsWriterImpl:
            //   [ ts (Timestamp) ][ sizing (Ordering) ][ order (Ordering) ][ payload ]
            qsizetype offset = 0;

            const auto ts = qFromBigEndian(*reinterpret_cast<const Packets::Timestamp *>(plain.constData() + offset));
            offset += sizeof(Packets::Timestamp);

            const auto sizing = qFromBigEndian(
                *reinterpret_cast<const Packets::Ordering *>(plain.constData() + offset)
            );
            offset += sizeof(Packets::Ordering);

            const auto order = qFromBigEndian(*reinterpret_cast<const Packets::Ordering *>(plain.constData() + offset));
            offset += sizeof(Packets::Ordering);

            // Strip transport metadata
            m_jitterBuffer.pushChunk(ts, order, sizing, plain.mid(offset));

            while (const auto v = m_jitterBuffer.pullComplete()) {
                m_storage.m_owner.pushData(std::move(*v));
            }
            return;
        }

        // If plain is empty, check for shutdown/remote-close
        if (m_storage.m_dtls->dtlsError() == QDtlsError::RemoteClosedConnectionError) {
            qWarning() << "DTLS shutdown received";
            m_storage.m_socket->close();
            return;
        }

        qWarning() << "Received zero-length DTLS plaintext or unexpected datagram";
    }

    auto &jitterBuffer()
    {
        return m_jitterBuffer;
    }

private:
    Storage &m_storage;
    Packets::JitterBuffer<> m_jitterBuffer;
};

template<typename T>
class DtlsReader : public DtlsCommon<T>, public DtlsReaderImpl<T, DtlsReader<T>>
{
public:
    explicit DtlsReader(T &owner, QDtls *dtls, QUdpSocket *socket, QMutex *mtx = nullptr)
        : DtlsCommon<T>(owner, dtls, socket, mtx)
        , DtlsReaderImpl<T, DtlsReader<T>>(*this)
    {}
};

template<typename T>
class DtlsWriter : public DtlsCommon<T>, public DtlsWriterImpl<T, DtlsWriter<T>>
{
public:
    explicit DtlsWriter(T &owner, QDtls *dtls, QUdpSocket *socket, QMutex *mtx = nullptr)
        : DtlsCommon<T>(owner, dtls, socket, mtx)
        , DtlsWriterImpl<T, DtlsWriter<T>>(*this)
    {}
};

template<typename T>
class DtlsReaderWriter :
    public DtlsCommon<T>,
    public DtlsReaderImpl<T, DtlsReaderWriter<T>>,
    public DtlsWriterImpl<T, DtlsReaderWriter<T>>
{
public:
    explicit DtlsReaderWriter(T &owner, QDtls *dtls, QUdpSocket *socket, QMutex *mtx = nullptr)
        : DtlsCommon<T>(owner, dtls, socket, mtx)
        , DtlsReaderImpl<T, DtlsReaderWriter>(*this)
        , DtlsWriterImpl<T, DtlsReaderWriter>(*this)
    {}
};

#endif // UDP_H
