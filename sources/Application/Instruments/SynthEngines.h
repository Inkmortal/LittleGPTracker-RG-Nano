#ifndef _SYNTH_ENGINES_H_
#define _SYNTH_ENGINES_H_

// The synth instrument's sound engines besides the original one, after the
// Dirtywave M8's FM Synth, Hypersynth and Wavsynth (behaviour taken from the
// M8 manual; no M8 code or wavetables are used):
//
//  FM4   - four operators A..D, the M8's twelve routings, a feedback loop
//          and a small attack/decay/sustain envelope per operator.
//  HYPER - a chord of six notes, each a pair of detuned saws spread across
//          the stereo field, plus a square sub.
//  WAV   - a raw, 8-bit style oscillator whose shape is bent with size,
//          mult, warp and mirror.
//
// The FM operator loop follows the approach of the Music Synthesizer for
// Android / Dexed "MSFA" engine (Copyright 2012 Google Inc., Apache License
// 2.0): integer phase accumulators that wrap for free, operator gains ramped
// linearly across each control block so level changes never click, and
// feedback taken from the average of the operator's last two outputs, which
// keeps high feedback stable. See NOTICE.md.
//
// Everything here runs per sample on the RG Nano's Cortex-A7, so the tick
// functions are inline and avoid libm.

#include <math.h>

enum SynthEngine {
	SE_SYNTH=0,
	SE_FM4,
	SE_HYPER,
	SE_WAV,
	SE_LAST
} ;

/***************************************************************
 Shared tables
 ***************************************************************/

#define SYNTH_SINE_BITS 11
#define SYNTH_SINE_SIZE (1<<SYNTH_SINE_BITS)
extern float synthSineTable[SYNTH_SINE_SIZE+1] ;
void SynthEnginesInit() ;

// Sine of a 32-bit phase (one cycle = 2^32), linear interpolation
static inline float synthSinU32(unsigned int ph) {
	unsigned int i=ph>>(32-SYNTH_SINE_BITS) ;
	float frac=(float)(ph&((1u<<(32-SYNTH_SINE_BITS))-1))*(1.0f/(float)(1u<<(32-SYNTH_SINE_BITS))) ;
	return synthSineTable[i]+(synthSineTable[i+1]-synthSineTable[i])*frac ;
}

// Cycles (-64..64) to a 32-bit phase offset; wraps like the accumulator
static inline unsigned int synthCyclesToU32(float cycles) {
	return ((unsigned int)(int)(cycles*16777216.0f))<<8 ;
}

static inline float synthU32ToFloat(unsigned int ph) {
	return (float)(ph>>8)*(1.0f/16777216.0f) ;
}

static inline float synthPolyBlep(float t,float dt) {
	if (dt<=0.0f) return 0.0f ;
	if (t<dt) {
		t/=dt ;
		return t+t-t*t-1.0f ;
	} else if (t>1.0f-dt) {
		t=(t-1.0f)/dt ;
		return t*t+t+t+1.0f ;
	}
	return 0.0f ;
}

static inline unsigned int synthXorshift(unsigned int &state) {
	unsigned int x=state ;
	x^=x<<13 ;
	x^=x>>17 ;
	x^=x<<5 ;
	state=x ;
	return x ;
}

/***************************************************************
 FM4
 ***************************************************************/

#define FM4_OPS 4
#define FM4_ALGO_COUNT 12
// Modulation depth of an operator at full level, in cycles (4 pi radians)
#define FM4_MOD_DEPTH 2.0f
#define FM4_RATIO_MIN 25
#define FM4_RATIO_MAX 1600

enum Fm4Shape {
	F4S_SIN=0,
	F4S_SW2,     // half sine: the top half, then silence
	F4S_SW3,     // rectified sine: two humps
	F4S_SW4,     // quarter sine: the rising quarter, twice
	F4S_SW5,     // double-speed sine, then silence
	F4S_SW6,     // double-speed rectified sine, then silence
	F4S_TRI,
	F4S_SAW,
	F4S_SQU,
	F4S_PUL,     // 25% pulse
	F4S_IMP,     // one click per cycle
	F4S_NOISE,
	F4S_LAST
} ;

struct Fm4Algo {
	unsigned char mods_[FM4_OPS] ;   // bit t set: this operator modulates op t
	unsigned char carriers_ ;         // bit per operator heard at the output
	// Diagram position of each operator: column (half steps) and row
	unsigned char col_[FM4_OPS] ;
	unsigned char row_[FM4_OPS] ;
	const char *formula_ ;
} ;

extern const Fm4Algo fm4Algos[FM4_ALGO_COUNT] ;
extern const char *fm4ShapeNames[F4S_LAST] ;
extern const char *fm4AlgoNames[FM4_ALGO_COUNT] ;

// Level knob (00..FF) to amplitude: squared, so low values stay subtle
static inline float fm4LevelAmp(int level) {
	float x=level/255.0f ;
	return x*x ;
}

// Feedback knob to depth (cycles applied to the averaged output)
static inline float fm4FeedbackDepth(int fb) {
	float x=fb/255.0f ;
	return 0.5f*x*x ;
}

int Fm4CarrierCount(int algo) ;

// One set of four operators
struct Fm4Ops {
	unsigned int phase_[FM4_OPS] ;
	unsigned int inc_[FM4_OPS] ;
	float gain_[FM4_OPS] ;        // current gain (ramped per sample)
	float gainStep_[FM4_OPS] ;
	float env_[FM4_OPS] ;         // envelope 0..1
	unsigned char stage_[FM4_OPS] ;  // 0 attack, 1 decay/sustain
	float fb_[FM4_OPS][2] ;       // last two outputs, for feedback
	unsigned int noise_ ;
	float noiseHold_[FM4_OPS] ;
	unsigned char noiseSeg_[FM4_OPS] ;
} ;

// Settings read once per control block
struct Fm4Params {
	int algo_ ;
	int shape_[FM4_OPS] ;
	float ratio_[FM4_OPS] ;
	float level_[FM4_OPS] ;       // amplitude from the level knob
	float fbDepth_[FM4_OPS] ;
	float attackStep_[FM4_OPS] ;  // envelope rise per block
	float decayCoef_[FM4_OPS] ;   // per block
	float sustain_[FM4_OPS] ;
	float modScale_ ;             // LFO "shape": scales every modulator
	float carrierNorm_ ;
} ;

void Fm4Start(Fm4Ops &o,unsigned int seed) ;
// Advances the envelopes one block and sets the pitch and gain ramps
void Fm4Block(Fm4Ops &o,const Fm4Params &p,float baseInc,int blockLength) ;

static inline float fm4Shape(int shape,unsigned int ph,Fm4Ops &o,int op,unsigned int inc) {
	switch(shape) {
		case F4S_SIN:
			return synthSinU32(ph) ;
		case F4S_SW2:
			return (ph&0x80000000u)?0.0f:synthSinU32(ph) ;
		case F4S_SW3:
			return fabsf(synthSinU32(ph)) ;
		case F4S_SW4:
			return (ph&0x40000000u)?0.0f:fabsf(synthSinU32(ph)) ;
		case F4S_SW5:
			return (ph&0x80000000u)?0.0f:synthSinU32(ph<<1) ;
		case F4S_SW6:
			return (ph&0x80000000u)?0.0f:fabsf(synthSinU32(ph<<1)) ;
		case F4S_TRI: {
			float t=synthU32ToFloat(ph+0x40000000u) ;
			return 1.0f-4.0f*fabsf(t-0.5f) ;
		}
		case F4S_SAW:
			return 2.0f*synthU32ToFloat(ph)-1.0f ;
		case F4S_SQU:
			return (ph&0x80000000u)?-1.0f:1.0f ;
		case F4S_PUL:
			return (ph<0x40000000u)?1.0f:-1.0f ;
		case F4S_IMP:
			return (ph<inc || ph<0x04000000u)?1.0f:0.0f ;
		default: {
			// Noise that changes 16 times per cycle: its colour follows the pitch
			unsigned char seg=(unsigned char)(ph>>28) ;
			if (seg!=o.noiseSeg_[op]) {
				o.noiseSeg_[op]=seg ;
				o.noiseHold_[op]=((float)(synthXorshift(o.noise_)&0xFFFF)/32768.0f)-1.0f ;
			}
			return o.noiseHold_[op] ;
		}
	}
}

// One output sample of the four operators (mono, about -1..1)
static inline float fm4Tick(Fm4Ops &o,const Fm4Params &p) {
	const Fm4Algo &a=fm4Algos[p.algo_] ;
	float in[FM4_OPS]={0.0f,0.0f,0.0f,0.0f} ;
	float out=0.0f ;
	for (int op=0;op<FM4_OPS;op++) {
		float g=o.gain_[op]+o.gainStep_[op] ;
		o.gain_[op]=g ;
		unsigned int inc=o.inc_[op] ;
		if (g<=0.0f) {
			o.fb_[op][0]=o.fb_[op][1]=0.0f ;
			o.phase_[op]+=inc ;
			continue ;
		}
		float m=in[op] ;
		if (p.fbDepth_[op]>0.0f) {
			m+=(o.fb_[op][0]+o.fb_[op][1])*p.fbDepth_[op] ;
		}
		unsigned int ph=o.phase_[op]+synthCyclesToU32(m) ;
		float y=fm4Shape(p.shape_[op],ph,o,op,inc)*g ;
		o.fb_[op][0]=o.fb_[op][1] ;
		o.fb_[op][1]=y ;
		unsigned char targets=a.mods_[op] ;
		if (targets) {
			float d=y*(FM4_MOD_DEPTH*p.modScale_) ;
			if (targets&2) in[1]+=d ;
			if (targets&4) in[2]+=d ;
			if (targets&8) in[3]+=d ;
		}
		if (a.carriers_&(1<<op)) {
			out+=y ;
		}
		o.phase_[op]+=inc ;
	}
	return out*p.carrierNorm_ ;
}

/***************************************************************
 HYPER
 ***************************************************************/

#define HYPER_NOTES 6
#define HYPER_NOTE_MIN -24
#define HYPER_NOTE_MAX 36
#define HYPER_CHORD_COUNT 16

extern const char *hyperChordNames[HYPER_CHORD_COUNT] ;
// Index 0 is "custom" (the six notes as edited)
extern const signed char hyperChords[HYPER_CHORD_COUNT][HYPER_NOTES] ;

struct HyperOsc {
	float phase_[HYPER_NOTES][2] ;
	float subPhase_ ;
} ;

struct HyperParams {
	float inc_[HYPER_NOTES][2] ;   // saw a (flat) / saw b (sharp)
	float gain_[HYPER_NOTES] ;     // from "shift"
	float gainL_[2] ;              // per saw of the pair, from "width"
	float gainR_[2] ;
	float subInc_ ;
	float subLevel_ ;
	float norm_ ;
} ;

void HyperStart(HyperOsc &o,unsigned int seed) ;
// Cents between the two saws of note n (each goes half of it up or down)
float HyperDetuneCents(int swarm,int note) ;
// Crossfade gains of the first and second three notes
void HyperShiftGains(int shift,float &first,float &second) ;
// Sub oscillator: octaves below (1 or 2) and level 0..1
void HyperSub(int sub,int &octaves,float &level) ;

static inline float hyperSaw(float &ph,float inc) {
	float t=ph ;
	float s=2.0f*t-1.0f-synthPolyBlep(t,inc) ;
	t+=inc ;
	if (t>=1.0f) t-=1.0f ;
	ph=t ;
	return s ;
}

static inline void hyperTick(HyperOsc &o,const HyperParams &p,float &left,float &right) {
	float a=0.0f,b=0.0f ;
	for (int n=0;n<HYPER_NOTES;n++) {
		float g=p.gain_[n] ;
		if (g<=0.0f) continue ;
		a+=g*hyperSaw(o.phase_[n][0],p.inc_[n][0]) ;
		b+=g*hyperSaw(o.phase_[n][1],p.inc_[n][1]) ;
	}
	a*=p.norm_ ;
	b*=p.norm_ ;
	left=a*p.gainL_[0]+b*p.gainL_[1] ;
	right=a*p.gainR_[0]+b*p.gainR_[1] ;
	if (p.subLevel_>0.0f) {
		float st=o.subPhase_ ;
		float s=(st<0.5f)?1.0f:-1.0f ;
		s+=synthPolyBlep(st,p.subInc_) ;
		float t2=st+0.5f ;
		if (t2>=1.0f) t2-=1.0f ;
		s-=synthPolyBlep(t2,p.subInc_) ;
		st+=p.subInc_ ;
		if (st>=1.0f) st-=1.0f ;
		o.subPhase_=st ;
		s*=p.subLevel_*0.6f ;
		left+=s ;
		right+=s ;
	}
}

/***************************************************************
 WAV
 ***************************************************************/

enum WavShape {
	WVS_PULSE12=0,
	WVS_PULSE25,
	WVS_PULSE50,
	WVS_PULSE75,
	WVS_SAW,
	WVS_TRIANGLE,
	WVS_SINE,
	WVS_TONENOISE,  // a short looping noise pattern: noise with a pitch
	WVS_NOISE,      // LFSR noise clocked by the note
	WVS_LAST
} ;

enum WavLimit {
	WVL_SOFT=0,   // gentle saturation (what the drive knob always did)
	WVL_CLIP,
	WVL_SIN,
	WVL_FOLD,
	WVL_WRAP,
	WVL_LAST
} ;

extern const char *wavShapeNames[WVS_LAST] ;
extern const char *wavLimitNames[WVL_LAST] ;
#define WAV_TONENOISE_STEPS 93
extern float wavToneNoise[WAV_TONENOISE_STEPS] ;

struct WavOsc {
	float phase_ ;
	unsigned int lfsr_ ;
	float noiseClock_ ;
	float noiseValue_ ;
} ;

struct WavParams {
	int shape_ ;
	float steps_ ;      // size: steps per cycle
	float invSteps_ ;
	float mult_ ;       // repeats of the shape per cycle
	float warpEnd_ ;    // warp: the shape fits in 0..warpEnd, then silence
	float invWarpEnd_ ;
	float mirror_ ;     // mirror: where the middle of the shape sits
	float invMirrorA_ ;
	float invMirrorB_ ;
} ;

void WavStart(WavOsc &o,unsigned int seed) ;
// Knob values to the oscillator settings
void WavSetup(WavParams &p,int shape,int size,int mult,int warp,int mirror) ;
int WavStepsFromSize(int size) ;
float WavMultFromParam(int mult) ;

// Where in the base shape a point of the cycle reads (after warp, mult,
// mirror and size); returns false in the silent part of a warped cycle.
static inline bool wavReadPoint(const WavParams &p,float t,float &u) {
	if (t>=p.warpEnd_) return false ;
	t*=p.invWarpEnd_ ;
	t*=p.mult_ ;
	t-=(float)(int)t ;
	if (t<p.mirror_) {
		t=0.5f*t*p.invMirrorA_ ;
	} else {
		t=0.5f+0.5f*(t-p.mirror_)*p.invMirrorB_ ;
	}
	t=(float)(int)(t*p.steps_)*p.invSteps_ ;
	u=t ;
	return true ;
}

static inline float wavShapeValue(int shape,float u) {
	switch(shape) {
		case WVS_PULSE12: return (u<0.125f)?1.0f:-1.0f ;
		case WVS_PULSE25: return (u<0.25f)?1.0f:-1.0f ;
		case WVS_PULSE50: return (u<0.5f)?1.0f:-1.0f ;
		case WVS_PULSE75: return (u<0.75f)?1.0f:-1.0f ;
		case WVS_SAW: return 2.0f*u-1.0f ;
		case WVS_TRIANGLE: {
			float x=u+0.25f ;
			if (x>=1.0f) x-=1.0f ;
			return 1.0f-4.0f*fabsf(x-0.5f) ;
		}
		case WVS_SINE: return synthSinU32((unsigned int)(u*4294967040.0f)) ;
		case WVS_TONENOISE: {
			int i=(int)(u*WAV_TONENOISE_STEPS) ;
			if (i>=WAV_TONENOISE_STEPS) i=WAV_TONENOISE_STEPS-1 ;
			return wavToneNoise[i] ;
		}
		default: return 0.0f ;
	}
}

static inline float wavTick(WavOsc &o,const WavParams &p,float inc) {
	float out ;
	if (p.shape_==WVS_NOISE) {
		// 15-bit LFSR clocked 32 times per note cycle
		o.noiseClock_+=inc*32.0f ;
		while (o.noiseClock_>=1.0f) {
			o.noiseClock_-=1.0f ;
			unsigned int bit=(o.lfsr_^(o.lfsr_>>1))&1u ;
			o.lfsr_=(o.lfsr_>>1)|(bit<<14) ;
			o.noiseValue_=(o.lfsr_&1u)?1.0f:-1.0f ;
		}
		out=o.noiseValue_ ;
	} else {
		float u ;
		if (wavReadPoint(p,o.phase_,u)) {
			out=wavShapeValue(p.shape_,u) ;
			// 8-bit samples, like the M8's wave buffer
			out=(float)(int)(out*127.0f+(out>=0.0f?0.5f:-0.5f))*(1.0f/127.0f) ;
		} else {
			out=0.0f ;
		}
	}
	float t=o.phase_+inc ;
	if (t>=1.0f) t-=(float)(int)t ;
	o.phase_=t ;
	return out ;
}

// The drive stage's clipping, used by every engine (soft is the default)
static inline float synthLimit(int mode,float x) {
	switch(mode) {
		case WVL_CLIP:
			return x>1.0f?1.0f:(x<-1.0f?-1.0f:x) ;
		case WVL_SIN: {
			// sin(x * pi/2): smooth, and folds over when pushed hard
			float c=x*0.25f ;
			c-=(float)floor(c) ;
			return synthSinU32((unsigned int)(c*4294967040.0f)) ;
		}
		case WVL_FOLD: {
			float y=x+1.0f ;
			y-=4.0f*(float)floor(y*0.25f) ;   // 0..4
			return y<2.0f?y-1.0f:3.0f-y ;
		}
		case WVL_WRAP: {
			float y=x+1.0f ;
			y-=2.0f*(float)floor(y*0.5f) ;    // 0..2
			return y-1.0f ;
		}
		default: {
			if (x>3.0f) return 1.0f ;
			if (x<-3.0f) return -1.0f ;
			float x2=x*x ;
			return x*(27.0f+x2)/(27.0f+9.0f*x2) ;
		}
	}
}

#endif
