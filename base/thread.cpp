#include "thread.h"
#include "current_thread.h"

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/weak_ptr.hpp>

#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

namespace kaycc {
namespace currentthread {
	__thread int t_cachedTid;
	__thread char t_tidString[32];
	__thread int t_tidStringLength;
	__thread const char * t_threadName = "unknown";

	const bool sameType = boost::is_same<int, pid_t>::value;
	BOOST_STATIC_ASSERT(sameType);
} //end currentthread

namespace detail {
	pid_t gettid() {
		return static_cast<pid_t>(::syscall(SYS_gettid));
	}

	void afterFork() {
		kaycc::currentthread::t_cachedTid = 0;
		kaycc::currentthread::t_threadName = "main";
		currentthread::tid();
	}

	class ThreadNameInitializer {
		public:
			ThreadNameInitializer() {
				kaycc::currentthread::t_threadName = "main";
				currentthread::tid();

				/* 说明：int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
					pthread_atfork()在fork()之前调用，当调用fork时，
					内部创建子进程前在父进程中会调用prepare，内部创建子进程成功后，父进程会调用parent ，子进程会调用child */
				pthread_atfork(NULL, NULL, &afterFork);
			}
	};

	ThreadNameInitializer init;

	struct ThreadData {
		typedef kaycc::Thread::ThreadFunc ThreadFunc;
		ThreadFunc func_;
		std::string name_;
		pid_t * tid_;
		CountDownLatch *latch_;

		ThreadData(const ThreadFunc &func, const std::string &name, 
				pid_t *tid, CountDownLatch *latch)
			: func_(func),
			  name_(name),
			  tid_(tid),
			  latch_(latch) {

			  }

		void runInThread() {
			*tid_ = kaycc::currentthread::tid();
			tid_ = NULL;

			latch_->countDown();
			latch_ = NULL;

			kaycc::currentthread::t_threadName = name_.empty() ? "kayccThread" : name_.c_str();
			::prctl(PR_SET_NAME, kaycc::currentthread::t_threadName); //把参数arg2作为调用进程的名字A, arg2最多16个字节

			try {
				func_();
				kaycc::currentthread::t_threadName = "finished";
			} catch (const std::exception &ex) {
				kaycc::currentthread::t_threadName = "crashed";
				fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
				fprintf(stderr, "reason: %s\n", ex.what());
				abort();
			} catch (...) {
				kaycc::currentthread::t_threadName = "crashed";
				fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
				throw;
			}
		}

	};

	void * startThread(void *obj) {
		ThreadData *data = static_cast<ThreadData *>(obj);
		data->runInThread();
		delete data;
		return NULL;
	}

} //end namespace detail 
} //end namespace kaycc


using namespace kaycc;

void currentthread::cacheTid() {
	if (t_cachedTid == 0) {
		t_cachedTid = detail::gettid(); //使用了detail的gettid()，所以currentthread的函数放在这里定义
		t_tidStringLength = snprintf(t_tidString, sizeof(t_tidString), "%5d", t_cachedTid);
	}
}

bool currentthread::isMainThread() {
	return tid() == ::getpid(); //getpid()得到的是进程的pid，在内核中，每个线程都有自己的PID，要得到线程的PID,必须用syscall(SYS_gettid);
}

void currentthread::sleepUsec(int64_t usec) {
	struct timespec ts = {0, 0};
	ts.tv_sec = static_cast<time_t>(usec / kMicroSecondsPerSecond);
	ts.tv_nsec = static_cast<long>(usec % kMicroSecondsPerSecond * 1000);
	::nanosleep(&ts, NULL);
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(const ThreadFunc &func, const std::string &n) 
	: started_(false),
	  joined_(false),
	  pthreadId_(0),
	  tid_(0),
	  func_(func),
	  name_(n),
	  latch_(1) {
		  setDefaultName();
	  }

#if __cplusplus >= 201103L
Thread::Thread(ThreadFunc&& func, const std::string& n)
	: started_(false),
	  joined_(false),
	  pthreadId_(0),
	  tid(0),
	  func(std::move(func)),
	  name_(n),
	  latch_(1) {
		  setDefaultName();
	  }
#endif
	
Thread::~Thread() {
	if (started_ && !joined_) {//如果线程已经启动，但是没有join，那么通过调用pthread_detach分离线程，这样该线程运行结束后会自动释放所有资源
		pthread_detach(pthreadId_);
	}
}

void Thread::setDefaultName() {
	int num = numCreated_.incrementAndGet();
	if (name_.empty()) {
		char buf[32];
		snprintf(buf, sizeof(buf), "Thread%d", num);
		name_ = buf;
	}
}

void Thread::start() {
	assert(!started_);
	started_ = true;

	detail::ThreadData *data = new detail::ThreadData(func_, name_, &tid_, &latch_);
	if (pthread_create(&pthreadId_, NULL, &detail::startThread, data)) {
		started_ = false;
		delete data;
		fprintf(stderr,  "Failed in pthread_create\n");
	} else {
		latch_.wait();
		assert(tid_ > 0);
	}
}

int Thread::join() {
	assert(started_);
	assert(!joined_);

	joined_ = true;
	return pthread_join(pthreadId_, NULL);
}
