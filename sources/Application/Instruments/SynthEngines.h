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
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

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
static inline __attribute__((always_inline)) float synthSinU32(unsigned int ph) {
	unsigned int i=ph>>(32-SYNTH_SINE_BITS) ;
	float frac=(float)(ph&((1u<<(32-SYNTH_SINE_BITS))-1))*(1.0f/(float)(1u<<(32-SYNTH_SINE_BITS))) ;
	return synthSineTable[i]+(synthSineTable[i+1]-synthSineTable[i])*frac ;
}

// Cycles (-64..64) to a 32-bit phase offset; wraps like the accumulator
static inline __attribute__((always_inline)) unsigned int synthCyclesToU32(float cycles) {
	return ((unsigned int)(int)(cycles*16777216.0f))<<8 ;
}

static inline float synthU32ToFloat(unsigned int ph) {
	return (float)(ph>>8)*(1.0f/16777216.0f) ;
}

static inline __attribute__((always_inline)) float synthPolyBlep(float t,float dt) {
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

static inline __attribute__((always_inline)) float fm4Shape(int shape,unsigned int ph,Fm4Ops &o,int op,unsigned int inc) {
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

// The longest run the block renderers below take at once
#define SYNTH_ENGINE_MAX_RUN 64

// One operator over a run of samples: the arithmetic of fm4Tick for that
// operator, with its shape, feedback and routing fixed for the run (the
// shape switch is resolved at compile time)
// The buffers start unwritten: the first operator to feed a modulation
// input or the output stores into it (first bit set in 'first': bit t for
// in[t], bit 0 for out), the others add (0 + x is x, so the sums are the
// same as from zeroed buffers); 'modulated' says whether an earlier
// operator feeds this one at all.
// What one operator does for one sample k (gain g already stepped)
struct Fm4OpRun {
	unsigned int phase ;
	unsigned int inc ;
	float fb0,fb1 ;
	float fbDepth ;
	unsigned char targets ;
	bool carrier ;
	float depth ;
	unsigned char first ;
	bool modulated ;
} ;

template <int SHAPE>
static inline __attribute__((always_inline)) void fm4OpSample(Fm4OpRun &r,Fm4Ops &o,int op,
                                  float in[FM4_OPS][SYNTH_ENGINE_MAX_RUN],float *out,int k,float g) {
	if (g<=0.0f) {
		r.fb0=r.fb1=0.0f ;
		r.phase+=r.inc ;
		// a silent operator still starts the buffers it is first in
		if (r.first&2) in[1][k]=0.0f ;
		if (r.first&4) in[2][k]=0.0f ;
		if (r.first&8) in[3][k]=0.0f ;
		if (r.first&1) out[k]=0.0f ;
		return ;
	}
	float m=r.modulated?in[op][k]:0.0f ;
	if (r.fbDepth>0.0f) {
		m+=(r.fb0+r.fb1)*r.fbDepth ;
	}
	unsigned int ph=r.phase+synthCyclesToU32(m) ;
	float y=fm4Shape(SHAPE,ph,o,op,r.inc)*g ;
	r.fb0=r.fb1 ;
	r.fb1=y ;
	if (r.targets) {
		float d=y*r.depth ;
		if (r.targets&2) { if (r.first&2) in[1][k]=d ; else in[1][k]+=d ; }
		if (r.targets&4) { if (r.first&4) in[2][k]=d ; else in[2][k]+=d ; }
		if (r.targets&8) { if (r.first&8) in[3][k]=d ; else in[3][k]+=d ; }
	}
	if (r.carrier) {
		if (r.first&1) out[k]=y ; else out[k]+=y ;
	}
	r.phase+=r.inc ;
}

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
// Four samples of a sine operator without feedback, all four at once: the
// phases are exact (integer steps), the table read and its interpolation
// (a fused multiply-add, as the compiler makes of synthSinU32) and every
// product are the same operations as fm4OpSample's, so the numbers are too
static inline __attribute__((always_inline)) void fm4SineQuad(Fm4OpRun &r,int op,
                                  float in[FM4_OPS][SYNTH_ENGINE_MAX_RUN],float *out,int k,
                                  float32x4_t g,uint32x4_t incRamp) {
	uint32x4_t ph=vaddq_u32(vdupq_n_u32(r.phase),incRamp) ;
	if (r.modulated) {
		int32x4_t off=vcvtq_n_s32_f32(vld1q_f32(&in[op][k]),24) ;
		ph=vaddq_u32(ph,vshlq_n_u32(vreinterpretq_u32_s32(off),8)) ;
	}
	uint32x4_t idx=vshrq_n_u32(ph,32-SYNTH_SINE_BITS) ;
	float32x4_t frac=vcvtq_n_f32_u32(vandq_u32(ph,vdupq_n_u32((1u<<(32-SYNTH_SINE_BITS))-1)),
	                                 32-SYNTH_SINE_BITS) ;
	unsigned int i[4] ;
	vst1q_u32(i,idx) ;
	float32x4_t t0=vdupq_n_f32(0.0f),t1=vdupq_n_f32(0.0f) ;
	t0=vld1q_lane_f32(synthSineTable+i[0],t0,0) ;
	t1=vld1q_lane_f32(synthSineTable+i[0]+1,t1,0) ;
	t0=vld1q_lane_f32(synthSineTable+i[1],t0,1) ;
	t1=vld1q_lane_f32(synthSineTable+i[1]+1,t1,1) ;
	t0=vld1q_lane_f32(synthSineTable+i[2],t0,2) ;
	t1=vld1q_lane_f32(synthSineTable+i[2]+1,t1,2) ;
	t0=vld1q_lane_f32(synthSineTable+i[3],t0,3) ;
	t1=vld1q_lane_f32(synthSineTable+i[3]+1,t1,3) ;
	float32x4_t y=vmulq_f32(vfmaq_f32(t0,frac,vsubq_f32(t1,t0)),g) ;
	if (r.targets) {
		float32x4_t d=vmulq_f32(y,vdupq_n_f32(r.depth)) ;
		for (int t=1;t<FM4_OPS;t++) {
			if (!(r.targets&(1<<t))) continue ;
			if (r.first&(1<<t)) vst1q_f32(&in[t][k],d) ;
			else vst1q_f32(&in[t][k],vaddq_f32(vld1q_f32(&in[t][k]),d)) ;
		}
	}
	if (r.carrier) {
		if (r.first&1) vst1q_f32(out+k,y) ;
		else vst1q_f32(out+k,vaddq_f32(vld1q_f32(out+k),y)) ;
	}
	r.fb0=vgetq_lane_f32(y,2) ;
	r.fb1=vgetq_lane_f32(y,3) ;
	r.phase+=4u*r.inc ;
}
#endif

template <int SHAPE>
static inline void fm4OperatorRun(Fm4Ops &o,const Fm4Params &p,const Fm4Algo &a,int op,
                                  float in[FM4_OPS][SYNTH_ENGINE_MAX_RUN],float *out,int n,
                                  unsigned char first,bool modulated) {
	float g=o.gain_[op] ;
	const float step=o.gainStep_[op] ;
	Fm4OpRun r ;
	r.phase=o.phase_[op] ;
	r.inc=o.inc_[op] ;
	r.fb0=o.fb_[op][0] ;
	r.fb1=o.fb_[op][1] ;
	r.fbDepth=p.fbDepth_[op] ;
	r.targets=a.mods_[op] ;
	r.carrier=(a.carriers_&(1<<op))!=0 ;
	r.depth=FM4_MOD_DEPTH*p.modScale_ ;
	r.first=first ;
	r.modulated=modulated ;
	int k=0 ;
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
	if (SHAPE==F4S_SIN && !(r.fbDepth>0.0f) && n>=4) {
		// no feedback: the samples do not depend on each other
		unsigned int rampInit[4]={0u,r.inc,2u*r.inc,3u*r.inc} ;
		const uint32x4_t incRamp=vld1q_u32(rampInit) ;
		for (;k+4<=n;k+=4) {
			// the gain ramp still steps sample by sample (same rounding)
			float g4[4] ;
			g4[0]=g=g+step ;
			g4[1]=g=g+step ;
			g4[2]=g=g+step ;
			g4[3]=g=g+step ;
			if (g4[0]>0.0f && g4[1]>0.0f && g4[2]>0.0f && g4[3]>0.0f) {
				fm4SineQuad(r,op,in,out,k,vld1q_f32(g4),incRamp) ;
			} else {
				for (int j=0;j<4;j++) fm4OpSample<SHAPE>(r,o,op,in,out,k+j,g4[j]) ;
			}
		}
	}
#endif
	for (;k<n;k++) {
		g=g+step ;
		fm4OpSample<SHAPE>(r,o,op,in,out,k,g) ;
	}
	o.gain_[op]=g ;
	o.phase_[op]=r.phase ;
	o.fb_[op][0]=r.fb0 ;
	o.fb_[op][1]=r.fb1 ;
}

// n (<= SYNTH_ENGINE_MAX_RUN) samples of the four operators: the same
// numbers as n calls of fm4Tick. Operators only modulate later ones, so
// each can run the whole block in turn. Two or more noise operators share
// one random sequence, drawn in sample order: those go sample by sample.
static inline void fm4Render(Fm4Ops &o,const Fm4Params &p,float *out,int n) {
	int noiseOps=0 ;
	for (int op=0;op<FM4_OPS;op++) {
		if (p.shape_[op]==F4S_NOISE) noiseOps++ ;
	}
	if (noiseOps>1) {
		for (int k=0;k<n;k++) out[k]=fm4Tick(o,p) ;
		return ;
	}
	const Fm4Algo &a=fm4Algos[p.algo_] ;
	float in[FM4_OPS][SYNTH_ENGINE_MAX_RUN] ;
	unsigned char written=0 ;   // bit t: in[t] started, bit 0: out started
	for (int op=0;op<FM4_OPS;op++) {
		unsigned char wants=(unsigned char)(a.mods_[op]&0x0E) ;
		if (a.carriers_&(1<<op)) wants|=1 ;
		unsigned char first=(unsigned char)(wants&~written) ;
		bool modulated=(written&(1<<op))!=0 ;
		written|=wants ;
		switch(p.shape_[op]) {
			case F4S_SIN: fm4OperatorRun<F4S_SIN>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SW2: fm4OperatorRun<F4S_SW2>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SW3: fm4OperatorRun<F4S_SW3>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SW4: fm4OperatorRun<F4S_SW4>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SW5: fm4OperatorRun<F4S_SW5>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SW6: fm4OperatorRun<F4S_SW6>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_TRI: fm4OperatorRun<F4S_TRI>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SAW: fm4OperatorRun<F4S_SAW>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_SQU: fm4OperatorRun<F4S_SQU>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_PUL: fm4OperatorRun<F4S_PUL>(o,p,a,op,in,out,n,first,modulated) ; break ;
			case F4S_IMP: fm4OperatorRun<F4S_IMP>(o,p,a,op,in,out,n,first,modulated) ; break ;
			default: fm4OperatorRun<F4S_NOISE>(o,p,a,op,in,out,n,first,modulated) ; break ;
		}
	}
	if (!(written&1)) {
		for (int k=0;k<n;k++) out[k]=0.0f ;
	}
	for (int k=0;k<n;k++) out[k]*=p.carrierNorm_ ;
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

// The saws run on 32-bit integer phases (one cycle = 2^32, wrapping by
// itself), as the FM operators and Braids do: the phase is exact to 2^-32
// of a cycle whatever the note (a float phase near 1.0 keeps only 24 bits)
// and four samples ahead is just four steps added - which lets the device
// work four samples at a time.
struct HyperOsc {
	unsigned int phase_[HYPER_NOTES][2] ;
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

// Phase step of a saw: cycles per sample (0..0.45) as a 32-bit step
static inline unsigned int hyperPhaseStep(float inc) {
	return inc>0.0f?(unsigned int)(inc*4294967296.0f):0u ;
}

// The PolyBLEP correction of a saw at t (0..1), dt = its step, invDt =
// 1/dt: the same polynomial as synthPolyBlep, a multiply for the division
static inline __attribute__((always_inline)) float hyperBlep(float t,float dt,float invDt) {
	if (dt<=0.0f) return 0.0f ;
	if (t<dt) {
		t*=invDt ;
		return t+t-t*t-1.0f ;
	} else if (t>1.0f-dt) {
		t=(t-1.0f)*invDt ;
		return t*t+t+t+1.0f ;
	}
	return 0.0f ;
}

static inline __attribute__((always_inline)) float hyperSaw(unsigned int &ph,unsigned int step,
                                                            float dt,float invDt) {
	float t=(float)ph*(1.0f/4294967296.0f) ;
	float s=2.0f*t-1.0f-hyperBlep(t,dt,invDt) ;
	ph+=step ;
	return s ;
}

static inline void hyperTick(HyperOsc &o,const HyperParams &p,float &left,float &right) {
	float a=0.0f,b=0.0f ;
	for (int n=0;n<HYPER_NOTES;n++) {
		float g=p.gain_[n] ;
		if (g<=0.0f) continue ;
		float dt=p.inc_[n][0] ;
		a+=g*hyperSaw(o.phase_[n][0],hyperPhaseStep(dt),dt,dt>0.0f?1.0f/dt:0.0f) ;
		dt=p.inc_[n][1] ;
		b+=g*hyperSaw(o.phase_[n][1],hyperPhaseStep(dt),dt,dt>0.0f?1.0f/dt:0.0f) ;
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

// One PolyBLEP saw over n samples, times g, stored into (assign) or added
// to acc. On the device four samples at a time in NEON: the plain ramp
// 2t-1 for all four, and the correction only for the few groups of four
// that sit next to the jump (it touches two samples per cycle); the
// numbers are those of hyperSaw.
static inline void hyperSawRun(unsigned int &phase,float inc,float g,float *acc,int n,bool assign) {
	const unsigned int step=hyperPhaseStep(inc) ;
	const float invDt=inc>0.0f?1.0f/inc:0.0f ;
	int k=0 ;
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
	if (inc>0.0f && n>=4) {
		const float32x4_t one=vdupq_n_f32(1.0f) ;
		const float32x4_t two=vdupq_n_f32(2.0f) ;
		const float32x4_t vg=vdupq_n_f32(g) ;
		const float32x4_t vdt=vdupq_n_f32(inc) ;
		const float32x4_t upper=vdupq_n_f32(1.0f-inc) ;
		unsigned int rampInit[4]={0u,step,2u*step,3u*step} ;
		const uint32x4_t ramp=vld1q_u32(rampInit) ;
		unsigned int ph=phase ;
		for (;k+4<=n;k+=4) {
			uint32x4_t u=vaddq_u32(vdupq_n_u32(ph),ramp) ;
			float32x4_t t=vcvtq_n_f32_u32(u,32) ;
			float32x4_t s=vsubq_f32(vmulq_f32(two,t),one) ;
			uint32x4_t near=vorrq_u32(vcltq_f32(t,vdt),vcgtq_f32(t,upper)) ;
			uint32x2_t near2=vorr_u32(vget_low_u32(near),vget_high_u32(near)) ;
			if (vget_lane_u32(near2,0)|vget_lane_u32(near2,1)) {
				// a jump is near: correct those samples
				float tt[4],ss[4] ;
				vst1q_f32(tt,t) ;
				vst1q_f32(ss,s) ;
				for (int j=0;j<4;j++) ss[j]-=hyperBlep(tt[j],inc,invDt) ;
				s=vld1q_f32(ss) ;
			}
			float32x4_t y=vmulq_f32(vg,s) ;
			if (assign) {
				vst1q_f32(acc+k,y) ;
			} else {
				vst1q_f32(acc+k,vaddq_f32(vld1q_f32(acc+k),y)) ;
			}
			ph+=4u*step ;
		}
		phase=ph ;
	}
#endif
	for (;k<n;k++) {
		float y=g*hyperSaw(phase,step,inc,invDt) ;
		if (assign) acc[k]=y ; else acc[k]+=y ;
	}
}

// n (<= SYNTH_ENGINE_MAX_RUN) samples of hyperTick: each saw runs through
// the whole block in turn, summed per sample in the same order
static inline void hyperRender(HyperOsc &o,const HyperParams &p,float *left,float *right,int n) {
	float a[SYNTH_ENGINE_MAX_RUN] ;
	float b[SYNTH_ENGINE_MAX_RUN] ;
	bool first=true ;   // the first sounding note stores, the rest add
	for (int note=0;note<HYPER_NOTES;note++) {
		const float g=p.gain_[note] ;
		if (g<=0.0f) continue ;
		hyperSawRun(o.phase_[note][0],p.inc_[note][0],g,a,n,first) ;
		hyperSawRun(o.phase_[note][1],p.inc_[note][1],g,b,n,first) ;
		first=false ;
	}
	if (first) {
		for (int k=0;k<n;k++) a[k]=b[k]=0.0f ;
	}
	for (int k=0;k<n;k++) {
		float x=a[k]*p.norm_ ;
		float y=b[k]*p.norm_ ;
		left[k]=x*p.gainL_[0]+y*p.gainL_[1] ;
		right[k]=x*p.gainR_[0]+y*p.gainR_[1] ;
	}
	if (p.subLevel_>0.0f) {
		float st=o.subPhase_ ;
		for (int k=0;k<n;k++) {
			float s=(st<0.5f)?1.0f:-1.0f ;
			s+=synthPolyBlep(st,p.subInc_) ;
			float t2=st+0.5f ;
			if (t2>=1.0f) t2-=1.0f ;
			s-=synthPolyBlep(t2,p.subInc_) ;
			st+=p.subInc_ ;
			if (st>=1.0f) st-=1.0f ;
			s*=p.subLevel_*0.6f ;
			left[k]+=s ;
			right[k]+=s ;
		}
		o.subPhase_=st ;
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
