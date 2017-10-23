#ifndef KAYCC_BASE_MUTEX_H
#define KAYCC_BASE_MUTEX_H

#include "current_thread.h"
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

#ifdef CHECK_PTHREAD_RETURN_VALUE

#ifdef NDEBUG //没有定义NDEBUG时，assert.h定声明了__assert_perror_fail，所以这里只在定义NDEBUG时，声明__assert_perror_fail，保证__assert_perror_fail一直都被声明
__BEGIN_DECLS //# define __BEGIN_DECLS extern "C" {    用C++编译器统一处理
extern void __assert_perror_fail(int errnum,
								const char *file,
								unsigned int line,
								const char *function)
	__THROW __attribure__ ((__noreturn__)); //在c++引用时，声明这个函数支持C++里的throw异常功能 #define __THROW throw()
__END_DECLS //# define __END_DECLS    }
#endif 

/* gcc对C语言的一个扩展保留字，用于声明变量类型,var可以是数据类型（int， char*..),也可以是变量表达式。
__typeof__(int *) x; //It   is   equivalent   to  'int  *x';

__typeof__(int) a;//It   is   equivalent   to  'int  a';

__typeof__(*x)  y;//It   is   equivalent   to  'int y';

__typeof__(&a) b;//It   is   equivalent   to  'int  b';

__typeof__(__typeof__(int *)[4])   z; //It   is   equivalent   to  'int  *z[4]';
*/


//if (__builtin_expect(errnum != 0, 0)) ,代表errnum != 0的可能性不大，一般不进入该if语句
#define KCHECK(ret) ({ __typeof__ (ret) errnum = (ret);	\
						if (__builtin_expect(errnum != 0, 0))  \
							__assert_perror_fail (errnum, __FILE__, __LINE__, __func__);}) 

#else //undef CHECK_PTHREAD_RETURN_VALUE

#define KCHECK(ret) ({ __typeof__ (ret) errnum = (ret);	\
						assert(errnum == 0); (void) errnum;})


#endif


namespace kaycc {
	
	class MutexLock : boost::noncopyable { //default public
		public:
			MutexLock()
				: holder_(0) {
					KCHECK(pthread_mutex_init(&mutex_, NULL));
				}
			~MutexLock() {
				assert(holder_ == 0);
				KCHECK(pthread_mutex_destroy(&mutex_));
			}

			bool isLockedByThisThread() const {
				return holder_ == currentthread::tid();
			}

			void assertLockByThisThread() const {
				assert(isLockedByThisThread());
			}

			void lock() {
				KCHECK(pthread_mutex_lock(&mutex_));
				assignHolder();
			}

			void unlock() {
				unassignHolder();
				KCHECK(pthread_mutex_unlock(&mutex_));
			}

			pthread_mutex_t* getPthreadMutex() {
				return &mutex_;
			}

		private:
			friend class Condition;

			class UnassignGuard : boost::noncopyable {
				public:
					UnassignGuard(MutexLock &owner)
						: owner_(owner) {
							owner_.unassignHolder(); //对mutex_的holder线程id清空, 
						}

					~UnassignGuard() {
						owner_.assignHolder(); //对mutex_的holder赋值线程id
					}

				private:
					MutexLock &owner_;
			};

		private:
			void assignHolder() {
				holder_ = currentthread::tid(); //对mutex_的holder赋值线程id
			}

			void unassignHolder() {
				holder_ = 0; //对mutex_的holder线程id清空
			}

		private:
			pthread_mutex_t mutex_;
			pid_t holder_;
	};

	class MutexLockGuard : boost::noncopyable {
		public:
			explicit MutexLockGuard(MutexLock &mutex) //这里不能为const引用,non-const指针/引用可以转换为const指针引用，反之不行
				: mutex_(mutex) {
					mutex_.lock();
				}

			~MutexLockGuard() {
				mutex_.unlock();
			}

		private:
			MutexLock &mutex_;
	};
}

#define MutexLockGuard(x) error "Missing guard object name" //定义这个宏时阻止这样错误使用，MutexLockGuard(mutex_);临时变量获取锁后立马又释放了锁

#endif
