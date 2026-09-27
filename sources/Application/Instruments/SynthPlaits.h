#ifndef _SYNTH_PLAITS_H_
#define _SYNTH_PLAITS_H_

// The synth's DRUM and PHYS engines: models from Emilie Gillet's Plaits
// module (MIT, see NOTICE.md and sources/Externals/Plaits), run at the
// mixer's rate.
//
//  DRUM - analog and synthetic kicks and snares, two hi-hats. Each model is
//         a small circuit simulation: a resonant filter pinged by a pulse
//         (808 kick, 808 snare's body), sine oscillators with pitch and FM
//         envelopes (909 kick and snare), six square oscillators or ring
//         modulated pairs through band- and high-pass filters (hats).
//  PHYS - physical models: a modal resonator (a bank of tuned band-pass
//         filters struck by a mallet: bars, bells, plates, drums) and an
//         inharmonic string (Karplus-Strong delay line with a stretching
//         all-pass, plucked by a burst of noise).
//
// Both make their own decay, so the synth's amp envelope usually stays
// open (the presets set it so) and a voice ends when the model is silent.
//
// Plaits' headers stay in SynthPlaits.cpp: the macro synth's Braids code
// brings an older stmlib that must not meet the one Plaits uses.

enum SynthDrumModel {
	SDM_KICK808=0,
	SDM_KICK909,
	SDM_SNARE808,
	SDM_SNARE909,
	SDM_HAT808,
	SDM_HATRING,
	SDM_LAST
} ;

enum SynthPhysModel {
	SPM_MODAL=0,
	SPM_STRING,
	SPM_LAST
} ;

// Largest block the models render at once (Plaits' kMaxBlockSize)
#define SYNTH_PLAITS_MAX_BLOCK 24

struct SynthPlaitsVoice ;

SynthPlaitsVoice *SynthPlaitsCreate() ;
void SynthPlaitsDelete(SynthPlaitsVoice *v) ;
// Every model back at rest (silent, nothing ringing)
void SynthPlaitsReset(SynthPlaitsVoice *v) ;

// Knobs are 0..1. f0 is the note's frequency / sample rate. 'strike'
// starts a new hit; n <= SYNTH_PLAITS_MAX_BLOCK samples are written to out.
void SynthDrumRender(SynthPlaitsVoice *v,int model,bool strike,float f0,
                     float tone,float decay,float snap,float accent,
                     float *out,int n) ;
void SynthPhysRender(SynthPlaitsVoice *v,int model,bool strike,float f0,
                     float material,float bright,float decay,float accent,
                     float *out,int n) ;

// Output level below which a model counts as silent (-80 dB under the
// model's own level, so the output gains don't lengthen or cut the tail)
float SynthDrumSilence() ;
float SynthPhysSilence() ;

// Model names for the knob and per-model help text
extern const char *synthDrumModelNames[SDM_LAST] ;
extern const char *synthPhysModelNames[SPM_LAST] ;

#endif
