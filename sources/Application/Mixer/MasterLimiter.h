#ifndef _MASTER_LIMITER_H_
#define _MASTER_LIMITER_H_

#include "Foundation/T_Singleton.h"
#include "Services/Audio/AudioInsert.h"

class Project;

// Look-ahead peak limiter on the whole mix, after the master EQ (like the
// M8's LIM). Drive pushes the mix into it, the output never goes above the
// ceiling. Drive 00 = off: the mix passes untouched, with no delay.
//
// Gain path per sample (stereo linked): the gain each sample needs to stay
// under the ceiling -> minimum over the look-ahead window (peak hold) ->
// release (falls instantly, recovers exponentially) -> two stacked moving
// averages as long as the window (a smooth attack). The audio is delayed by
// the window, so the gain is fully down when the peak comes out. After
// "Designing a straightforward limiter" (Geraint Luff, Signalsmith Audio).
#define LIMITER_MAX_WINDOW 1024   // samples; power of two
#define LIMITER_HISTORY 112       // meter history, one entry per audio buffer

class MasterLimiter : public AudioInsert, public T_Singleton<MasterLimiter> {
public:
	MasterLimiter();
	void SetProject(Project *project);
	// Without a project (the device checks): drive, ceiling, attack, release
	void SetParams(const int params[4]);
	virtual void Process(fixed *buffer, int frames);
	// The mix stopped: forget the look-ahead audio and let the meter fall
	virtual void Silence();

	bool Enabled();
	// Meters, for the LIMIT screen and the mixer's master strip
	float GetGainReductionDb() { return grDb_; }      // last buffer, >= 0
	float GetInputPeakDb() { return inDb_; }          // after drive, dBFS
	float GetOutputPeakDb() { return outDb_; }        // dBFS
	// i = 0 oldest .. LIMITER_HISTORY-1 newest
	float GetHistoryGrDb(int i);
	float GetHistoryInDb(int i);
	// Audio delay while on (the look-ahead), in samples
	int GetLatency() { return enabled_ ? window_ - 1 : 0; }

	// Knob curves, shared with the LIMIT screen
	static float DriveDbFromParam(int value);      // 00 off, 01 = 0 dB .. FF = +25.4 dB
	static float CeilingDbFromParam(int value);    // 00 = -25.5 .. FF = 0 dB
	static float AttackMsFromParam(int value);     // 0.1 .. 10 ms
	static float ReleaseMsFromParam(int value);    // 00 auto (-1), 01 = 4 .. FF = 1000 ms

private:
	void readParams(int params[4]);
	void configure(const int params[4]);
	void reset();
	void setWindow(int window);

	Project *project_;
	int fixed_[4];
	int params_[4];
	bool enabled_;
	bool idle_;           // state is clear (bypassed)
	float rate_;
	int window_;          // look-ahead / attack length in samples (>=1)
	int box1_, box2_;     // moving average lengths, box1_+box2_-1 == window_
	float drive_;         // linear
	float ceiling_;       // in fixed-point sample units (32767 = full scale)
	float releaseMs_;     // <0: auto

	unsigned int n_;      // sample counter (ring indexes)
	float delay_[LIMITER_MAX_WINDOW * 2];
	// peak hold (running minimum) as a monotonic queue
	float holdValue_[LIMITER_MAX_WINDOW];
	unsigned int holdIndex_[LIMITER_MAX_WINDOW];
	unsigned int holdHead_, holdTail_;
	float released_;
	float boxRing1_[LIMITER_MAX_WINDOW];
	float boxRing2_[LIMITER_MAX_WINDOW];
	double sum1_, sum2_;

	volatile float grDb_;
	volatile float inDb_;
	volatile float outDb_;
	float historyGr_[LIMITER_HISTORY];
	float historyIn_[LIMITER_HISTORY];
	volatile int historyPos_;
};

#endif
