#include "utils.h"

#include <cerrno>
#include <ctime>

int msleep(const long msec)
{
    timespec ts{.tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000};
    int res = 0;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}
