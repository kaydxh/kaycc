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
				MutexLock::UnassignGuard ug(mutex_); //线程刮起前，先清空mutex_的holder线程id(因为pthread_cond_wait会先对mutex解锁)， pthread_cond_wait返回后，在赋值holder线程id
				//pthread_cond_wait完成三件事：
				// (1)对mutex解锁;
  				// (2)等待条件, 直到有线程向他发送通知;
  				// (3)当wait返回时, 再对mutex重新加锁;
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
