#ifndef KAYCC_BASE_ATOMIC_H
#define KAYCC_BASE_ATOMIC_H

#include <boost/noncopyable.hpp>
#include <stdint.h>

namespace kaycc { 
namespace atomics {
	
	template <typename T>
	class AtomicInteger {
		public:
			AtomicInteger()
				: value_(0) {
			}


			T get() {
				return __sync_val_compare_and_swap(&value_, 0, 0);
			}

			T getAndAdd(T x) {
				return __sync_fetch_and_add(&value_, x);
			}

			T addAndGet(T x) {
				return getAndAdd(x) + x; //getAndAdd(x)返回增加之前的数
			}

			T incrementAndGet() {
				return addAndGet(1);
			}

			T decrementAndGet() {
				return addAndGet(-1);
			}

			void add(T x) {
				getAndAdd(x);
			}

			void increment() {
				incrementAndGet();
			}

			void decrement() {
				decrementAndGet();
			}

			T getAndSet(T x) {
				return __sync_lock_test_and_set(&value_, x); //__sync_lock_test_and_set将value_设置为x，返回之前的值
			}

		private:
			volatile T value_; //volatile 这个关键字来避免编译时内存乱序访问 如：
							   // x = y; y = 1;编译器优化的结果可能导致 y = 1 在 x = r 之前执行完成
	};
}
	typedef atomics::AtomicInteger<int32_t> AtomicInt32;
	typedef atomics::AtomicInteger<int64_t> AtomicInt64;
}


#endif
