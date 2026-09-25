#ifndef _INSTRUMENT_EQ_H_
#define _INSTRUMENT_EQ_H_

// Every synth and sample instrument has its own three-band EQ (like the
// M8's instrument EQ): the master EQ's low shelf / mid bell / high shelf
// (ThreeBandEQ), run on each voice in the instrument's render. Flat by
// default; a band at 0 dB costs nothing, a flat EQ is skipped.

#include "Application/Mixer/ThreeBandEQ.h"
#include "Application/Model/Song.h"
#include "Application/Utils/fixed.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/Variable.h"
#include "Foundation/Variables/VariableContainer.h"

// Variable ids: 'I'nstrument 'Q' band (L/M/H) Gain / Freq
#define IEQ_LOW_GAIN  MAKE_FOURCC('I','Q','L','G')
#define IEQ_LOW_FREQ  MAKE_FOURCC('I','Q','L','F')
#define IEQ_MID_GAIN  MAKE_FOURCC('I','Q','M','G')
#define IEQ_MID_FREQ  MAKE_FOURCC('I','Q','M','F')
#define IEQ_HIGH_GAIN MAKE_FOURCC('I','Q','H','G')
#define IEQ_HIGH_FREQ MAKE_FOURCC('I','Q','H','F')

class InstrumentEQ {
public:
	InstrumentEQ() ;
	void Create(VariableContainer &owner) ;
	// Back to flat (a fresh synth preset)
	void Reset() ;
	// Once per rendered buffer: reads the knobs, redesigns when they moved.
	// False when the EQ is flat (nothing to do).
	bool Prepare() ;
	// A voice starts from silence: no filter memory from an older note
	void ResetVoice(int channel) ;
	// One mono sample of a voice (the synth, before its pan)
	inline float TickMono(int channel,float x) {
		double y=x ;
		double (*z)[2][2]=state_[channel] ;
		for (int k=0;k<activeCount_;k++) {
			int b=active_[k] ;
			y=ThreeBandEQ::Step(coeffs_[b],z[b][0],y) ;
		}
		return (float)y ;
	}
	// A rendered stereo voice buffer (the sampler)
	void ProcessStereo(int channel,fixed *buffer,int frames) ;

	static const FourCC Ids[6] ;
private:
	Variable *vars_[6] ;
	int params_[6] ;
	float rate_ ;
	float coeffs_[3][5] ;
	int active_[3] ;      // bands not at 0 dB
	int activeCount_ ;
	double state_[SONG_CHANNEL_COUNT][3][2][2] ; // [voice][band][L/R][z1,z2]
} ;

#endif
