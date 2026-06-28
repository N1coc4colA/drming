#include "sse.h"

#include <drm_fourcc.h>
#include <immintrin.h>

#include <algorithm>
#include <array>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  Compile-time swizzle engine — 32-bit pixels
//
//  Mask4::v[i] = "to produce output byte i, take input byte v[i]".
//
//  Qt's in-memory layout for all ARGB32/RGB32 formats (little-endian):
//    byte[0]=B  byte[1]=G  byte[2]=R  byte[3]=A/X
//
//  DRM fourcc channel order is MSB→LSB in a 32-bit word, so on a LE host
//  the byte at the lowest address is the *last* named channel.  For example:
//
//    ARGB8888: word A[31:24]R[23:16]G[15:8]B[7:0]  → bytes [B,G,R,A] = Qt ✓
//    ABGR8888: word A[31:24]B[23:16]G[15:8]R[7:0]  → bytes [R,G,B,A]
//              want [B,G,R,A] → take {src2,src1,src0,src3} = kSwapRB
//    RGBA8888: word R[31:24]G[23:16]B[15:8]A[7:0]  → bytes [A,B,G,R]
//              want [B,G,R,A] → take {src1,src2,src3,src0} = kRotRight
//    BGRA8888: word B[31:24]G[23:16]R[15:8]A[7:0]  → bytes [A,R,G,B]
//              want [B,G,R,A] → take {src3,src2,src1,src0} = kReverse
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct Mask4
{
    uint8_t v[4];
};

// Named 32-bit swizzle masks
constexpr Mask4 kSwapRB = {2, 1, 0, 3};   // ABGR/XBGR → ARGB/XRGB
constexpr Mask4 kRotRight = {1, 2, 3, 0}; // RGBA/RGBX → ARGB/XRGB
constexpr Mask4 kReverse = {3, 2, 1, 0};  // BGRA/BGRX → ARGB/XRGB

struct ShuffleMask
{
    alignas(16) int8_t bytes[16];
    constexpr ShuffleMask()
        : bytes{}
    {}
};

// Build a 16-byte SSSE3 _mm_shuffle_epi8 mask from a 4-byte per-pixel mask.
// Each 128-bit register holds 4 pixels × 4 bytes.  Pixel p occupies bytes
// [p*4 .. p*4+3]; we apply the same per-pixel permutation to all four.
constexpr ShuffleMask make_shuffle32(Mask4 m)
{
    ShuffleMask s{};
    for (int p = 0; p < 4; ++p)
        for (int b = 0; b < 4; ++b)
            s.bytes[p * 4 + b] = int8_t(p * 4 + m.v[b]);
    return s;
}

// One compiled-in SSSE3 function per distinct mask.
// The shuffle constant is fully materialised at compile time; no runtime
// construction, no shared dispatch code.
template<Mask4 M>
__attribute__((target("ssse3"))) static void swizzle32(QImage &img)
{
    // Compile-time: fills a plain int8_t array — legal in constexpr.
    static constexpr ShuffleMask kMask = make_shuffle32(M);
    // Runtime: loaded once on first call, then a dead store eliminated by the
    // optimiser since kShuffle never changes.
    static const __m128i kShuffle = _mm_load_si128(reinterpret_cast<const __m128i *>(kMask.bytes));

    for (int y = 0; y < img.height(); ++y) {
        uchar *line = img.scanLine(y);
        const int bytes = img.width() * 4;
        int x = 0;
        for (; x <= bytes - 16; x += 16) {
            auto *p = reinterpret_cast<__m128i *>(line + x);
            _mm_storeu_si128(p, _mm_shuffle_epi8(_mm_loadu_si128(p), kShuffle));
        }
        for (; x < bytes; x += 4) {
            uchar tmp[4];
            std::memcpy(tmp, line + x, 4);
            line[x + 0] = tmp[M.v[0]];
            line[x + 1] = tmp[M.v[1]];
            line[x + 2] = tmp[M.v[2]];
            line[x + 3] = tmp[M.v[3]];
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  16-bit pixel swizzles (RGB565/BGR565)
//
//  DRM_FORMAT_BGR565: word B[15:11]G[10:5]R[4:0]
//  Qt Format_RGB16:   word R[15:11]G[10:5]B[4:0]
//  Conversion: swap the 5-bit R and B fields, G is unchanged.
// ─────────────────────────────────────────────────────────────────────────────
static void swizzle_bgr565(QImage &img)
{
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<uint16_t *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const uint16_t p = line[x];
            const uint16_t r = (p >> 0) & 0x1Fu;
            const uint16_t g = (p >> 5) & 0x3Fu;
            const uint16_t b = (p >> 11) & 0x1Fu;
            line[x] = uint16_t((r << 11) | (g << 5) | b);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  24-bit pixel swizzles (RGB888/BGR888)
//
//  DRM_FORMAT_BGR888: bytes [B, G, R] in memory
//  Qt Format_RGB888:  bytes [R, G, B] in memory
//  Swap byte[0] ↔ byte[2].
//
//  Note: Qt also has Format_BGR888 (Qt 5.14+), so DRM_FORMAT_BGR888 maps
//  directly — no conversion needed (handled in the table).
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  64-bit pixel swizzles (16 bits per channel)
//
//  Qt Format_RGBX64 / Format_RGBA64 store halfwords [R, G, B, X/A] in memory
//  (little-endian 16-bit units in R→G→B→X/A order).
//
//  DRM_FORMAT_XBGR16161616: word X[63:48]B[47:32]G[31:16]R[15:0]
//    → 16-bit units in memory: [R, G, B, X]  → Qt RGBX64 ✓  (direct)
//  DRM_FORMAT_ABGR16161616: → 16-bit units in memory: [R, G, B, A]
//    → Qt RGBA64 ✓  (direct)
//
//  DRM_FORMAT_XRGB16161616: word X[63:48]R[47:32]G[31:16]B[15:0]
//    → 16-bit units in memory: [B, G, R, X]
//    → Qt RGBX64 wants [R, G, B, X]: swap unit0 ↔ unit2 (R↔B)
//  DRM_FORMAT_ARGB16161616: same, swap R↔B → Qt RGBA64
//
//  The same analysis applies to the float variants (XRGB/XBGR/ARGB/ABGR
//  16161616F and ABGR32323232F).
// ─────────────────────────────────────────────────────────────────────────────

// Swap channel 0 ↔ channel 2 within each N-byte-wide pixel unit.
// Used for both 16-bit-per-channel (unitBytes=2, pixelBytes=8) and
// 32-bit-per-channel (unitBytes=4, pixelBytes=16).
template<int UnitBytes>
static void swizzle_swap_rb_wide(QImage &img)
{
    constexpr int pixelBytes = UnitBytes * 4;
    for (int y = 0; y < img.height(); ++y) {
        uchar *line = img.scanLine(y);
        const int rowBytes = img.width() * pixelBytes;
        for (int x = 0; x < rowBytes; x += pixelBytes) {
            // pixel layout: [ch0, ch1, ch2, ch3] each UnitBytes wide
            // swap ch0 ↔ ch2
            uchar *ch0 = line + x;
            uchar *ch2 = line + x + 2 * UnitBytes;
            for (int b = 0; b < UnitBytes; ++b) {
                std::swap(ch0[b], ch2[b]);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dispatch table
//
//  Each entry is:  { drmFourcc, qtFormat, convertFn }
//  convertFn == nullptr means the DRM memory layout already matches Qt's.
//
//  Formats explicitly NOT supported (and why):
//    C1/C2/C4        — sub-byte indexed, no Qt sub-byte format
//    D1/D2/D4/D8     — "darkness" (inverted brightness), no Qt semantic match
//    R1/R2/R4        — sub-byte red, no Qt equivalent (R1 could be Format_Mono
//                      but the MSB-first vs LSB-first convention is ambiguous
//                      without knowing the source driver)
//    R10/R12         — packed 10/12-bit red in a 16-bit word, no Qt match
//    RG88/GR88       — two-channel 8-bit, no Qt match
//    RG1616/GR1616   — two-channel 16-bit, no Qt match
//    RGB332/BGR233   — 8-bit packed RGB (3:3:2 / 2:3:3), no Qt match
//    XBGR/RGBX/BGRX 4444 — Qt only has ARGB4444_Premultiplied; the X variants
//                      have no straight counterpart
//    ABGR/RGBA/BGRA 4444 — same; Qt's Format_RGB444 has no alpha slot
//    XBGR/RGBX/BGRX 1555 — Qt has RGB555 (=XRGB1555) but not the swapped forms
//    ABGR/RGBA/BGRA 1555 — Qt has no ABGR/RGBA/BGRA 1555 format
//    XRGB/XBGR/RGBX/BGRX 2101010 — Qt has BGR30/RGB30 for X forms only; the
//                      RGBX/BGRX (pad-bit at MSB) variants differ from
//                      Qt's A2BGR30/A2RGB30 (alpha at MSB) — skip those two
//    RGBA/BGRA 1010102, RGBX/BGRX 1010102 — no Qt match
//    RGB161616/BGR161616 — 48-bit RGB (no alpha), no Qt 3×16 format
//    XRGB16161616/XBGR16161616 — 64-bit with X channel:
//                      Qt RGBX64 is halfword-ordered [R,G,B,X];
//                      XBGR maps directly, XRGB needs R↔B swap ✓
//    BGR161616F      — 3×16FP, no Qt 3-channel half-float format
//    GR1616F         — two-channel, no Qt match
//    R16F/R32F       — single-channel float; Qt has no Format_GrayscaleF
//    GR3232F         — two-channel, no Qt match
//    BGR323232F      — 3×32FP, no Qt 3-channel float format
//    AXBXGXRX106106106106 — 10-bit with 6-bit padding per channel, no Qt match
// ─────────────────────────────────────────────────────────────────────────────
struct Entry
{
    uint32_t drmFourcc = 0;
    QImage::Format qtFormat = QImage::Format_Invalid;
    DrmFormat::ConvertFn convert = nullptr;
};

// clang-format off
static constexpr std::array kTable = std::to_array<Entry>({

    // ── Indexed / palette ────────────────────────────────────────────────────
    // C8: caller must populate the QImage colour table for meaningful display.
    { DRM_FORMAT_C8,            QImage::Format_Indexed8,                   nullptr },

    // ── Single-channel (red / grayscale) ────────────────────────────────────
    { DRM_FORMAT_R8,            QImage::Format_Grayscale8,                 nullptr },
    { DRM_FORMAT_R16,           QImage::Format_Grayscale16,                nullptr },

    // ── 16-bit RGB ───────────────────────────────────────────────────────────
    { DRM_FORMAT_RGB565,        QImage::Format_RGB16,                      nullptr },
    { DRM_FORMAT_BGR565,        QImage::Format_RGB16,                      &swizzle_bgr565 },

    // XRGB4444: Qt's Format_RGB444 stores [x:R:G:B 4:4:4:4] natively.
    { DRM_FORMAT_XRGB4444,      QImage::Format_RGB444,                     nullptr },

    // ARGB4444: Qt only has the premultiplied variant.  Straight-alpha DRM
    // frames will appear with incorrect alpha unless pre-multiplied upstream.
    { DRM_FORMAT_ARGB4444,      QImage::Format_ARGB4444_Premultiplied,     nullptr },

    // XRGB1555 / ARGB1555
    { DRM_FORMAT_XRGB1555,      QImage::Format_RGB555,                     nullptr },
    // Qt's Format_ARGB8555_Premultiplied is 24-bit (8-bit alpha + 15-bit RGB),
    // not a direct match for DRM's 16-bit ARGB1555.  Use the premultiplied
    // ARGB32 path: expand via swizzle32 after widening — but that would change
    // the format.  Instead map to RGB555 (dropping alpha) or skip.
    // We expose it as RGB555 so the geometry/stride still works; alpha is lost.
    { DRM_FORMAT_ARGB1555,      QImage::Format_RGB555,                     nullptr },

    // ── 24-bit RGB ───────────────────────────────────────────────────────────
    { DRM_FORMAT_RGB888,        QImage::Format_RGB888,                     nullptr },
    // Qt 5.14+ has Format_BGR888 natively — no conversion needed.
    { DRM_FORMAT_BGR888,        QImage::Format_BGR888,                     nullptr },

    // ── 32-bit ARGB / XRGB — native Qt ARGB32/RGB32 layout ─────────────────
    // Qt ARGB32 in memory (LE): [B, G, R, A] — matches DRM ARGB8888 directly.
    { DRM_FORMAT_ARGB8888,      QImage::Format_ARGB32,                     nullptr },
    { DRM_FORMAT_XRGB8888,      QImage::Format_RGB32,                      nullptr },

    // ── 32-bit: swap R↔B (ABGR/XBGR → ARGB/XRGB) ──────────────────────────
    { DRM_FORMAT_ABGR8888,      QImage::Format_ARGB32,                     &swizzle32<kSwapRB> },
    { DRM_FORMAT_XBGR8888,      QImage::Format_RGB32,                      &swizzle32<kSwapRB> },

    // ── 32-bit: Qt Format_RGBX8888 / Format_RGBA8888 ────────────────────────
    // Qt stores these as byte-ordered [R, G, B, X/A] — matching DRM directly.
    { DRM_FORMAT_RGBX8888,      QImage::Format_RGBX8888,                   nullptr },
    { DRM_FORMAT_RGBA8888,      QImage::Format_RGBA8888,                   nullptr },

    // ── 32-bit: rotate A right (BGRA/BGRX → Qt RGBX/RGBA) ──────────────────
    // DRM BGRA8888 bytes: [A, R, G, B]; Qt RGBA8888 bytes: [R, G, B, A].
    // Mask kReverse {3,2,1,0} maps [A,R,G,B] → [B,G,R,A] (= ARGB32), but we
    // want [R,G,B,A] for Format_RGBA8888.  Use a dedicated mask instead.
    //   BGRA: src bytes [A, R, G, B] → want [R, G, B, A] = {1, 2, 3, 0}
    //   BGRX: src bytes [X, R, G, B] → want [R, G, B, X] = {1, 2, 3, 0}
    { DRM_FORMAT_BGRA8888,      QImage::Format_RGBA8888,                   &swizzle32<Mask4{1,2,3,0}> },
    { DRM_FORMAT_BGRX8888,      QImage::Format_RGBX8888,                   &swizzle32<Mask4{1,2,3,0}> },

    // ── 32-bit 10-bit per channel ────────────────────────────────────────────
    // Qt Format_BGR30  = packed [x:B:G:R 2:10:10:10] = DRM XRGB2101010 ✓
    // Qt Format_RGB30  = packed [x:R:G:B 2:10:10:10] = DRM XBGR2101010 ✓
    // Qt Format_A2BGR30_Premultiplied = DRM ARGB2101010 ✓  (premul caveat)
    // Qt Format_A2RGB30_Premultiplied = DRM ABGR2101010 ✓  (premul caveat)
    { DRM_FORMAT_XRGB2101010,   QImage::Format_BGR30,                      nullptr },
    { DRM_FORMAT_XBGR2101010,   QImage::Format_RGB30,                      nullptr },
    { DRM_FORMAT_ARGB2101010,   QImage::Format_A2BGR30_Premultiplied,      nullptr },
    { DRM_FORMAT_ABGR2101010,   QImage::Format_A2RGB30_Premultiplied,      nullptr },

    // ── 64-bit integer (16 bits per channel) ─────────────────────────────────
    // Qt Format_RGBX64 / Format_RGBA64: halfwords in memory [R, G, B, X/A].
    // DRM XBGR16161616 LE word X[63:48]B[47:32]G[31:16]R[15:0]
    //   → 16-bit units in memory: [R, G, B, X] → RGBX64 ✓
    // DRM ABGR16161616 → [R, G, B, A] → RGBA64 ✓
    { DRM_FORMAT_XBGR16161616,  QImage::Format_RGBX64,                     nullptr },
    { DRM_FORMAT_ABGR16161616,  QImage::Format_RGBA64,                     nullptr },
    // DRM XRGB16161616 → [B, G, R, X] → swap unit0↔unit2 → RGBX64
    // DRM ARGB16161616 → [B, G, R, A] → swap unit0↔unit2 → RGBA64
    { DRM_FORMAT_XRGB16161616,  QImage::Format_RGBX64,                     &swizzle_swap_rb_wide<2> },
    { DRM_FORMAT_ARGB16161616,  QImage::Format_RGBA64,                     &swizzle_swap_rb_wide<2> },

    // ── 64-bit half-float (16FP per channel) ─────────────────────────────────
    // Same channel-in-memory analysis as integer 16161616:
    // XBGR16161616F → [R,G,B,X] halfwords → Format_RGBX16FPx4 ✓
    // ABGR16161616F → [R,G,B,A] halfwords → Format_RGBA16FPx4 ✓
    { DRM_FORMAT_XBGR16161616F, QImage::Format_RGBX16FPx4,                 nullptr },
    { DRM_FORMAT_ABGR16161616F, QImage::Format_RGBA16FPx4,                 nullptr },
    // XRGB16161616F → [B,G,R,X] → swap → RGBX16FPx4
    // ARGB16161616F → [B,G,R,A] → swap → RGBA16FPx4
    { DRM_FORMAT_XRGB16161616F, QImage::Format_RGBX16FPx4,                 &swizzle_swap_rb_wide<2> },
    { DRM_FORMAT_ARGB16161616F, QImage::Format_RGBA16FPx4,                 &swizzle_swap_rb_wide<2> },

    // ── 128-bit float (32FP per channel) ─────────────────────────────────────
    // DRM ABGR32323232F: 32FP units in memory [R, G, B, A]
    // Qt Format_RGBA32FPx4: 32FP units in memory [R, G, B, A] ✓
    { DRM_FORMAT_ABGR32323232F, QImage::Format_RGBA32FPx4,                 nullptr },
});
// clang-format on

} // namespace

namespace DrmFormat {

std::optional<FormatDescriptor> resolve(const uint32_t drmFourcc)
{
    for (const auto &e : kTable) {
        if (e.drmFourcc == drmFourcc)
            return FormatDescriptor{e.qtFormat, e.convert};
    }
    return std::nullopt;
}

} // namespace DrmFormat
