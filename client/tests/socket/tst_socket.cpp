#include <QtTest>

#include <QBuffer>
#include <QFile>
#include <QTemporaryDir>

#include "../net/udp.h"
#include "fileprovider.h"
#include "imagedecoder.h"
#include "mdns.h"
#include "models/filesmodel.h"
#include "models/servicesmodel.h"
#include "networkstatus.h"
#include "palette.h"

class TestUdpStreamSocket final : public UdpStreamSocket
{
public:
    using UdpStreamSocket::UdpStreamSocket;

    void addData(const QByteArray &) override
    {}
};

class TestMdns final : public Mdns
{
public:
    using Mdns::Mdns;

    void startDiscovery() override
    {}
    void stopDiscovery() override
    {}
};

class TestClient : public QObject
{
    Q_OBJECT

public:
    TestClient()
        : m_mdns(nullptr)
    {}

private Q_SLOTS:
    void serviceInfoEqualityAndHash()
    {
        const ServiceInfo first{ "name", "type", "host", "127.0.0.1", 443 };
        const ServiceInfo same = first;
        const ServiceInfo different{ "name", "type", "host", "127.0.0.1", 445 };

        QCOMPARE(first.toHashable(), QStringLiteral("nametypehost127.0.0.1443"));
        QVERIFY(first == same);
        QVERIFY(first != different);
        QCOMPARE(std::hash<ServiceInfo>{}(first), qHash(first.toHashable()));
    }

    void mdnsServiceLifecycle()
    {
        QSignalSpy foundSpy(&m_mdns, &Mdns::serviceFound);
        QSignalSpy resolvedSpy(&m_mdns, &Mdns::serviceResolved);
        QSignalSpy lostSpy(&m_mdns, &Mdns::serviceLost);
        QSignalSpy countSpy(&m_mdns, &Mdns::countChanged);

        m_mdns.onServiceFound("desk", "_drming._udp");
        QCOMPARE(m_mdns.count(), 1);
        QCOMPARE(foundSpy.count(), 1);
        QCOMPARE(foundSpy.at(0).at(0).toString(), QStringLiteral("desk*"));

        m_mdns.onServiceResolved("desk", "desk.local", "192.0.2.1", 443);
        QCOMPARE(m_mdns.count(), 2);
        QCOMPARE(resolvedSpy.count(), 1);

        m_mdns.onServiceLost("desk", "192.0.2.1");
        QCOMPARE(m_mdns.count(), 1);
        QCOMPARE(lostSpy.count(), 1);

        m_mdns.onServiceLost("desk", {});
        QCOMPARE(m_mdns.count(), 0);
        QCOMPARE(lostSpy.count(), 2);
        QCOMPARE(countSpy.count(), 3);
    }

    void servicesModelReflectsResolvedServices()
    {
        ServicesModel model;

        m_mdns.onServiceResolved("camera", "camera.local", "192.0.2.2", 8443);
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.rowCount({}), 1);
        QCOMPARE(model.rowCount(model.index(0, 0)), 0);
        QCOMPARE(model.data(model.index(0, 0), ServicesModel::NameRole).toString(), QStringLiteral("camera"));
        QCOMPARE(model.data(model.index(0, 0), ServicesModel::HostRole).toString(), QStringLiteral("camera.local"));
        QCOMPARE(model.data(model.index(0, 0), ServicesModel::IpRole).toString(), QStringLiteral("192.0.2.2"));
        QCOMPARE(model.data(model.index(0, 0), ServicesModel::PortRole).toInt(), 8443);
        QCOMPARE(model.data(model.index(0, 0), ServicesModel::TypeRole).toString(), QString());
        QCOMPARE(model.get(0).value("name").toString(), QStringLiteral("camera"));
        QVERIFY(model.get(-1).isEmpty());
        QVERIFY(model.get(1).isEmpty());
        QVERIFY(!model.data({}, ServicesModel::NameRole).isValid());

        m_mdns.onServiceResolved("camera", "new.local", "192.0.2.3", 9443);
        // It is possible that due to Avahi's behaviour, a same iface with multiple IPs advertize
        // this same number of service on each IP although you explicitly told it to do so on only
        // one IP of one iface.
        QCOMPARE(model.count(), 2);
        QCOMPARE(model.get(0).value("host").toString(), QStringLiteral("camera.local"));
        QCOMPARE(model.get(1).value("host").toString(), QStringLiteral("new.local"));

        m_mdns.onServiceLost("camera", "192.0.2.3");
        QCOMPARE(model.count(), 1);
        model.clear();
    }

    void filesModelRolesAndBounds()
    {
        FilesModel model;
        const QDateTime timestamp(QDate(2026, 10, 19), QTime(18, 9));
        model.setContent({ { timestamp, "client.crt", { { "valid", true } } } });

        QCOMPARE(model.count(), 1);
        QCOMPARE(model.rowCount({}), 1);
        QCOMPARE(model.data(model.index(0, 0), FilesModel::NameRole).toString(), QStringLiteral("client.crt"));
        QCOMPARE(model.data(model.index(0, 0), FilesModel::InfoRole).toMap().value("valid").toBool(), true);
        QVERIFY(!model.data(model.index(0, 0), Qt::UserRole + 99).isValid());
        QVERIFY(model.get(-1).isEmpty());
        QVERIFY(model.get(1).isEmpty());
        QCOMPARE(model.get(0).value("name").toString(), QStringLiteral("client.crt"));
        QVERIFY(!model.data({}, FilesModel::NameRole).isValid());
    }

    void paletteRoundTripAndMissingFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("palette.data");
        QPalette palette;
        palette.setColor(QPalette::Window, QColor("#123456"));
        qApp->setPalette(palette);

        QCOMPARE(writePalette(path), true);
        const auto pal = readPalette(path);

        QCOMPARE(pal.has_value(), true);
        QCOMPARE(pal.value().color(QPalette::Window), QColor("#123456"));
        const auto missing = readPalette(directory.filePath("missing.data"));
        QVERIFY(!missing.has_value());
    }

    void imageDecoderDecodesAndResets()
    {
        QImage source(2, 1, QImage::Format_RGB32);
        source.setPixelColor(0, 0, Qt::red);
        source.setPixelColor(1, 0, Qt::green);
        QByteArray encoded;
        QBuffer buffer(&encoded);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(source.save(&buffer, "PNG"));

        ImageDecoder decoder;
        QMutex mutex;
        QWaitCondition condition;
        QList<QImage> frames;
        decoder.setFrameCallback([&](const QImage &image, const qint64) {
            QMutexLocker locker(&mutex);
            frames.append(image);
            condition.wakeAll();
        });
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(R"(ImageDecoder: Failed to decode image of format "PNG" size \d+)")
        );
        decoder.queueFrame("not an image", "PNG", 0);
        decoder.queueFrame(encoded, "PNG", 0);

        QMutexLocker locker(&mutex);
        QVERIFY(condition.wait(&mutex, 2000));
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.first().format(), QImage::Format_RGBA8888);
        QCOMPARE(frames.first().size(), QSize(2, 1));
        decoder.reset();
    }

    void fileProviderModelValidation()
    {
        auto provider = FileProvider::instance(nullptr);
        QVERIFY(provider->clientCertsModel()->get(0).isEmpty());
        QVERIFY(provider->validClientEntries().isEmpty());
        QVERIFY(!provider->updateClientEntry({}));
        QVERIFY(!provider->updateClientEntry({ { "previousName", "old" } }));
        QVERIFY(!provider->updateClientEntry({ { "previousName", "old" }, { "updatedName", "" } }));

        delete provider;
    }

    void networkStateTransitions()
    {
        NetworkState state;
        QSignalSpy connectivitySpy(&state, &NetworkState::connectivityChanged);
        state.onConnectivityChanged(true);
        QVERIFY(state.getConnected());
        QCOMPARE(connectivitySpy.count(), 1);
        QCOMPARE(connectivitySpy.at(0).at(0).toBool(), true);
    }

    void udpKeyAndInvalidMulticast()
    {
        TestUdpStreamSocket socket;
        const auto initialKey = socket.getKey();
        const auto initialStamp = socket.getKeystamp();
        QSignalSpy keySpy(&socket, &UdpStreamSocket::keyChanged);
        socket.setKey(initialKey, initialStamp);
        QCOMPARE(keySpy.count(), 0);
        socket.setKey("test-key", 7);
        QCOMPARE(socket.getKey(), QByteArray("test-key"));
        QCOMPARE(socket.getKeystamp(), Packets::Keystamp(7));
        QCOMPARE(keySpy.count(), 1);
        QTest::ignoreMessage(QtWarningMsg, "Invalid protocol: QAbstractSocket::UnknownNetworkLayerProtocol");
        socket.connectToMulticast(QHostAddress("not-an-address"), 1234);
        socket.close();
    }

private:
    TestMdns m_mdns;
};

QTEST_MAIN(TestClient)
#include "tst_socket.moc"
