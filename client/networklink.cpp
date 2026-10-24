#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QtEndian>

#include "../settings.h"

#include "fileprovider.h"

#ifdef Q_OS_ANDROID
#include "platform/android/networklink.h"
#else
#include "platform/linux/networklink.h"
#endif

NetworkLink::NetworkLink(QObject *parent)
    : QObject(parent)
    , m_sslSocket(new QSslSocket(this))
    , m_udpSocket(new QUdpSocket(this))
    , m_dtls(new QDtls(QSslSocket::SslClientMode, this))
    , m_rw(*this, m_dtls, m_udpSocket, nullptr)
{
    QObject::connect(m_udpSocket, &QUdpSocket::connected, this, &NetworkLink::onUdpConnected);
    QObject::connect(m_udpSocket, &QUdpSocket::disconnected, this, &NetworkLink::onDtlsDisconnected);
    QObject::connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkLink::onDtlsDataAvailable);
    QObject::connect(m_udpSocket, &QUdpSocket::errorOccurred, this, &NetworkLink::onError);

    QObject::connect(m_sslSocket, &QSslSocket::connected, this, &NetworkLink::onSslConnected);
    QObject::connect(m_sslSocket, &QSslSocket::encrypted, this, &NetworkLink::onSslEncrypted);
    QObject::connect(m_sslSocket, &QSslSocket::disconnected, this, &NetworkLink::onSslDisconnected);
    QObject::connect(m_sslSocket, &QSslSocket::readyRead, this, &NetworkLink::onSslDataAvailable);
    QObject::connect(m_sslSocket, &QSslSocket::handshakeInterruptedOnError, this, &NetworkLink::onSslError);
    QObject::connect(m_sslSocket, &QSslSocket::errorOccurred, this, &NetworkLink::onError);
    QObject::connect(m_sslSocket, &QSslSocket::peerVerifyError, [](const QSslError &error) {
        qWarning() << "Peer verification error:" << error;
    });
    QObject::connect(m_sslSocket, &QSslSocket::modeChanged, [](const QSslSocket::SslMode newMode) {
        qInfo() << "SSL mode changed:" << newMode;
    });

    QObject::connect(m_sslSocket, &QAbstractSocket::disconnected, this, &NetworkLink::closed);
    QObject::connect(m_udpSocket, &QAbstractSocket::disconnected, this, &NetworkLink::closed);

    QObject::connect(m_dtls, &QDtls::handshakeTimeout, this, [this] {
        // Retransmit on handshake, stall as peer stopped.
        if (!m_dtls->isConnectionEncrypted()) {
            qDebug() << "DTLS handshake retry";
            m_dtls->handleTimeout(m_udpSocket);
        } else {
            qWarning() << "Established DTLS session timed out";
            Q_EMIT error(tr("Peer stopped responding."));
            close();
        }
    });

    QObject::connect(this, &NetworkLink::connectionReady, &m_inactivityTimer, qOverload<>(&QTimer::start));
    QObject::connect(&m_inactivityTimer, &QTimer::timeout, this, &NetworkLink::onConnectionTimeout);
    m_inactivityTimer.setTimerType(Qt::VeryCoarseTimer);
    m_inactivityTimer.setSingleShot(true);
    m_inactivityTimer.setInterval(Settings::inactivityTimeout + 5000);
    m_inactivityTimer.stop();

    m_telemetryTimer.setInterval(2000);
    m_telemetryTimer.setTimerType(Qt::VeryCoarseTimer);
    m_telemetryTimer.stop();
    QObject::connect(&m_telemetryTimer, &QTimer::timeout, this, &NetworkLink::onTelemetryTimeout);

    QObject::connect(this, &NetworkLink::connectionReady, &m_telemetryTimer, qOverload<>(&QTimer::start));
    QObject::connect(this, &NetworkLink::closed, &m_telemetryTimer, &QTimer::stop);
}

NetworkLink::~NetworkLink()
{
    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }

    m_inactivityTimer.stop();
    m_udpSocket->close();
    m_sslSocket->close();
}

NetworkLink *NetworkLink::createForPlatform(QObject *parent)
{
    return new Platform::NetworkLink(parent);
}

void NetworkLink::close()
{
    if (m_sslSocket->state() == QAbstractSocket::UnconnectedState
        && m_udpSocket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }

    m_inactivityTimer.stop();
    m_udpSocket->close();
    m_sslSocket->close();
}

void NetworkLink::resetSessionState()
{
    m_buffer.clear();
    m_protocol.clear();
    m_connectionReady = false;
    m_format = 0;
    m_width = 0;
    m_height = 0;
    m_stride = 0;
    m_imageSize = 0;
    m_rw.jitterBuffer().clear();
    m_inactivityTimer.stop();
    m_telemetryTimer.stop();
    m_lastTelemetryNs = 0;
}

void NetworkLink::connect(const QString &address, const int port, const QString &clientName, const QString &protocolName)
{
    m_sslSocket->close();
    m_udpSocket->close();

    if (m_sslSocket->state() != QAbstractSocket::UnconnectedState
        || m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "Invalid socket state";
        return;
    }

    // Reset all states from previous sessions.
    resetSessionState();

    qInfo() << "Connecting to:" << address << port << "using client" << clientName << "with protocol" << protocolName;

    const auto clientData = FileProvider::instance()->clientData(clientName);
    if (clientData.first.isNull()) {
        Q_EMIT error(tr("The certificate of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }
    if (clientData.second.isNull()) {
        Q_EMIT error(tr("The key of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }

    auto sslConf = protocolName == "dtls" ? QSslConfiguration::defaultDtlsConfiguration()
                                          : QSslConfiguration::defaultConfiguration();
    sslConf.setCaCertificates({});
    sslConf.setPeerVerifyMode(QSslSocket::VerifyNone); // Avoid chain validation
    sslConf.setLocalCertificate(clientData.first);
    sslConf.setPrivateKey(clientData.second);

    const QHostAddress hostAddress(address);

    if (protocolName == "dtls") {
        connectDtls(sslConf, hostAddress, port);
    } else if (protocolName == "ssl") {
        connectSsl(sslConf, hostAddress, port);
    } else {
        qWarning() << "Unknown protocol name requested to connect to server:" << protocolName;
        Q_EMIT error(tr("The protocol is invalid: %1").arg(protocolName));
    }
}

void NetworkLink::connectDtls(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, const int port)
{
    qDebug() << "Connecting over DTLS";

    m_protocol = "dtls";

    m_dtls->setDtlsConfiguration(sslConf);
    m_dtls->setMtuHint(Settings::dtlsChunkSize);
    m_dtls->setPeer(hostAddress, static_cast<quint16>(port));

    // Bind ephemeral local port
    // [TODO] Should automatically switch between IPv4 & v6.
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, 0)) {
        qWarning() << "Failed to bind UDP socket:" << m_udpSocket->errorString();
        Q_EMIT error(m_udpSocket->errorString());
        return;
    }

    // Connect the UDP socket to the remote peer so readDatagram() only
    // yields packets from the peer.
    m_udpSocket->connectToHost(hostAddress, static_cast<quint16>(port));

    qDebug() << "Performing handshake";
    if (!m_dtls->doHandshake(m_udpSocket)) {
        const auto errorString = m_dtls->dtlsErrorString();
        qDebug() << "DTLS error:" << errorString;
        Q_EMIT error(tr("Failed to start DTLS handshake: %1").arg(errorString));
        return;
    }

    if (m_dtls->isConnectionEncrypted()) {
        qDebug() << "DTLS connection ready";
        m_connectionReady = true;
        Q_EMIT connectionReady();
    }

    qInfo() << "DTLS handshake started";
}

void NetworkLink::connectSsl(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, const int port)
{
    qDebug() << "Connecting over SSL";

    m_protocol = "ssl";
    m_sslSocket->setSslConfiguration(sslConf);

    // connectToHostEncrypted() does TCP connect + TLS handshake
    // asynchronously. connectionReady is handled in encrypted() handler in the ctor.
    m_sslSocket->connectToHostEncrypted(hostAddress.toString(), static_cast<quint16>(port));
}

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    qWarning() << "Connection error occurred: " << error;

    if (m_protocol == "dtls" && m_udpSocket) {
        Q_EMIT NetworkLink::error(m_udpSocket->errorString());
    } else if (m_protocol == "ssl" && m_sslSocket) {
        Q_EMIT NetworkLink::error(m_sslSocket->errorString());
    } else {
        Q_EMIT NetworkLink::error(tr("A network error occurred."));
    }
}

void NetworkLink::onSslError(const QSslError &error)
{
    qWarning() << "SSL Connection error occurred: " << error;
}

void NetworkLink::onUdpConnected()
{
    qInfo() << "UDP socket connected";
    Q_EMIT connectionInitialised();
}

void NetworkLink::onSslConnected()
{
    qInfo() << "TCP socket connected";
    Q_EMIT connectionInitialised();
    // connectionReady emission from onSslEncrypted on TLS completion.
}

void NetworkLink::onSslEncrypted()
{
    qInfo() << "SSL handshake completed";
    m_connectionReady = true;
    Q_EMIT connectionReady();
}

void NetworkLink::onDtlsDisconnected()
{
    m_udpSocket->close();
    m_buffer.clear();
    m_inactivityTimer.stop();
    m_rw.jitterBuffer().clear();
    m_connectionReady = false;
}

void NetworkLink::onSslDisconnected()
{
    m_sslSocket->close();
    m_connectionReady = false;
}

void NetworkLink::onDtlsDataAvailable()
{
    m_inactivityTimer.start();

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray dgram(m_udpSocket->pendingDatagramSize(), Qt::Uninitialized);
        const auto bytesRead = m_udpSocket->readDatagram(dgram.data(), dgram.size());

        if (bytesRead <= 0) {
            qWarning() << "Spurious UDP read";
            continue;
        }

        dgram.resize(bytesRead);

        if (!m_dtls->isConnectionEncrypted()) {
            // Continue handshake with incoming datagram.
            qDebug() << "Performing handshake";
            if (!m_dtls->doHandshake(m_udpSocket, dgram)) {
                if (m_dtls->dtlsError() != QDtlsError::NoError) {
                    const auto errorString = m_dtls->dtlsErrorString();
                    qDebug() << "DTLS error:" << errorString;
                    Q_EMIT error(tr("DTLS handshake error: %1").arg(errorString));
                }

                // Another datagram may still complete the handshake, continue queue processing.
                continue;
            }

            if (m_dtls->isConnectionEncrypted()) {
                qInfo() << "DTLS encrypted connection established";
                m_connectionReady = true;
                Q_EMIT connectionReady();
            }

            continue;
        }

        m_rw.processDtls(dgram);
    }
}

void NetworkLink::onSslDataAvailable()
{
    m_inactivityTimer.start();

    const auto plain = m_sslSocket->readAll();
    if (!plain.isEmpty()) {
        pushData(std::move(plain));
        return;
    }

    if (m_sslSocket->state() == QAbstractSocket::UnconnectedState) {
        qWarning() << "SSL shutdown received";
        m_sslSocket->close();
        return;
    }

    qWarning() << "Received empty SSL payload";
}

void NetworkLink::onConnectionTimeout()
{
    const bool sslActive  = m_sslSocket->state() == QAbstractSocket::ConnectedState;
    const bool dtlsActive = m_dtls && m_dtls->isConnectionEncrypted();

    if (!sslActive && !dtlsActive) {
        return;
    }

    qDebug() << "Connection timed out";
    Q_EMIT error(tr("Connection timed out."));
    close();
}

void NetworkLink::onTelemetryTimeout()
{
    if (!m_connectionReady) {
        return;
    }

    const auto nowNs = steadyNowNs();
    if (m_lastTelemetryNs == 0) {
        m_lastTelemetryNs = nowNs;
        return;
    }

    const auto windowNs = nowNs - m_lastTelemetryNs;
    if (windowNs < 500'000'000LL) {
        // don't spam a sub-0.5 s window
        return;
    }

    const auto snap = m_telemetry.takeSnapshot(windowNs);
    m_lastTelemetryNs = nowNs;

    if (snap.fpsX10 == 0 && snap.droppedFrames == 0 && snap.latencyMsAvg == 0) {
        // nothing was rendered, don't wake the server
        return;
    }

    Packets::ClientTelemetry t{};
    t.fpsX10.data = snap.fpsX10;
    t.latencyMsAvg.data = snap.latencyMsAvg;
    t.latencyMsP95.data = snap.latencyMsP95;
    t.droppedFrames.data = snap.droppedFrames;
    t.recvRateKbps.data = snap.recvRateKbps;
    t.decodeQueueDepth.data = snap.decodeQueueDepth;
    t.renderQueueDepth.data = snap.renderQueueDepth;

    qDebug() << "Sending telemetry:"
             << "fps=" << (snap.fpsX10 / 10.0)
             << "avg=" << snap.latencyMsAvg << "ms"
             << "p95=" << snap.latencyMsP95 << "ms"
             << "drops=" << snap.droppedFrames
             << "kbps=" << snap.recvRateKbps
             << "dQ=" << snap.decodeQueueDepth
             << "rQ=" << snap.renderQueueDepth;

    write(Packets::Writer::generate(t));
}

void NetworkLink::write(const QByteArray &data)
{
    /*
     * Dispatch on encryption state, not on socket state: a UDP socket is
     * "ConnectedState" as soon as connectToHost() returns, well before the
     * DTLS handshake finishes. Same for QSslSocket vs. its TCP layer.
     */

    if (m_dtls && m_dtls->isConnectionEncrypted()) {
        writeDtls(data);
    } else if (m_sslSocket && m_sslSocket->isEncrypted()) {
        writeSsl(data);
    } else {
        qWarning() << "write() called before transport is ready";
    }
}

qint64 NetworkLink::writeDtls(const QByteArray &data)
{
    return m_rw.writeDtls(data);
}

qint64 NetworkLink::writeSsl(const QByteArray &data)
{
    /*
     * Non-blocking: QSslSocket buffers internally. If callers need to know
     * when the kernel has actually accepted the bytes, they can watch
     * QSslSocket::bytesWritten.
     */

    const auto ret = m_sslSocket->write(data);
    if (ret < 0) {
        qWarning() << "SSL write failed:" << m_sslSocket->errorString();
    }

    return ret;
}
