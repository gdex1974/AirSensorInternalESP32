#pragma once

#include <cstdint>
#include <sys/time.h>

constexpr int64_t microsecondsInSecond = 1000000ll;
constexpr int64_t microsecondsInMinute = 60 * microsecondsInSecond;

inline constexpr int64_t microsecondsFromTimeval(const timeval &tv)
{
    return ((int64_t) tv.tv_sec) * microsecondsInSecond + tv.tv_usec;
}

inline int64_t microsecondsNow()
{
    timeval tv {};
    gettimeofday(&tv, nullptr);
    return microsecondsFromTimeval(tv);
}
