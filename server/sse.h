#ifndef SSE_H
#define SSE_H

#include <QImage>
#include <cstdint>
#include <optional>

namespace DrmFormat {

/// Converts a DRM channel layout in-place into the associated Qt layout.
/// A null function means that no conversion is required.
using ConvertFn = void (*)(QImage &);

/// Format descriptor holding Qt format and optional in-place channel conversion.
struct FormatDescriptor
{
    /// Equivalent Qt image format.
    QImage::Format qtFormat = QImage::Format_Invalid;
    /**
     * In-place pixel conversion function, or nullptr if no conversion is needed.
     */
    ConvertFn convert = nullptr;
};

/**
 * Maps a DRM fourcc code to a Qt format and optional channel conversion.
 * Returns nullopt when Qt cannot represent the DRM format safely.
 */
std::optional<FormatDescriptor> resolve(uint32_t drmFourcc);

} // namespace DrmFormat

#endif // SSE_H
