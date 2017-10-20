#ifndef KAYCC_BASE_CONDITION_H 
#define KAYCC_BASE_CONDITION_H 

#include "mutex.h"
#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace kaycc {
	class Condition : boost::noncopyable {
		public:
			explicit Condition(MutexLock &mutex)
				: mutex_(mutex) {
				KCHECK(pthread_cond_init(&pcond_, NULL));
			}

			~Condition() {
				KCHECK(pthread_cond_destroy(&pcond_));
			}

			void wait() {
				MutexLock::UnassignGuard ug(mutex_); //线程刮起前，先清空mutex_的holder线程id， pthread_cond_wait返回后，在赋值holder线程id
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
