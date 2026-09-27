#include "RGNanoSimMemory.h"

#include <malloc.h>
#include <stddef.h>
#include <string.h>

// Heap accounting for the simulator (see the header). The linker routes
// malloc/calloc/realloc/free here (-Wl,--wrap=... in Makefile.RGNANO_SIM);
// the C++ runtime is linked statically so new/delete land here too. Sizes
// come from msvcrt's _msize, so no header is added to blocks and a block
// allocated elsewhere (SDL.dll) can still be freed normally.

extern "C" {
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);
}

static volatile long live_ = 0;       // bytes
static volatile long peak_ = 0;
static volatile long limit_ = 0;      // 0: no limit
static volatile long failures_ = 0;
static volatile long largestFailure_ = 0;
static volatile long liveAtFailure_ = 0;

static void account(long delta) {
	long now = __sync_add_and_fetch(&live_, delta);
	long peak = peak_;
	while (now > peak) {
		long seen = __sync_val_compare_and_swap(&peak_, peak, now);
		if (seen == peak) break;
		peak = seen;
	}
}

// The device refuses what it can't back with RAM: malloc returns NULL
static bool admit(size_t extra) {
	long limit = limit_;
	if (!limit) return true;
	if ((long)extra >= 0 && live_ + (long)extra <= limit) return true;
	__sync_add_and_fetch(&failures_, 1);
	if ((long)extra > largestFailure_) largestFailure_ = (long)extra;
	liveAtFailure_ = live_;
	return false;
}

extern "C" void *__wrap_malloc(size_t size) {
	if (!admit(size)) return 0;
	void *ptr = __real_malloc(size);
	if (ptr) account((long)_msize(ptr));
	return ptr;
}

extern "C" void *__wrap_calloc(size_t count, size_t size) {
	if (size && count > ((size_t)-1) / size) return 0;
	if (!admit(count * size)) return 0;
	void *ptr = __real_calloc(count, size);
	if (ptr) account((long)_msize(ptr));
	return ptr;
}

extern "C" void __wrap_free(void *ptr) {
	if (!ptr) return;
	account(-(long)_msize(ptr));
	__real_free(ptr);
}

extern "C" void *__wrap_realloc(void *ptr, size_t size) {
	if (!ptr) return __wrap_malloc(size);
	if (!size) {
		__wrap_free(ptr);
		return 0;
	}
	long old = (long)_msize(ptr);
	if ((long)size > old && !admit(size - old)) return 0;
	void *moved = __real_realloc(ptr, size);
	if (moved) account((long)_msize(moved) - old);
	return moved;
}

void RGNanoSimMemory::SetDeviceBudget(unsigned int processKB, unsigned int nonHeapKB) {
	limit_ = processKB > nonHeapKB ? (long)(processKB - nonHeapKB) * 1024 : 0;
}

unsigned int RGNanoSimMemory::LimitKB() {
	return (unsigned int)(limit_ / 1024);
}

unsigned int RGNanoSimMemory::LiveKB() {
	long live = live_;
	return live > 0 ? (unsigned int)(live / 1024) : 0;
}

unsigned int RGNanoSimMemory::PeakKB() {
	return (unsigned int)(peak_ / 1024);
}

void RGNanoSimMemory::ResetPeak() {
	peak_ = live_;
}

unsigned int RGNanoSimMemory::TakeFailures(unsigned int &largestKB, unsigned int &liveKB) {
	long count = __sync_lock_test_and_set(&failures_, 0);
	largestKB = (unsigned int)((largestFailure_ + 1023) / 1024);
	liveKB = (unsigned int)(liveAtFailure_ / 1024);
	if (count) largestFailure_ = 0;
	return (unsigned int)count;
}
