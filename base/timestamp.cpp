#include "timestamp.h"
#include <sys/time.h>

using namespace kaycc;

Timestamp Timestamp::now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;

    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);//返回自1970 01 01 开始的微秒数的一个新对象
}