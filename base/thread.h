#ifndef KAYCC_BASE_THREAD_H
#define KAYCC_BASE_THREAD_H

#include "atomic.h"
#include "count_down_latch.h"

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <pthread.h>

namespace kaycc {

	class Thread : boost::noncopyable {
		public:
			typedef boost::function<void ()> ThreadFunc;

			explicit Thread(const ThreadFunc&, const std::string& name = std::string());

			~Thread();

			void start();
			int join();

			bool started() const {
				return started_;
			}

			pid_t tid() const  {
				return tid_;
			}

			const std::string& name() const {
				return name_;
			}

			static int numCreated() {
				return numCreated_.get();
			}

		private:
			void setDefaultName();

			bool started_;
			bool joined_;
			pthread_t pthreadId_;
			pid_t tid_;
			ThreadFunc func_;
			std::string name_;
			CountDownLatch latch_;

			static AtomicInt32 numCreated_;
	};
}

#endif
