#include "condition.h"
#include <errno.h>

bool kaycc::Condition::waitForSeconds(double seconds) { //seconds是相对时间，比如10s
	struct timespec abstime;
	clock_gettime(CLOCK_REALTIME, &abstime);

	const int64_t kNanoSecondsPerSecond = 1e9;
	int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);

	abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
	abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);

	MutexLock::UnassignGuard ug(mutex_);
	return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPthreadMutex(), &abstime); //pthread_cond_timedwait的时间是绝对时间，
        //如，10s超时，先要获取当前时间，再加上超时时间，转化为timespec结构。
}
