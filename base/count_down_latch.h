#ifndef KAYCC_BASE_COUNTDOWNLATCH_H
#define KAYCC_BASE_COUNTDOWNLATCH_H

//倒计时类，满足count个条件后，触发
#include "condition.h"
#include "mutex.h"
#include <boost/noncopyable.hpp>

namespace kaycc {

	class CountDownLatch : boost::noncopyable {
	public:
		explicit CountDownLatch(int count);
		
		void wait();

		void countDown();

		int getCount() const;

	private:
		mutable MutexLock mutex_;
		Condition condition_;
		int count_;
	};

}

#endif
