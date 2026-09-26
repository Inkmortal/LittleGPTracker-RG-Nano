#ifndef _AUDIO_INSERT_H_
#define _AUDIO_INSERT_H_

#include "Application/Utils/fixed.h"

// An effect a mixer runs on its summed output, before its volume and
// clipping (e.g. the master EQ)
class AudioInsert {
public:
	AudioInsert() : profileSlot_(-1) {}
	virtual ~AudioInsert() {}
	// Stereo interleaved, frames samples per channel
	virtual void Process(fixed *buffer, int frames) = 0;
	// Nothing played this buffer (the mix is silent): drop any held audio
	virtual void Silence() {}
	// What the audio profiler charges this effect to (AudioProfileSlot)
	void SetProfileSlot(int slot) { profileSlot_ = slot; }
	int GetProfileSlot() { return profileSlot_; }

private:
	int profileSlot_;
};

#endif
