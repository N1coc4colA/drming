#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslConfiguration>

#include "../settings.h"

#include "fileprovider.h"

#ifdef Q_OS_ANDROID
#include "platform/android/networklink.h"
#else
#include "platform/linux/networklink.h"
#endif

NetworkLink::NetworkLink(QObject *parent)
    : QObject(parent)
    , m_udpSocket(new QUdpSocket(this))
    , m_dtls(new QDtls(QSslSocket::SslClientMode, this))
    , m_rw(*this, m_dtls, m_udpSocket, nullptr)
{
    QObject::connect(m_udpSocket, &QUdpSocket::connected, this, &NetworkLink::onUdpConnected);
    QObject::connect(m_udpSocket, &QUdpSocket::disconnected, this, &NetworkLink::onDtlsDisconnected);
    QObject::connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkLink::onDtlsDataAvailable);
    QObject::connect(m_udpSocket, &QUdpSocket::errorOccurred, this, &NetworkLink::onError);

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
}

NetworkLink *NetworkLink::createForPlatform(QObject *parent)
{
    return new Platform::NetworkLink(parent);
}

void NetworkLink::close()
{
    if (m_udpSocket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    if (m_dtls && m_dtls->handshakeState() == QDtls::HandshakeComplete) {
        m_dtls->shutdown(m_udpSocket);
    }

    m_inactivityTimer.stop();
    m_udpSocket->close();

    closePlatform();
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

void NetworkLink::connect(const QString &address, const int port, const QString &clientName)
{
    m_udpSocket->close();

    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "Invalid socket state";
        return;
    }

    // Reset all states from previous sessions.
    resetSessionState();

    qInfo() << "Connecting to:" << address << port << "using client" << clientName;

    const auto [cert, key] = FileProvider::instance()->clientData(clientName);
    if (cert.isNull()) {
        Q_EMIT error(tr("The certificate of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }
    if (key.isNull()) {
        Q_EMIT error(tr("The key of '%1' that was about to be used is invalid.").arg(clientName));
        return;
    }

    auto sslConf = QSslConfiguration::defaultDtlsConfiguration();
    sslConf.setCaCertificates({});
    sslConf.setPeerVerifyMode(QSslSocket::VerifyNone); // Avoid chain validation
    sslConf.setLocalCertificate(cert);
    sslConf.setPrivateKey(key);

    const QHostAddress hostAddress(address);

    connectDtls(sslConf, hostAddress, port);
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

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    qWarning() << "Connection error occurred: " << error;

    if (m_protocol == "dtls" && m_udpSocket) {
        Q_EMIT NetworkLink::error(m_udpSocket->errorString());
    } else {
        Q_EMIT NetworkLink::error(tr("A network error occurred."));
    }
}

void NetworkLink::onUdpConnected()
{
    qInfo() << "UDP socket connected";
    Q_EMIT connectionInitialised();
}

void NetworkLink::onDtlsDisconnected()
{
    m_udpSocket->close();
    m_buffer.clear();
    m_inactivityTimer.stop();
    m_rw.jitterBuffer().clear();
    m_connectionReady = false;
}

void NetworkLink::onDtlsDataAvailable()
{
    markActivity();

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

void NetworkLink::markActivity()
{
    m_inactivityTimer.start();
}

void NetworkLink::onConnectionTimeout()
{
    if (!m_dtls || !m_dtls->isConnectionEncrypted()) {
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

    if (snap.receivedFrames == 0 && snap.droppedFrames == 0 && snap.latencyMsAvg == 0) {
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

#ifdef TELEMETRY_OUTPUT
    qDebug() << "Sending telemetry:"
             << "fps=" << (snap.fpsX10 / 10.0) << "avg=" << snap.latencyMsAvg << "ms"
             << "p95=" << snap.latencyMsP95 << "ms"
             << "drops=" << snap.droppedFrames << "kbps=" << snap.recvRateKbps << "dQ=" << snap.decodeQueueDepth
             << "rQ=" << snap.renderQueueDepth;
#endif

    write(Packets::Writer::generate(t));
}

std::optional<quint64> NetworkLink::write(const QByteArray &data)
{
    /*
     * Dispatch on encryption state, not on socket state: a UDP socket is
     * "ConnectedState" as soon as connectToHost() returns, well before the
     * DTLS handshake finishes.
     */

    if (m_dtls && m_dtls->isConnectionEncrypted()) {
        return writeDtls(data);
    }

    qWarning() << "write() called before transport is ready";

    return {};
}

std::optional<quint64> NetworkLink::writeDtls(const QByteArray &data)
{
    return m_rw.writeDtls(data);
}
