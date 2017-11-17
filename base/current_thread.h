#ifndef KAYCC_BASE_CURRENTTHREAD_H
#define KAYCC_BASE_CURRENTTHREAD_H

#include <stdint.h>

namespace kaycc {
namespace currentthread {
	// __thread 表明每个线程有一份独立实体
	extern __thread int t_cachedTid;
	extern __thread char t_tidString[32];
	extern __thread int t_tidStringLength;
	extern __thread const char * t_threadName;

	const int64_t kMicroSecondsPerSecond = 1e6;

	void cacheTid();

	inline int tid() {
		if (__builtin_expect(t_cachedTid == 0, 0)) { //t_cachedTid不太可能为0
			cacheTid();
		}

		return t_cachedTid;
	}

	inline const char * tidString() {
		return t_tidString;
	}

	inline int tidStringLength() {
		return t_tidStringLength;
	}

	inline const char * name () {
		return t_threadName;
	}

	bool isMainThread();
	
	void sleepUsec(int64_t usec);
}
}
#endif
