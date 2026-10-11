#ifndef SETTINGS_H
#define SETTINGS_H

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

}

#endif // SETTINGS_H
