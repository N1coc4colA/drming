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

static constexpr int dtlsChunkSize = 1024;
static constexpr int frameMSecsInterval = 60;
static constexpr int inactivityTimeout = 5000;

static constexpr auto timeFormat = " HH:mm";

}

#endif // SETTINGS_H
