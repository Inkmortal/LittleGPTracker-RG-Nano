#ifndef _AUDIO_PROFILER_H_
#define _AUDIO_PROFILER_H_

// Where the audio thread spends its time, per component of the mix.
//
// The render path marks which component is running (the sequencer, each
// track's instrument, the channel buses, the send effects, the master EQ,
// the limiter, the master mix); time is charged to the component running
// when it was spent, so a bus's own work is not counted twice with the
// instruments inside it. One buffer's figures are kept, plus totals.
//
// Cheap enough to stay on: a few clock reads per buffer. The device logs a
// summary in its heartbeat line; the DSP harness (tools/dsp-harness) swaps
// the clock for its own counter (instructions under qemu) through SetMarkHook.
//
// Only the audio thread calls Enter/Leave/BeginBuffer/EndBuffer.

enum AudioProfileSlot {
	APS_OTHER=0,      // outside the marked parts (driver, buffer copy)
	APS_SEQUENCER,    // the player: song position, commands, tables
	APS_CHANNEL0,     // track 1's instrument(s), sends and EQ included
	APS_CHANNEL7=APS_CHANNEL0+7,
	APS_BUSES,        // the channel buses: summing, volume, clip, meters
	APS_SENDFX,       // reverb, delay, chorus
	APS_MODCLOCK,
	APS_MASTER_EQ,
	APS_LIMITER,
	APS_MASTER,       // master sum, soft clip, volume, meters
	APS_COUNT
} ;

class AudioProfiler {
public:
	// Short name of a slot for logs ("ch1", "fx", ...)
	static const char *SlotName(int slot) ;

	static void Enable(bool on) ;
	static bool Enabled() { return enabled_ ; }

	// A buffer starts / ends (frames rendered)
	static void BeginBuffer() ;
	static void EndBuffer(int frames) ;

	// The render path enters a component and leaves it again (nesting is
	// fine: time goes to the innermost one)
	static inline void Enter(int slot) { if (enabled_ && slot>=0) enter(slot) ; }
	static inline void Leave(int slot) { if (enabled_ && slot>=0) leave() ; }

	// Last buffer, in microseconds per slot, and its total
	static double LastMicros(int slot) ;
	static double LastTotalMicros() ;
	// Running totals since the last Reset (microseconds, buffers, frames)
	static double TotalMicros(int slot) ;
	static unsigned long TotalBuffers() ;
	static unsigned long TotalFrames() ;
	// The worst buffer since the last TakePeak, as a share of its play time
	// (percent), with the slot shares of that buffer (percent of play time)
	static int TakePeak(int shares[APS_COUNT]) ;
	static void Reset() ;

	// Replaces the clock: hook(slot) is called at every switch to 'slot',
	// with -2 when a buffer starts and -1 when it ends; the times then
	// stay zero.
	typedef void (*MarkHook)(int slot) ;
	static void SetMarkHook(MarkHook hook) ;

	// Microseconds, monotonic
	static double NowMicros() ;
	// CPU time of the calling thread, microseconds (wall time where the
	// system has no thread clock)
	static double ThreadMicros() ;

private:
	static void enter(int slot) ;
	static void leave() ;
	static void switchTo(int slot) ;

	static bool enabled_ ;
} ;

#endif
