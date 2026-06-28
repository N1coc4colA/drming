#ifndef SSE_H
#define SSE_H

#include <QImage>
#include <cstdint>
#include <optional>

namespace DrmFormat {

// A function that swizzles a QImage in-place from a DRM layout into the
// layout Qt expects for the accompanying qtFormat.  nullptr = already correct.
using ConvertFn = void (*)(QImage &);

struct FormatDescriptor
{
    QImage::Format qtFormat = QImage::Format_Invalid;
    ConvertFn convert = nullptr; // nullptr → no conversion needed
};

// Returns nullopt for DRM formats that have no usable Qt equivalent
// (sub-byte packed, darkness, two-channel, 10-bit packed variants, etc.).
std::optional<FormatDescriptor> resolve(const uint32_t drmFourcc);

} // namespace DrmFormat

#endif // SSE_H
