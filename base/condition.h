#ifndef KAYCC_BASE_CONDITION_H 
#define KAYCC_BASE_CONDITION_H 

#include "mutex.h"
#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace kaycc {
	class Condition : boos::noncopyable {
		public:
			explicit Condition(MutexLock &mutex)
				: mutex_(mutex) {
				KCHECK(pthread_cond_init(pcond_, NULL);
			}

			~Condition() {
				KCHECK(pthread_cond_destory(&pcond_);
			}

			void wait() {
				MutexLock::UnassignGuard ug(mutex_);
				KCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
			}

			bool waitForSeconds(double seconds);

			void notify() {
				KCHECK(pthread_cond_signal(&pcond_));
			}

			void notifyAll() {
				KCHECK(pthread_cond_broadcast(&pcond_));
			}

		private:
			MutexLock &mutex_;
			pthread_cond_t pcond_;
	};
}
#endif 
