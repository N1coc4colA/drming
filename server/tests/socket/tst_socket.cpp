#include <QtTest>

#include <drm_fourcc.h>

#include "displayreader.h"
#include "ffmpeg.h"
#include "parameters.h"
#include "streamsocket.h"
#include "utils.h"

class TestStreamSocket final : public StreamSocket
{
public:
    using StreamSocket::StreamSocket;

    void addData(QByteArray) override {}
};

class TestServerCore : public QObject
{
    Q_OBJECT

private slots:
    void fourccSplitting()
    {
        uint8_t a = 0, b = 0, c = 0, d = 0;
        Drm::split_fourcc(DRM_FORMAT_XRGB8888, a, b, c, d);
        QCOMPARE(a, uint8_t{'X'});
        QCOMPARE(b, uint8_t{'R'});
        QCOMPARE(c, uint8_t{'2'});
        QCOMPARE(d, uint8_t{'4'});

        char ca = 0, cb = 0, cc = 0, cd = 0;
        Drm::split_fourcc(DRM_FORMAT_ARGB8888, ca, cb, cc, cd);
        QByteArray chars;
        chars.append(ca);
        chars.append(cb);
        chars.append(cc);
        chars.append(cd);
        QCOMPARE(chars, QByteArrayLiteral("AR24"));
    }

    void imageFromFramebufferPreservesStrideAndConverts()
    {
        constexpr uint32_t width = 2;
        constexpr uint32_t height = 1;
        constexpr uint32_t stride = 8;
        uchar pixels[stride] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
        const auto descriptor = DrmFormat::resolve(DRM_FORMAT_ABGR8888);
        QVERIFY(descriptor.has_value());

        const auto image = DisplayReader::imageFromFrameBuffer(pixels, width, height, stride, *descriptor);
        QCOMPARE(image.size(), QSize(2, 1));
        QCOMPARE(image.bytesPerLine(), int(stride));
        const auto *converted = image.constBits();
        QCOMPARE(converted[0], uchar{0x33});
        QCOMPARE(converted[1], uchar{0x22});
        QCOMPARE(converted[2], uchar{0x11});
        QCOMPARE(converted[3], uchar{0x44});
        QCOMPARE(converted[4], uchar{0x77});
        QCOMPARE(converted[5], uchar{0x66});
        QCOMPARE(converted[6], uchar{0x55});
        QCOMPARE(converted[7], uchar{0x88});
    }

    void cursorCompositeHandlesInvalidAndValidFrames()
    {
        QImage primary(2, 2, QImage::Format_ARGB32);
        primary.fill(Qt::black);
        CursorFrameBuffer invalid{};
        const auto descriptor = DrmFormat::resolve(DRM_FORMAT_ARGB8888);
        QVERIFY(descriptor.has_value());
        DisplayReader::compositeWithCursor(primary, invalid, *descriptor);
        QCOMPARE(primary.pixelColor(0, 0), QColor(Qt::black));

        uchar cursorPixel[4] = {0x00, 0x00, 0xff, 0xff};
        CursorFrameBuffer cursor{};
        cursor.data = cursorPixel;
        cursor.width = 1;
        cursor.height = 1;
        cursor.stride = 4;
        cursor.crtc_x = 1;
        cursor.crtc_y = 1;
        DisplayReader::compositeWithCursor(primary, cursor, *descriptor);
        QCOMPARE(primary.pixelColor(1, 1), QColor(Qt::red));
    }

    void frameBufferReleaseClearsState()
    {
        VkmsFrameBuffer fb{};
        fb.fd = -1;
        fb.buffer_fd = -1;
        fb.width = 12;
        fb.height = 8;
        fb.data = reinterpret_cast<void *>(0x1);
        fb.size = 0;
        DisplayReader::releaseVkmsFrameBuffer(fb);
        QCOMPARE(fb.fd, -1);
        QCOMPARE(fb.width, uint32_t{0});
        QCOMPARE(fb.data, nullptr);

        CursorFrameBuffer cursor{};
        cursor.buffer_fd = -1;
        cursor.fd = 42;
        cursor.width = 4;
        DisplayReader::releaseCursorFrameBuffer(cursor);
        QCOMPARE(cursor.fd, -1);
        QCOMPARE(cursor.width, uint32_t{0});
    }

    void ffmpegPixelFormatMapping()
    {
        QCOMPARE(Ffmpeg::Encoder::formatFromFcc(DRM_FORMAT_ARGB8888), AV_PIX_FMT_ARGB);
        QCOMPARE(Ffmpeg::Encoder::formatFromFcc(DRM_FORMAT_ABGR8888), AV_PIX_FMT_ABGR);
        QCOMPARE(Ffmpeg::Encoder::formatFromFcc(DRM_FORMAT_RGB565), AV_PIX_FMT_RGB565);
        QCOMPARE(Ffmpeg::Encoder::formatFromFcc(0xdeadbeef), AV_PIX_FMT_NONE);
        QCOMPARE(Ffmpeg::Encoder::formatFromQt(QImage::Format_ARGB32), AV_PIX_FMT_BGRA);
        QCOMPARE(Ffmpeg::Encoder::formatFromQt(QImage::Format_RGB32), AV_PIX_FMT_BGR0);
        QCOMPARE(Ffmpeg::Encoder::formatFromQt(QImage::Format_RGB16), AV_PIX_FMT_RGB565LE);
        QTest::ignoreMessage(QtWarningMsg, "QImage::Format unsupported by FFmpeg: QImage::Format_Invalid");
        QCOMPARE(Ffmpeg::Encoder::formatFromQt(QImage::Format_Invalid), AV_PIX_FMT_NONE);
    }

    void ffmpegErrorText()
    {
        QVERIFY(!Ffmpeg::avError(-1).isEmpty());
        QVERIFY(Ffmpeg::avError(0).contains("Success"));
    }

    void parametersHaveExpectedZeroInitialization()
    {
        const Parameters parameters{};
        QCOMPARE(parameters.port, 0);
        QCOMPARE(parameters.audioPort, 0);
        QCOMPARE(parameters.videoPort, 0);
        QCOMPARE(parameters.qualityLevel, 0);
        QCOMPARE(parameters.servedScreens, 0);
        QCOMPARE(parameters.advertise, false);
        QCOMPARE(parameters.streamFormat, Opts::DisplayStreamType{});
        QVERIFY(parameters.serviceIp.isNull());
        QVERIFY(!parameters.serviceIface.isValid());
    }

    void sleepReturnsAndKeyChangesAreObservable()
    {
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(msleep(1), 0);
        QVERIFY(timer.elapsed() >= 0);

        TestStreamSocket socket;
        const auto initialKey = socket.getKey();
        const auto initialStamp = socket.getKeystamp();
        QSignalSpy keySpy(&socket, &UdpStreamSocket::keyChanged);
        socket.setKey(initialKey, initialStamp);
        QCOMPARE(keySpy.count(), 0);
        socket.setKey(QByteArrayLiteral("server-test-key"), 12);
        QCOMPARE(socket.getKey(), QByteArrayLiteral("server-test-key"));
        QCOMPARE(socket.getKeystamp(), Packets::Keystamp{12});
        QCOMPARE(keySpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestServerCore)
#include "tst_socket.moc"
