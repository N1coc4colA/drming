#include <QtTest>

#include "commandparser.h"
#include "parameters.h"

class TestCommandParser : public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        Parameters::instance = Parameters{};  // fresh per test
    }

    void defaultsAreAccepted()
    {
        CommandParser p;
        QCOMPARE(p.parse({"drming"}), CommandParser::Continue);
        QCOMPARE(Parameters::instance.port, 80);
        QCOMPARE(Parameters::instance.videoPort, 80);
        QCOMPARE(Parameters::instance.streamFormat, Opts::DisplayStreamType::h265);
        QCOMPARE(Parameters::instance.qualityLevel, 70);
    }

    void portBelowRangeRejected()
    {
        CommandParser p;
        QCOMPARE(p.parse({"drming", "--port", "0"}), CommandParser::Failure);
    }

    void portAboveRangeRejected()
    {
        CommandParser p;
        QCOMPARE(p.parse({"drming", "-p", "70000"}), CommandParser::Failure);
    }

    void nonNumericQualityRejected()
    {
        CommandParser p;
        QCOMPARE(p.parse({"drming", "-q", "high"}), CommandParser::Failure);
    }

    void invalidStreamFormatRejected()
    {
        CommandParser p;
        QCOMPARE(p.parse({"drming", "-f", "vp9"}), CommandParser::Failure);
    }

    void nonMulticastAudioIpRejected()
    {
        // Default audio IP is multicast; force a unicast one.
        CommandParser p;
        QCOMPARE(p.parse({"drming", "--ip-audio", "192.168.1.1"}), CommandParser::Failure);
    }

    void validCustomConfiguration()
    {
        CommandParser p;
        QCOMPARE(p.parse({
                     "drming",
                     "-f", "png",
                     "-q", "95",
                     "--port", "9999",
                     "--ip-audio", "239.1.2.3",
                     "--ip-video", "239.1.2.4",
                 }), CommandParser::Continue);
        QCOMPARE(Parameters::instance.streamFormat, Opts::DisplayStreamType::png);
        QCOMPARE(Parameters::instance.qualityLevel, 95);
        QCOMPARE(Parameters::instance.port, 9999);
    }
};

QTEST_GUILESS_MAIN(TestCommandParser)
#include "tst_commandparser.moc"
