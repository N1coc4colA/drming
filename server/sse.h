#ifndef SSE_H
#define SSE_H

#include <QImage>
#include <cstdint>
#include <optional>

namespace DrmFormat {

/// Converts a DRM channel layout in-place into the associated Qt layout.
/// A null function means that no conversion is required.
using ConvertFn = void (*)(QImage &);

struct FormatDescriptor
{
    QImage::Format qtFormat = QImage::Format_Invalid;
    ConvertFn convert = nullptr; // nullptr → no conversion needed
};

/// Maps a DRM fourcc code to a Qt format and optional channel conversion.
/// Returns nullopt when Qt cannot represent the DRM format safely.
std::optional<FormatDescriptor> resolve(uint32_t drmFourcc);

} // namespace DrmFormat

#endif // SSE_H
