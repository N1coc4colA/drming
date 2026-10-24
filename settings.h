#ifndef SETTINGS_H
#define SETTINGS_H

#include <qtypes.h>

namespace Settings {

namespace utils {

constexpr int numberCount(const int v)
{
    const auto reduced = v / 10;
    return 1 + (reduced > 0 ? numberCount(reduced) : 0);
}

} // namespace utils

static constexpr int maximumDisplayCount = 999;
static constexpr int maximumDisplayCountLength = utils::numberCount(maximumDisplayCount);
static constexpr int maximumLPTries = 3;

// Safe UDP payload size that avoids IP fragmentation on standard 1500 MTU Ethernet / Wi-Fi
static constexpr int dtlsChunkSize = 1400;
// It would be impossible to reach a clean 60 fps anyway.
static constexpr int frameMSecsInterval = 100;
static constexpr int inactivityTimeout = 2000;
static constexpr auto errorLimit = 8192;
static constexpr int ivSize = 16;
static constexpr int dkLen = 32;
static constexpr int keyIterations = 10000;
static constexpr int saltLen = 32;

static constexpr auto timeFormat = " HH:mm";

// Capture loop: fast poll for frame changes. Actual encodes are
// rate-limited separately. 5 ms is a good balance: reacts to changes
// quickly without burning meaningful CPU on ioctl polls.
constexpr int pollMSecsInterval = 5;

// Hard cap on outbound frame rate. Prevents saturating the client when
// the compositor draws faster than the client can decode.
constexpr int maxForwardFps = 65;
constexpr qint64 minForwardIntervalNs = 15'000'000LL;

// Emit a HeartBeat packet when no frame has been forwarded for this long.
// Keeps client connections alive during idle content.
constexpr int idleKeepAliveMSecs = 5000;

} // namespace Settings

#endif // SETTINGS_H
