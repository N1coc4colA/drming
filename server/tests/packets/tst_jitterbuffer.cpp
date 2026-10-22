#include <QtTest>

#include "../net/parser.h"

class TestJitterBuffer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void outOfOrderAssembly();
    void duplicateChunksAreIgnored();
    void evictsOldTimestamps();
    void emptyUntilComplete();
};

void TestJitterBuffer::outOfOrderAssembly()
{
    Packets::JitterBuffer<> jb;
    jb.pushChunk(1, 2, 3, "third");
    jb.pushChunk(1, 1, 3, "second");
    QVERIFY(!jb.pullComplete().has_value());

    jb.pushChunk(1, 0, 3, "first");
    const auto out = jb.pullComplete();
    QVERIFY(out.has_value());
    QCOMPARE(*out, QByteArrayLiteral("firstsecondthird"));
    QVERIFY(!jb.pullComplete().has_value()); // drained
}

void TestJitterBuffer::duplicateChunksAreIgnored()
{
    Packets::JitterBuffer<> jb;
    jb.pushChunk(7, 0, 2, "a");
    jb.pushChunk(7, 0, 2, "XXX"); // duplicate order — must be ignored
    jb.pushChunk(7, 1, 2, "b");
    QCOMPARE(*jb.pullComplete(), QByteArrayLiteral("ab"));
}

void TestJitterBuffer::evictsOldTimestamps()
{
    Packets::JitterBuffer<> jb; // MaxTimestamps == 3
    for (Packets::Timestamp ts = 0; ts < 5; ++ts)
        jb.pushChunk(ts, 0, 1, QByteArray::number(ts));

    // ts 0 and 1 were evicted, ts 2 is the oldest complete
    QCOMPARE(*jb.pullComplete(), QByteArrayLiteral("2"));
    QCOMPARE(*jb.pullComplete(), QByteArrayLiteral("3"));
    QCOMPARE(*jb.pullComplete(), QByteArrayLiteral("4"));
}

void TestJitterBuffer::emptyUntilComplete()
{
    Packets::JitterBuffer<> jb;
    jb.pushChunk(0, 0, 2, "a");
    QVERIFY(!jb.pullComplete().has_value());
}

QTEST_GUILESS_MAIN(TestJitterBuffer)
#include "tst_jitterbuffer.moc"
