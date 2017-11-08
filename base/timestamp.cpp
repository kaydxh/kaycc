#include "timestamp.h"

#include <sys/time.h>
#include <stdio.h> //snprintf

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h> //PRId64

using namespace kaycc;

//int64_t用来表示64位整数，在32位系统中是long long int，在64位系统中是long int，所以打印int64_t的格式化方法是：
//printf("%ld", value); // 64bit OS  
//printf("%lld", value); // 32bit OS 
//跨平台的方法:

/*#include <inttypes.h>  
printf("%" PRId64 "\n", value);  
// 相当于64位的：  
printf("%" "ld" "\n", value);  
// 或32位的：  
printf("%" "lld" "\n", value);
*/
//C++ 中使用PRId64需要定义宏__STDC_FORMAT_MACROS

std::string Timestamp::toString() const {
    char buf[32] = {0};

    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf) - 1, "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
}

Timestamp Timestamp::now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;

    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);//返回自1970 01 01 开始的微秒数的一个新对象
}