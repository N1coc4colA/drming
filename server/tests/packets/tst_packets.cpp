#include <QtTest>

#include "net/parser.h"

struct CollectingReceiver
{
    std::vector<Packets::PacketVariant> received;
    int errorBatches = 0;

    template<typename T>
    void processPacket(const T &p)
    {
        received.emplace_back(p);
    }

    void onPacketErrors()
    {
        ++errorBatches;
    }
};

class TestPackets : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void emptyPacketRoundTrip();
    void keyUpdateRoundTrip();
    void audioSourceRoundTrip();
    void unknownTypeIsSkipped();
    void oversizedVariableFieldIsRejected();
};

static std::vector<Packets::PacketVariant> roundTrip(const QByteArray &bytes)
{
    CollectingReceiver r;
    Packets::Parser<CollectingReceiver> parser(r);
    parser.pushData(bytes);
    return r.received;
}

void TestPackets::emptyPacketRoundTrip()
{
    Packets::Reinit p;
    const auto bytes = Packets::Writer::generate(p);
    const auto got = roundTrip(bytes);
    QCOMPARE(got.size(), std::size_t{ 1 });
    QVERIFY(std::holds_alternative<Packets::Reinit>(got.front()));
}

void TestPackets::keyUpdateRoundTrip()
{
    Packets::KeyUpdate p;
    p.keyStamp.data = -7;
    p.key.data = QByteArrayLiteral("0123456789abcdef");
    const auto got = roundTrip(Packets::Writer::generate(p));
    QCOMPARE(got.size(), std::size_t{ 1 });
    const auto &ku = std::get<Packets::KeyUpdate>(got.front());
    QCOMPARE(ku.keyStamp.data, qint16{ -7 });
    QCOMPARE(ku.key.data, QByteArrayLiteral("0123456789abcdef"));
}

void TestPackets::audioSourceRoundTrip()
{
    Packets::AudioSource p;
    p.ip.data = QByteArray::fromHex("ef000001"); // 239.0.0.1
    p.port.data = 5004;
    p.isV4.data = true;
    const auto got = roundTrip(Packets::Writer::generate(p));
    const auto &a = std::get<Packets::AudioSource>(got.front());
    QCOMPARE(a.port.data, quint16{ 5004 });
    QCOMPARE(a.isV4.data, true);
    QCOMPARE(a.ip.data, QByteArray::fromHex("ef000001"));
}

void TestPackets::unknownTypeIsSkipped()
{
    // Garbage type word: parser should resync, not crash, and count an error.
    QByteArray bogus;
    bogus.append(char(0xff));
    bogus.append(char(0xff));

    Packets::Reinit p;
    const auto valid = Packets::Writer::generate(p);

    CollectingReceiver r;
    Packets::Parser<CollectingReceiver> parser(r);
    parser.pushData(bogus + valid);

    QCOMPARE(r.received.size(), std::size_t{ 1 });
    QVERIFY(std::holds_alternative<Packets::Reinit>(r.received.front()));
}

void TestPackets::oversizedVariableFieldIsRejected()
{
    // ServerStream::data has a default 8 MiB cap; try to lie about the size.
    Packets::ServerStream p;
    p.data.data = QByteArray(1024, 'x');
    auto bytes = Packets::Writer::generate(p);
    // Blow the length prefix past the cap (it's the first field after the type).
    // Header: 2 bytes type + 8 bytes size prefix.
    bytes[2] = bytes[3] = bytes[4] = bytes[5] = char(0xff);
    bytes[6] = bytes[7] = bytes[8] = bytes[9] = char(0xff);

    CollectingReceiver r;
    Packets::Parser<CollectingReceiver> parser(r);
    parser.pushData(bytes);
    QVERIFY(r.received.empty());
}

QTEST_GUILESS_MAIN(TestPackets)
#include "tst_packets.moc"
