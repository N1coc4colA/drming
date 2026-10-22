#include <QtTest>

#include <drm_fourcc.h>

#include "sse.h"

class TestDrmFormat : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void directLayoutsHaveNoConvertFn()
    {
        for (uint32_t fourcc : {DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888,
                                DRM_FORMAT_RGB888,   DRM_FORMAT_BGR888,
                                DRM_FORMAT_RGB565}) {
            const auto d = DrmFormat::resolve(fourcc);
            QVERIFY2(d.has_value(), qPrintable(QString::number(fourcc, 16)));
            QVERIFY2(d->convert == nullptr, qPrintable(QString::number(fourcc, 16)));
        }
    }

    void swappedLayoutsHaveConvertFn()
    {
        for (uint32_t fourcc : {DRM_FORMAT_ABGR8888, DRM_FORMAT_XBGR8888,
                                DRM_FORMAT_BGR565,   DRM_FORMAT_BGRA8888,
                                DRM_FORMAT_BGRX8888}) {
            const auto d = DrmFormat::resolve(fourcc);
            QVERIFY(d.has_value());
            QVERIFY2(d->convert != nullptr, qPrintable(QString::number(fourcc, 16)));
        }
    }

    void unknownFourccIsNullopt()
    {
        QVERIFY(!DrmFormat::resolve(0xdeadbeef).has_value());
    }

    void abgrToArgbSwizzles()
    {
        // One pixel, bytes [R,G,B,A] in DRM's ABGR8888 little-endian layout.
        // After swizzle, expect [B,G,R,A] (Qt ARGB32's memory layout).
        QImage img(1, 1, QImage::Format_ARGB32);
        auto *p = img.bits();
        p[0] = 0x11; p[1] = 0x22; p[2] = 0x33; p[3] = 0x44; // R G B A

        const auto d = DrmFormat::resolve(DRM_FORMAT_ABGR8888);
        d->convert(img);

        QCOMPARE(p[0], uchar{0x33});
        QCOMPARE(p[1], uchar{0x22});
        QCOMPARE(p[2], uchar{0x11});
        QCOMPARE(p[3], uchar{0x44});
    }
};

QTEST_GUILESS_MAIN(TestDrmFormat)
#include "tst_drmformat.moc"
