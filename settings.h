#ifndef SETTINGS_H
#define SETTINGS_H

#include <qtypes.h>

namespace Settings {

namespace utils {

/// Counts the digits in a number (by gradually dividing it by 10).
constexpr int numberCount(const int v)
{
    const auto reduced = v / 10;
    return 1 + (reduced > 0 ? numberCount(reduced) : 0);
}

} // namespace utils

/// Maximum number of virtual displays supported.
static constexpr int maximumDisplayCount = 999;
/// Number of digits required to represent maximumDisplayCount.
static constexpr int maximumDisplayCountLength = utils::numberCount(maximumDisplayCount);
/// Maximum attempts to find an available local port.
static constexpr int maximumLPTries = 3;

// Safe UDP payload size that avoids IP fragmentation on standard 1500 MTU Ethernet / Wi-Fi
static constexpr int dtlsChunkSize = 1400;
// It would be impossible to reach a clean 60 fps anyway.
static constexpr int frameMSecsInterval = 100;
/// Inactivity timeout in milliseconds before a connection is considered timed out.
static constexpr int inactivityTimeout = 2000;
/// Maximum packet parsing error count before parser reset.
static constexpr auto errorLimit = 8192;
/// AES initialization vector size in bytes.
static constexpr int ivSize = 16;
/// Derived encryption key length in bytes.
static constexpr int dkLen = 32;
/// Iteration count for PBKDF2 key derivation.
static constexpr int keyIterations = 10000;
/// Salt length in bytes for key derivation.
static constexpr int saltLen = 32;

/// Format string for displaying timestamps.
static constexpr auto timeFormat = " HH:mm";

// Capture loop: fast poll for frame changes. Actual encodes are
// rate-limited separately. 5 ms is a good balance: reacts to changes
// quickly without burning meaningful CPU on ioctl polls.
constexpr int pollMSecsInterval = 5;

// Hard cap on outbound frame rate. Prevents saturating the client when
// the compositor draws faster than the client can decode.
constexpr int maxForwardFps = 35;
/// Minimum interval in nanoseconds between forwarded frames.
constexpr qint64 minForwardIntervalNs = 15'000'000LL;

// Emit a HeartBeat packet when no frame has been forwarded for this long.
// Keeps client connections alive during idle content.
constexpr int idleKeepAliveMSecs = 5000;

/// Dropped frame count threshold indicating client overload.
constexpr quint16 overloadedDroppedFrames = 3;
/// 95th-percentile latency threshold (in ms) indicating client overload.
constexpr quint16 overloadedLatencyMsP95 = 60;
/// Decoder queue depth threshold indicating client overload.
constexpr quint8 overloadedDecodeQueueDepth = 4;
/// Render queue depth threshold indicating client overload.
constexpr quint8 overloadedRenderQueueDepth = 4;

/// Target dropped frame count for a healthy client state.
constexpr quint16 healthyDroppedFrames = 0;
/// Maximum 95th-percentile latency (in ms) for a healthy client state.
constexpr quint16 healthyLatencyMsP95 = 45;
/// Maximum decoder queue depth for a healthy client state.
constexpr quint8 healthyDecodeQueueDepth = 3;
/// Maximum render queue depth for a healthy client state.
constexpr quint8 healthyRenderQueueDepth = 3;

/// Limit of consecutive timing anomalies before adjusting frame delivery.
constexpr int consecutiveTALLimit = 5;

} // namespace Settings

#endif // SETTINGS_H
