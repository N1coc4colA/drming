#ifndef SETTINGS_H
#define SETTINGS_H

namespace Settings {

namespace utils {

constexpr int numberCount(const int v)
{
    const auto reduced = int(v / 10);
    return 1 + (reduced > 0 ? numberCount(reduced) : 0);
}

} // namespace utils

static constexpr int maximumDisplayCount = 999;
static constexpr int maximumDisplayCountLength = utils::numberCount(maximumDisplayCount);
static constexpr int maximumLPPTries = 3;

static constexpr int dtlsChunkSize = 1000;
static constexpr int frameMSecsInterval = 60;
static constexpr int inactivityTimeout = 5000;

static constexpr auto advertisementServiceType = "_drming._udp";
static constexpr auto frameImageFormat = "WEBP";

static constexpr auto timeFormat = " HH:mm";

}

#endif // SETTINGS_H
