#ifndef _RGNANO_SIM_MEMORY_H_
#define _RGNANO_SIM_MEMORY_H_

// The simulator's heap, held to what the app can get on the RG Nano.
//
// Every malloc/calloc/realloc/free in the exe (the app, and the statically
// linked C++ runtime behind new/delete) goes through wrappers (the linker's
// --wrap) that count the bytes in use. With a limit set, a request that
// would take the heap past it fails the way it fails on the device: malloc
// returns NULL and new throws std::bad_alloc (which ends the app, as on the
// device). The UI thread turns failures into an RGNANO_SIM_OOM error line,
// so expect_no_error catches them. See docs/RGNANO_SIM.md for the numbers.

// RAM the app process can hold on the RG Nano, and the part of its RSS that
// is not heap. Derivation in docs/RGNANO_SIM.md; override the first with
// -RGNANOSIM_DEVICE_PROCESS_KB=<kb> (0 = count only, no limit).
#define RGNANOSIM_DEVICE_PROCESS_KB 45056
#define RGNANOSIM_DEVICE_NONHEAP_KB 4096

class RGNanoSimMemory {
public:
	// The device's RAM for the whole process and the part of it that is not
	// heap (code, libraries, stacks, SDL's own buffers): the heap may use
	// the difference. 0 = no limit (counting only).
	static void SetDeviceBudget(unsigned int processKB, unsigned int nonHeapKB);
	static unsigned int LimitKB();
	static unsigned int LiveKB();
	static unsigned int PeakKB();
	static void ResetPeak();
	// Requests refused since the last call, and the largest of them
	static unsigned int TakeFailures(unsigned int &largestKB, unsigned int &liveKB);
};

#endif
