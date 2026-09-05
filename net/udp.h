#ifndef UDP_H
#define UDP_H

#include <QDtls>
#include <QMutex>
#include <QNetworkInterface>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QUdpSocket>
#include <QtEndian>

#include "qaesencryption.h"

#include "parser.h"

#include "../settings.h"

using Endpoint = QPair<QHostAddress, quint16>;

class UdpStreamSocket : public QObject
{
    Q_OBJECT

public:
    explicit UdpStreamSocket(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_socket4, &QUdpSocket::connected, this, &UdpStreamSocket::connected);
        connect(&m_socket4, &QUdpSocket::disconnected, this, &UdpStreamSocket::disconnected);
        connect(&m_socket4, &QUdpSocket::errorOccurred, this, &UdpStreamSocket::errorOccurred);
        connect(&m_socket4, &QUdpSocket::readyRead, this, &UdpStreamSocket::onReadyRead4);
        connect(&m_socket6, &QUdpSocket::connected, this, &UdpStreamSocket::connected);
        connect(&m_socket6, &QUdpSocket::disconnected, this, &UdpStreamSocket::disconnected);
        connect(&m_socket6, &QUdpSocket::errorOccurred, this, &UdpStreamSocket::errorOccurred);
        connect(&m_socket6, &QUdpSocket::readyRead, this, &UdpStreamSocket::onReadyRead6);

        rotateKey();
    }

    inline Packets::Keystamp getKeystamp() const { return m_ks; }
    inline QByteArray getKey() const { return m_key; }

Q_SIGNALS:
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError error);
    void keyChanged();

public Q_SLOTS:
    void connectToHost(const QHostAddress &address, const quint16 port)
    {
        const auto proto = address.protocol();
        if (proto == QAbstractSocket::IPv4Protocol) {
            m_socket4.connectToHost(address, port);
        } else if (proto == QAbstractSocket::IPv6Protocol) {
            m_socket6.connectToHost(address, port);
        } else {
            qWarning() << "Invalid protocol:" << proto;
        }
    }

    bool listen(const QHostAddress &address4,
                const QHostAddress &address6,
                const quint16 port,
                const QNetworkInterface &iface4,
                const QNetworkInterface &iface6)
    {
        if (!address4.isNull() && !listen(m_socket4, QHostAddress::AnyIPv4, address4, port, iface4)) {
            return false;
        }

        if (!address6.isNull() && !listen(m_socket6, QHostAddress::AnyIPv6, address6, port, iface6)) {
            return false;
        }

        m_ipv4Address = address4;
        m_ipv6Address = address6;

        return true;
    }

    void close()
    {
        m_socket4.close();
        m_socket6.close();
    }

    bool write(const QByteArray &data)
    {
        if (m_socket4.state() == QAbstractSocket::BoundState && !write(data, m_socket4, m_ipv4Address, m_socket4.localPort())) {
            return false;
        }
        if (m_socket6.state() == QAbstractSocket::BoundState && !write(data, m_socket6, m_ipv6Address, m_socket6.localPort())) {
            return false;
        }

        return true;
    }

    void onReadyRead4() { onReadyRead(m_socket4); }
    void onReadyRead6() { onReadyRead(m_socket6); }

    void setKey(const QByteArray &key, const Packets::Keystamp ks)
    {
        m_key = key;
        m_ks = ks;
        Q_EMIT keyChanged();
    }

    void rotateKey()
    {
        m_keyCount++;
        m_keyCount %= std::numeric_limits<Packets::Keystamp>::max() - 1;

        setKey(QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                                  generate(sizeof(quint32) * 10),
                                                  generate(Settings::saltLen),
                                                  Settings::keyIterations,
                                                  Settings::dkLen),
               m_keyCount);
    }

protected:
    virtual void addData(QByteArray additional) = 0;

private:
    QHostAddress m_ipv4Address{};
    QHostAddress m_ipv6Address{};

    QUdpSocket m_socket4{};
    QUdpSocket m_socket6{};

    Packets::JitterBuffer<> m_jitterBuffer;
    Packets::Timestamp m_ts = 0;

    inline static Packets::Keystamp m_keyCount = 0;
    Packets::Keystamp m_ks = 0;
    QByteArray m_key;
    QAESEncryption m_aes = {QAESEncryption::AES_256, QAESEncryption::CBC, QAESEncryption::PKCS7};

    static QByteArray generate(const qsizetype len)
    {
        assert(len % sizeof(quint32) == 0);

        QByteArray d(len, Qt::Uninitialized);
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(d.data()), d.size() / sizeof(quint32));

        return d;
    }

    static inline QByteArray generateIV() { return generate(Settings::ivSize); }

    bool listen(QUdpSocket &socket, const QHostAddress &bindAddr, const QHostAddress &address, const quint16 port, const QNetworkInterface &iface)
    {
        if (!socket.bind(bindAddr, port, QAbstractSocket::ShareAddress)) {
            qCritical() << "Failed to bind UDP socket" << address << ':' << port << ':' << socket.errorString();
            return false;
        }

        const bool isValid = iface.isValid();
        if (isValid) {
            socket.setMulticastInterface(iface);
        }

        if (isValid ? !socket.joinMulticastGroup(address, iface) : !socket.joinMulticastGroup(address)) {
            qCritical() << "Failed to join multicast" << iface.name() << ':' << address.toString() << ':' << port << socket.errorString();
            return false;
        }

        qDebug() << "Joined multicast" << address << ':' << port;
        return true;
    }

    void processPlain(QByteArray plain, const Packets::Timestamp ts, const Packets::Ordering order, const Packets::Ordering sizing)
    {
        if (plain.isEmpty()) {
            qDebug() << "invalid plain size:" << plain.size();
            return;
        }

        // We need to slice as the ordering is not part of the information wanted by the application.
        m_jitterBuffer.pushChunk(ts, order, sizing, std::move(plain.slice(static_cast<qsizetype>(sizeof(Packets::Ordering)))));

        while (const auto v = m_jitterBuffer.pullComplete()) {
            addData(std::move(*v));
        }
    }

    void onReadyRead(QUdpSocket &socket)
    {
        while (socket.hasPendingDatagrams()) {
            QByteArray dgram(socket.pendingDatagramSize(), Qt::Uninitialized);
            const qint64 bytesRead = socket.readDatagram(dgram.data(), dgram.size());
            if (bytesRead <= (Settings::ivSize + sizeof(Packets::Keystamp) + sizeof(Packets::Timestamp) + sizeof(Packets::Ordering) * 2)) {
                qWarning() << "Spurious UDP read";
                continue;
            }

            dgram.resize(bytesRead);

            const auto ks = qFromBigEndian(*reinterpret_cast<Packets::Timestamp *>(dgram.first(sizeof(Packets::Keystamp)).data()));
            if (ks != m_keyCount) {
                continue;
            }

            const auto iv = dgram.slice(static_cast<qsizetype>(sizeof(Packets::Keystamp))).first(Settings::ivSize);

            const auto ts = qFromBigEndian(
                *reinterpret_cast<Packets::Timestamp *>(dgram.slice(Settings::ivSize).first(sizeof(Packets::Timestamp)).data()));
            const auto sizing = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(
                dgram.slice(static_cast<qsizetype>(sizeof(Packets::Timestamp))).first(sizeof(Packets::Ordering)).data()));
            const auto order = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(
                dgram.slice(static_cast<qsizetype>(sizeof(Packets::Ordering))).first(sizeof(Packets::Ordering)).data()));

            bool ok = false;
            const auto plain = m_aes.decode(dgram.slice(sizeof(Packets::Ordering)), m_key, iv, &ok);
            if (!ok) {
                qDebug() << "Failed to decode";
                continue;
            }

            processPlain(std::move(plain), ts, order, sizing);
        }
    }

    bool write(const QByteArray &data, QUdpSocket &socket, const QHostAddress &address, const quint16 port)
    {
        if (data.isEmpty()) [[unlikely]] {
            return true;
        }

        static constexpr auto innerSize = Settings::dtlsChunkSize - Settings::ivSize - sizeof(Packets::Keystamp) - sizeof(Packets::Ordering) * 2
                                          - sizeof(Packets::Timestamp);
        const auto size = data.size();
        const Packets::Ordering sizing = size / innerSize + (size % innerSize ? 1 : 0);
        const auto iv = generateIV();
        const auto ks_be = qToBigEndian(m_keyCount);
        const auto ts_be = qToBigEndian(m_ts);
        const auto sizing_be = qToBigEndian(sizing);

        Packets::Ordering order = 0;
        qsizetype offset = 0;
        while (offset < size) {
            const auto order_be = qToBigEndian(order);

            bool ok = false;
            auto chunk = m_aes.encode(data.mid(offset, innerSize), m_key, iv, &ok);
            if (!ok) {
                qDebug() << "Failed to encode.";
                return false;
            }

            chunk.prepend(reinterpret_cast<const char *>(&order_be), sizeof(order_be));
            chunk.prepend(reinterpret_cast<const char *>(&sizing_be), sizeof(sizing_be));
            chunk.prepend(reinterpret_cast<const char *>(&ts_be), sizeof(ts_be));
            chunk.prepend(iv);
            chunk.prepend(reinterpret_cast<const char *>(&ks_be), sizeof(ks_be));

            if (m_socket4.writeDatagram(chunk, address, port) < 0) {
                const auto err = m_socket4.error();
                switch (err) {
                case QAbstractSocket::RemoteHostClosedError:
                case QAbstractSocket::SocketAccessError:
                case QAbstractSocket::SocketResourceError:
                case QAbstractSocket::SocketTimeoutError:
                case QAbstractSocket::ProxyConnectionClosedError:
                case QAbstractSocket::ProxyConnectionTimeoutError:
                case QAbstractSocket::OperationError:
                case QAbstractSocket::UnknownSocketError:
                case QAbstractSocket::NetworkError: {
                    close();
                    break;
                }
                case QAbstractSocket::TemporaryError: {
                    continue;
                }
                default: {
                    qWarning() << "Write error on" << m_ts << order << static_cast<int>(err) << socket.errorString();
                }
                }

                m_ts++;
                m_ts = m_ts % Settings::simultaneousPendings;

                return false;
            }

            offset += chunk.size();
            order++;
        }

        m_ts++;
        m_ts = m_ts % Settings::simultaneousPendings;

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

    void setDtls(QDtls *dtls) { m_dtls = dtls; }
    void setSocket(QUdpSocket *socket) { m_socket = socket; }
    void setMutex(QMutex *mtx) { m_mtx = mtx; }

    QDtls *dtls() { return m_dtls; }
    const QDtls *dtls() const { return m_dtls; }
    QUdpSocket *socket() { return m_socket; }
    const QUdpSocket *socket() const { return m_socket; }
    QMutex *mutex() { return m_mtx; }
    const QMutex *mutex() const { return m_mtx; }

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
        static constexpr auto innerSize = Settings::dtlsChunkSize - sizeof(Packets::Ordering) * 2 - sizeof(Packets::Timestamp);
        const auto size = data.size();
        const Packets::Ordering sizing = size / innerSize + (size % innerSize ? 1 : 0);
        const auto sizing_be = qToBigEndian(sizing);

        Packets::Ordering order = 0;
        qsizetype offset = 0;
        while (offset < size) {
            auto chunk = data.mid(offset, innerSize);
            const auto ts_be = qToBigEndian(m_ts);
            const auto order_be = qToBigEndian(order);
            chunk.prepend(reinterpret_cast<const char *>(&order_be), sizeof(order_be));
            chunk.prepend(reinterpret_cast<const char *>(&sizing_be), sizeof(sizing_be));
            chunk.prepend(reinterpret_cast<const char *>(&ts_be), sizeof(ts_be));

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
                    qWarning() << "DTLS write error on" << m_ts << order << static_cast<int>(err) << m_storage.m_dtls->dtlsErrorString();
                }
                }

                m_ts++;
                m_ts = m_ts % Settings::simultaneousPendings;

                return -1;
            }

            offset += chunk.size();
            order++;
        }

        m_ts++;
        m_ts = m_ts % Settings::simultaneousPendings;

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
        // Decrypt application datagram
        QByteArray plain;
        {
            QMutexLocker lock(m_storage.m_mtx);
            plain = m_storage.m_dtls->decryptDatagram(m_storage.m_socket, dgram);
        }

        if (!plain.isEmpty()) {
            if (plain.size() < static_cast<qsizetype>(sizeof(Packets::Ordering) * 2 + sizeof(Packets::Timestamp))) {
                qWarning() << "Invalid DTLS decrypted packet size has been received:" << plain.size();
                return;
            }

            const auto ts = qFromBigEndian(*reinterpret_cast<Packets::Timestamp *>(plain.first(sizeof(Packets::Timestamp)).data()));
            const auto sizing = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(
                plain.slice(static_cast<qsizetype>(sizeof(Packets::Timestamp))).first(sizeof(Packets::Ordering)).data()));
            const auto order = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(
                plain.slice(static_cast<qsizetype>(sizeof(Packets::Ordering))).first(sizeof(Packets::Ordering)).data()));

            // We need to slice as the ordering is not part of the information wanted by the application.
            m_jitterBuffer.pushChunk(ts, order, sizing, plain.slice(static_cast<qsizetype>(sizeof(Packets::Ordering))));

            while (const auto v = m_jitterBuffer.pullComplete()) {
                m_storage.m_owner.addData(std::move(*v));
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

    auto &jitterBuffer() { return m_jitterBuffer; }

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
class DtlsReaderWriter : public DtlsCommon<T>, public DtlsReaderImpl<T, DtlsReaderWriter<T>>, public DtlsWriterImpl<T, DtlsReaderWriter<T>>
{
public:
    explicit DtlsReaderWriter(T &owner, QDtls *dtls, QUdpSocket *socket, QMutex *mtx = nullptr)
        : DtlsCommon<T>(owner, dtls, socket, mtx)
        , DtlsReaderImpl<T, DtlsReaderWriter>(*this)
        , DtlsWriterImpl<T, DtlsReaderWriter>(*this)
    {}
};

#endif // UDP_H
