#include "SynthPlaits.h"

#include "Externals/Plaits/plaits/dsp/drums/analog_bass_drum.h"
#include "Externals/Plaits/plaits/dsp/drums/analog_snare_drum.h"
#include "Externals/Plaits/plaits/dsp/drums/hi_hat.h"
#include "Externals/Plaits/plaits/dsp/drums/synthetic_bass_drum.h"
#include "Externals/Plaits/plaits/dsp/drums/synthetic_snare_drum.h"
#include "Externals/Plaits/plaits/dsp/fx/overdrive.h"
#include "Externals/Plaits/plaits/dsp/physical_modelling/modal_voice.h"
#include "Externals/Plaits/plaits/dsp/physical_modelling/string_voice.h"
#include "Externals/Plaits/stmlib/dsp/limiter.h"
#include "Externals/Plaits/stmlib/utils/buffer_allocator.h"

#include <string.h>

const char *synthDrumModelNames[SDM_LAST]={
	"kick 808","kick 909","snare 808","snare 909","hat 808","hat ring"
} ;

const char *synthPhysModelNames[SPM_LAST]={
	"modal","string"
} ;

// The string's delay lines: Plaits' String allocates kDelayLineSize and a
// quarter of that again
#define PLAITS_STRING_FLOATS (plaits::kDelayLineSize+plaits::kDelayLineSize/4)

// Output levels (measured with tools/dsp-harness/plaits_engines_check.cpp)
#define SYNTH_DRUM_GAIN 1.6f
#define SYNTH_STRING_GAIN 2.5f

// One voice of every model. The models keep ringing into the next hit the
// way the module's do (the synth fades the old note out first).
struct SynthPlaitsVoice {
	plaits::AnalogBassDrum analogKick_ ;
	plaits::SyntheticBassDrum syntheticKick_ ;
	plaits::Overdrive kickDrive_ ;
	plaits::AnalogSnareDrum analogSnare_ ;
	plaits::SyntheticSnareDrum syntheticSnare_ ;
	plaits::HiHat<plaits::SquareNoise,plaits::SwingVCA,true,false> hat808_ ;
	plaits::HiHat<plaits::RingModNoise,plaits::LinearVCA,false,true> hatRing_ ;
	plaits::ModalVoice modal_ ;
	plaits::StringVoice string_ ;
	pstmlib::Limiter limiter_ ;
	float stringMemory_[PLAITS_STRING_FLOATS] ;
	float temp1_[SYNTH_PLAITS_MAX_BLOCK] ;
	float temp2_[SYNTH_PLAITS_MAX_BLOCK] ;
	float aux_[SYNTH_PLAITS_MAX_BLOCK] ;
	float materialLp_ ;       // Plaits smooths the structure knob (modal)
	bool materialFresh_ ;     // a reset model starts at the knob
	int lastModel_ ;          // a changed model starts from rest
} ;

static void resetModels(SynthPlaitsVoice *v) {
	v->analogKick_.Init() ;
	v->syntheticKick_.Init() ;
	v->kickDrive_.Init() ;
	v->analogSnare_.Init() ;
	v->syntheticSnare_.Init() ;
	v->hat808_.Init() ;
	v->hatRing_.Init() ;
	v->modal_.Init() ;
	memset(v->stringMemory_,0,sizeof(v->stringMemory_)) ;
	pstmlib::BufferAllocator allocator(v->stringMemory_,sizeof(v->stringMemory_)) ;
	v->string_.Init(&allocator) ;
	v->limiter_.Init() ;
	v->materialLp_=0.0f ;
	v->materialFresh_=true ;
}

SynthPlaitsVoice *SynthPlaitsCreate() {
	SynthPlaitsVoice *v=new SynthPlaitsVoice ;
	resetModels(v) ;
	v->lastModel_=-1 ;
	return v ;
}

void SynthPlaitsDelete(SynthPlaitsVoice *v) {
	delete v ;
}

void SynthPlaitsReset(SynthPlaitsVoice *v) {
	resetModels(v) ;
	v->lastModel_=-1 ;
}

static inline float clamp01(float x) {
	if (x<0.0f) return 0.0f ;
	if (x>1.0f) return 1.0f ;
	return x ;
}

// The knob mappings are the Plaits engines' own (bass_drum_engine.cc,
// snare_drum_engine.cc, hi_hat_engine.cc): 'tone' is their timbre, 'decay'
// their morph and 'snap' their harmonics knob.
void SynthDrumRender(SynthPlaitsVoice *v,int model,bool strike,float f0,
                     float tone,float decay,float snap,float accent,
                     float *out,int n) {
	if (n<=0) return ;
	if (n>SYNTH_PLAITS_MAX_BLOCK) n=SYNTH_PLAITS_MAX_BLOCK ;
	int key=model ;
	if (key!=v->lastModel_) {
		resetModels(v) ;
		v->lastModel_=key ;
	}
	tone=clamp01(tone) ;
	decay=clamp01(decay) ;
	snap=clamp01(snap) ;
	accent=clamp01(accent) ;
	switch(model) {
		case SDM_KICK808: {
			float attackFm=snap*4.0f ;
			if (attackFm>1.0f) attackFm=1.0f ;
			float selfFm=clamp01(snap*4.0f-1.0f) ;
			float drive=snap*2.0f-1.0f ;
			if (drive<0.0f) drive=0.0f ;
			float lowOnly=1.0f-16.0f*f0 ;
			if (lowOnly<0.0f) lowOnly=0.0f ;
			v->analogKick_.Render(false,strike,accent,f0,tone,decay,attackFm,selfFm,out,n) ;
			v->kickDrive_.Process(0.5f+0.5f*drive*lowOnly,out,n) ;
			break ;
		}
		case SDM_KICK909: {
			float fmAmount=snap*2.0f ;
			if (fmAmount>1.0f) fmAmount=1.0f ;
			float fmDecay=clamp01(snap*2.0f-1.0f) ;
			v->syntheticKick_.Render(false,strike,accent,f0,tone,decay,
			                         0.4f-0.25f*decay*decay,fmAmount,fmDecay,out,n) ;
			break ;
		}
		case SDM_SNARE808:
			v->analogSnare_.Render(false,strike,accent,f0,tone,decay,snap,out,n) ;
			break ;
		case SDM_SNARE909:
			v->syntheticSnare_.Render(false,strike,accent,f0,tone,decay,snap,out,n) ;
			break ;
		case SDM_HAT808:
			v->hat808_.Render(false,strike,accent,f0,tone,decay,snap,v->temp1_,v->temp2_,out,n) ;
			break ;
		default:
			v->hatRing_.Render(false,strike,accent,f0,tone,decay,snap,v->temp1_,v->temp2_,out,n) ;
			break ;
	}
	// Twice Plaits' output gain for its drum engines (0.8): the synth's
	// other engines peak about that loud at the same volume
	for (int i=0;i<n;i++) out[i]*=SYNTH_DRUM_GAIN ;
}

// Plaits' modal_engine.cc and string_engine.cc: 'material' is their
// harmonics (structure), 'bright' their timbre, 'decay' their morph
// (damping). Their output goes through the module's limiter.
void SynthPhysRender(SynthPlaitsVoice *v,int model,bool strike,float f0,
                     float material,float bright,float decay,float accent,
                     float *out,int n) {
	if (n<=0) return ;
	if (n>SYNTH_PLAITS_MAX_BLOCK) n=SYNTH_PLAITS_MAX_BLOCK ;
	int key=SDM_LAST+model ;
	if (key!=v->lastModel_) {
		resetModels(v) ;
		v->lastModel_=key ;
	}
	material=clamp01(material) ;
	bright=clamp01(bright) ;
	decay=clamp01(decay) ;
	accent=clamp01(accent) ;
	for (int i=0;i<n;i++) {
		out[i]=0.0f ;
		v->aux_[i]=0.0f ;
	}
	if (model==SPM_MODAL) {
		if (v->materialFresh_) {
			// the module's smoothing starts at the knob, not at 0
			v->materialLp_=material ;
			v->materialFresh_=false ;
		}
		v->materialLp_+=0.01f*(material-v->materialLp_) ;
		v->modal_.Render(false,strike,accent,f0,v->materialLp_,bright,decay,
		                 v->temp1_,out,v->aux_,n) ;
	} else {
		// The string's delay line reaches down to ~43 Hz
		v->string_.Render(false,strike,accent,f0,material,bright*bright,decay,
		                  v->temp1_,out,v->aux_,n) ;
		// A plucked string comes out far quieter than a struck bar
		for (int i=0;i<n;i++) out[i]*=SYNTH_STRING_GAIN ;
	}
	v->limiter_.Process(1.0f,out,n) ;
}
