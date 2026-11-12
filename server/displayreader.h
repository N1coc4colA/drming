#ifndef DISPLAYREADER_H
#define DISPLAYREADER_H

#include <QHash>
#include <QImage>
#include <QString>

#include <vector>

#include "sse.h"
#include "vkmsfb.h"

/**
 * Reads primary and cursor buffers exposed by the VKMS DRM connector.
 *
 * DisplayReader is the sole owner of the DRM fd and of every mapped
 * framebuffer. The VkmsFrameBuffer / CursorFrameBuffer views it hands out are
 * *borrows*: they must not be closed, unmapped, or freed by the caller.
 *
 * @param writableMapping
 *   When true, primary framebuffers are mapped PROT_READ|PROT_WRITE and
 *   compositeWithCursorInPlace() may be used to burn the cursor into the
 *   mapped buffer. This MODIFIES the VKMS framebuffer itself, so if the
 *   connector is being displayed locally you will see the cursor twice
 *   (hardware cursor plane + burned-in copy). Enable only for headless
 *   connectors. Defaults to false (read-only mapping, copy-based composite).
 */
class DisplayReader
{
public:
    explicit DisplayReader(QString connectorName, bool writableMapping = false);
    ~DisplayReader();
    DisplayReader(const DisplayReader &) = delete;
    DisplayReader &operator=(const DisplayReader &) = delete;

    /// Returns true if memory mapping is writable and in-place compositing is supported.
    bool supportsInPlaceComposite() const
    {
        return m_writableMapping;
    }

    /// Retrieves and maps the current primary plane framebuffer.
    bool getVkmsFrameBuffer(VkmsFrameBuffer &fb);

    /**
     * No-op on the borrowed view; kept for API symmetry with the previous
     * ownership model. Cache lifetime is tied to DisplayReader.
     */
    static void releaseVkmsFrameBuffer(VkmsFrameBuffer &fb);

    /// Retrieves and maps the current cursor plane framebuffer.
    bool getCursorFrameBuffer(CursorFrameBuffer &cursor, const VkmsFrameBuffer &primary);

    /// No-op on the borrowed view; the underlying mapping lives in the cache.
    static void releaseCursorFrameBuffer(CursorFrameBuffer &cursor);

    /**
     * Copy-based composite: copies `primary` into `output` (if they differ)
     * and then draws `cursor` into `output` with QPainter. `output` is
     * reused across frames to avoid QImage detach churn.
     */
    static void compositeWithCursor(
        const QImage &primary,
        const CursorFrameBuffer &cursor,
        const DrmFormat::FormatDescriptor &fmtDesc,
        QImage &output
    );

    /**
     * In-place composite: writes the cursor directly into `primary`.
     * `primary` must be a writable view over a PROT_READ|PROT_WRITE mapping
     * (i.e. obtained from imageFromFrameBuffer(..., writable=true) with a
     * non-swizzle format). No fallback; caller must pick the right path.
     */
    static void compositeWithCursorInPlace(
        QImage &primary,
        const CursorFrameBuffer &cursor,
        const DrmFormat::FormatDescriptor &fmtDesc
    );

    /**
     * @brief Wraps a DRM buffer in a QImage and applies any required channel swizzle.
     * When `writable` is true the returned QImage wraps the buffer non-const,
     * allowing QPainter writes to land in-place. `writable` is silently
     * demoted to false for formats whose `fmtDesc.convert` is non-null, since
     * the swizzle is a one-way transform and running it in-place on a cached
     * mapping would corrupt the next frame.
     */
    static QImage imageFromFrameBuffer(
        const uint8_t *data,
        uint32_t width,
        uint32_t height,
        uint32_t stride,
        const DrmFormat::FormatDescriptor &fmtDesc,
        bool writable = false
    );

private:
    /// Cached memory mapping and metadata for a primary DRM plane.
    struct CachedPrimary
    {
        /// Memory-mapped pixel data pointer.
        void *data = nullptr;
        /// Memory-mapped buffer size in bytes.
        size_t size = 0;
        /// Plane width in pixels.
        uint32_t width = 0;
        /// Plane height in pixels.
        uint32_t height = 0;
        /// Pixel row stride in bytes.
        uint32_t stride = 0;
        /// DRM FourCC pixel format.
        uint32_t format = 0;
        /// Bits per pixel.
        uint32_t bpp = 0;
        /// Whether the buffer mapping is writable.
        bool writable = false;
    };

    /// Cached memory mapping and metadata for a cursor DRM plane.
    struct CachedCursor
    {
        /// Memory-mapped pixel data pointer.
        void *data = nullptr;
        /// Memory-mapped buffer size in bytes.
        size_t size = 0;
        /// Cursor width in pixels.
        uint32_t width = 0;
        /// Cursor height in pixels.
        uint32_t height = 0;
        /// Cursor row stride in bytes.
        uint32_t stride = 0;
        /// DRM FourCC pixel format.
        uint32_t format = 0;
    };

    /// DRM property IDs identifying a cursor plane on the CRTC.
    struct CursorPlaneInfo
    {
        /// DRM plane ID.
        uint32_t plane_id = 0;
        /// DRM property ID for plane type.
        uint32_t typePropId = 0;
        /// DRM property ID for CRTC X coordinate.
        uint32_t crtcXPropId = 0;
        /// DRM property ID for CRTC Y coordinate.
        uint32_t crtcYPropId = 0;
    };

    /// Maximum number of primary buffer mappings retained in cache.
    static constexpr int kMaxCachedPrimaries = 8;
    /// Maximum number of cursor buffer mappings retained in cache.
    static constexpr int kMaxCachedCursors = 4;

    /// Initializes the DRM file descriptor, connector, CRTC, and cursor planes.
    /// Returns false on failure.
    bool ensureInitialised();
    /// Discovers cursor planes compatible with the active CRTC.
    void discoverCursorPlanes();
    void teardown();

    /// Target display connector name.
    const QString m_connectorName{};
    /// DRM device file path for the connector.
    QString m_connectorPath{};

    /// Open file descriptor for the DRM device.
    int m_drmFd = -1;
    /// Active CRTC identifier.
    uint32_t m_crtcId = 0;
    /// Whether framebuffer mappings are requested with write permissions.
    bool m_writableMapping = false;

    /// CRTC resource handle reused across capture frames.
    Drm::DrmCrtcPtr m_crtc{};

    /// Cache of mapped primary framebuffers indexed by fb_id.
    QHash<uint32_t, CachedPrimary> m_primaryBuffers;
    /// Cache of mapped cursor framebuffers indexed by fb_id.
    QHash<uint32_t, CachedCursor> m_cursorBuffers;

    /// List of cursor planes associated with the active CRTC.
    std::vector<CursorPlaneInfo> m_cursorPlanes;
};

#endif // DISPLAYREADER_H
