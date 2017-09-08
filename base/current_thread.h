#ifndef KAYCC_BASE_CURRENTTHREAD_H
#define KAYCC_BASE_CURRENTTHREAD_H

namespace kaycc {
namespace currentthread {
	// __thread 表明每个线程有一份独立实体
	extern __thread int t_cachedTid;
	extern __thread char t_tidString[32];
	extern __thread int t_tidStringLength;
	extern __thread const char * t_threadName;
	
	void cacheTid();

	inline int tid() {
		if (__builtin_expect(t_cachedTid == 0, 0)) {
			cacheTid();
		}

		return t_cachedTid;
	}

	inline const char * tidString() const {
		return t_tidString;
	}

	inline int tidStringLength() const {
		return t_tidStringLength;
	}

	inline const * name () const {
		return t_threadName;
	}

	bool isMainThread();
	
	void sleepUsec(int64_t usec);
}
}
#endif
