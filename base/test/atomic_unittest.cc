#include "../atomic.h"
#include <assert.h>

int
main() {
	kaycc::AtomicInt32 a0;
	assert(a0.get() == 0);
	assert(a0.getAndAdd(1) == 0);
	assert(a0.get() == 1);

	assert(a0.addAndGet(2) == 3);
	assert(a0.get() == 3);

	assert(a0.incrementAndGet() == 4);
	assert(a0.get() == 4);

	a0.increment();
	assert(a0.get() == 5);

	assert(a0.addAndGet(-3) == 2);
	assert(a0.get() == 2);

	assert(a0.decrementAndGet() == 1);
	assert(a0.get() == 1);

	a0.decrement();
	assert(a0.get() == 0);
}
