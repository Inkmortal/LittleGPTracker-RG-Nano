#ifndef _AUDIO_INSERT_H_
#define _AUDIO_INSERT_H_

#include "Application/Utils/fixed.h"

// An effect a mixer runs on its summed output, before its volume and
// clipping (e.g. the master EQ)
class AudioInsert {
public:
	virtual ~AudioInsert() {}
	// Stereo interleaved, frames samples per channel
	virtual void Process(fixed *buffer, int frames) = 0;
	// Nothing played this buffer (the mix is silent): drop any held audio
	virtual void Silence() {}
};

#endif
