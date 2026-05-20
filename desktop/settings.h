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

}

#endif // SETTINGS_H
