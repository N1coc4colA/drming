#ifndef DISPLAYREADER_H
#define DISPLAYREADER_H

#include <QString>

#include "sse.h"
#include "vkmsfb.h"

class DisplayReader
{
public:
    explicit DisplayReader(QString connectorName);

    bool getVkmsFrameBuffer(VkmsFrameBuffer &fb) const;
    static void releaseVkmsFrameBuffer(VkmsFrameBuffer &fb);

    static bool getCursorFrameBuffer(CursorFrameBuffer &cursor, const VkmsFrameBuffer &primary);
    static void releaseCursorFrameBuffer(CursorFrameBuffer &cursor);

    static QImage compositeWithCursor(const QImage &primary, const CursorFrameBuffer &cursor, const DrmFormat::FormatDescriptor &fmtDesc);

    static QImage imageFromFrameBuffer(
        const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, const DrmFormat::FormatDescriptor &fmtDesc);

private:
    const QString m_connectorName{};
};

namespace Drm {

inline void split_fourcc(const uint32_t code, uint8_t &a, uint8_t &b, uint8_t &c, uint8_t &d)
{
    a = static_cast<uint8_t>((code >> 0) & 0xff);
    b = static_cast<uint8_t>((code >> 8) & 0xff);
    c = static_cast<uint8_t>((code >> 16) & 0xff);
    d = static_cast<uint8_t>((code >> 24) & 0xff);
}

inline void split_fourcc(const uint32_t code, char &a, char &b, char &c, char &d)
{
    a = static_cast<char>((code >> 0) & 0xff);
    b = static_cast<char>((code >> 8) & 0xff);
    c = static_cast<char>((code >> 16) & 0xff);
    d = static_cast<char>((code >> 24) & 0xff);
}

} // namespace Drm

#endif // DISPLAYREADER_H
