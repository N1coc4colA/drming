#ifndef DISPLAYREADER_H
#define DISPLAYREADER_H

#include <QImage>
#include <QPixmap>
#include <QString>

#include "vkmsfb.h"

class DisplayReader
{
public:
    explicit DisplayReader(const QString &connectorName);

    bool getVkmsFrameBuffer(VkmsFrameBuffer &fb);
    static void releaseVkmsFrameBuffer(VkmsFrameBuffer &fb);

    bool getCursorFrameBuffer(CursorFrameBuffer &cursor, const VkmsFrameBuffer &primary);
    static void releaseCursorFrameBuffer(CursorFrameBuffer &cursor);

    static QImage compositeWithCursor(const QImage &primary, const CursorFrameBuffer &cursor);

    static QImage imageFromFrameBuffer(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format);
    static QPixmap pixmapFromFrameBuffer(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format);

private:
    const QString m_connectorName{};
};

QImage::Format qtImageFormatFromDrm(uint32_t drmFormat);

#endif // DISPLAYREADER_H
