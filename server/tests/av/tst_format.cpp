#include <QtTest>

#include <drm_fourcc.h>

#include "encoder/av/libav.h"

class TestAVFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ffmpegPixelFormatMapping()
    {
        QCOMPARE(LibAV::formatFromFcc(DRM_FORMAT_ARGB8888), AV_PIX_FMT_ARGB);
        QCOMPARE(LibAV::formatFromFcc(DRM_FORMAT_ABGR8888), AV_PIX_FMT_ABGR);
        QCOMPARE(LibAV::formatFromFcc(DRM_FORMAT_RGB565), AV_PIX_FMT_RGB565);
        QCOMPARE(LibAV::formatFromFcc(0xdeadbeef), AV_PIX_FMT_NONE);
        QCOMPARE(LibAV::formatFromQt(QImage::Format_ARGB32), AV_PIX_FMT_BGRA);
        QCOMPARE(LibAV::formatFromQt(QImage::Format_RGB32), AV_PIX_FMT_BGR0);
        QCOMPARE(LibAV::formatFromQt(QImage::Format_RGB16), AV_PIX_FMT_RGB565LE);
        QTest::ignoreMessage(QtWarningMsg, "QImage::Format unsupported by FFmpeg: QImage::Format_Invalid");
        QCOMPARE(LibAV::formatFromQt(QImage::Format_Invalid), AV_PIX_FMT_NONE);
    }

    void ffmpegErrorText()
    {
        QVERIFY(!LibAV::avError(-1).isEmpty());
        QVERIFY(LibAV::avError(0).contains("Success"));
    }
};

QTEST_GUILESS_MAIN(TestAVFormat)
#include "tst_format.moc"
