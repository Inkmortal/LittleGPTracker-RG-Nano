// Synth engines: FM4, HYPER and WAV (see SynthEngines.h).
//
// The FM4 operator scheme (32-bit phase accumulators, per-block linear gain
// ramps, feedback from the average of the last two outputs) is adapted from
// the Music Synthesizer for Android "MSFA" engine used by Dexed:
//   Copyright 2012 Google Inc.
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//       http://www.apache.org/licenses/LICENSE-2.0
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// The algorithms, shapes and parameters follow the Dirtywave M8 manual.

#include "SynthEngines.h"
#include <math.h>

float synthSineTable[SYNTH_SINE_SIZE+1] ;
float wavToneNoise[WAV_TONENOISE_STEPS] ;
static bool enginesReady=false ;

void SynthEnginesInit() {
	if (enginesReady) return ;
	for (int i=0;i<=SYNTH_SINE_SIZE;i++) {
		synthSineTable[i]=(float)sin(6.28318530717959*i/SYNTH_SINE_SIZE) ;
	}
	// A 7-bit LFSR run: the "periodic noise" of 8-bit consoles
	unsigned int lfsr=0x5A ;
	for (int i=0;i<WAV_TONENOISE_STEPS;i++) {
		unsigned int bit=((lfsr>>0)^(lfsr>>6))&1u ;
		lfsr=((lfsr>>1)|(bit<<14))&0x7FFFu ;
		wavToneNoise[i]=(lfsr&1u)?1.0f:-1.0f ;
	}
	enginesReady=true ;
}

/***************************************************************
 FM4
 ***************************************************************/

// The M8's twelve routings. mods_: bit 1 = B, 2 = C, 3 = D.
const Fm4Algo fm4Algos[FM4_ALGO_COUNT]={
	{{0x2,0x4,0x8,0x0},0x8,{0,0,0,0},{0,1,2,3},"A>B>C>D"},
	{{0x4,0x4,0x8,0x0},0x8,{0,2,1,1},{0,0,1,2},"[A+B]>C>D"},
	{{0x2,0x8,0x8,0x0},0x8,{0,0,2,1},{0,1,1,2},"[A>B+C]>D"},
	{{0x6,0x8,0x8,0x0},0x8,{1,0,2,1},{0,1,1,2},"[A>B+A>C]>D"},
	{{0x8,0x8,0x8,0x0},0x8,{0,2,4,2},{0,0,0,1},"[A+B+C]>D"},
	{{0x2,0x4,0x0,0x0},0xC,{0,0,0,2},{0,1,2,2},"[A>B>C]+D"},
	{{0x2,0xC,0x0,0x0},0xC,{1,1,0,2},{0,1,2,2},"[A>B>C]+[A>B>D]"},
	{{0x2,0x0,0x8,0x0},0xA,{0,0,2,2},{0,1,0,1},"[A>B]+[C>D]"},
	{{0xE,0x0,0x0,0x0},0xE,{2,0,2,4},{0,1,1,1},"[A>B]+[A>C]+[A>D]"},
	{{0x6,0x0,0x0,0x0},0xE,{1,0,2,4},{0,1,1,1},"[A>B]+[A>C]+D"},
	{{0x2,0x0,0x0,0x0},0xE,{0,0,2,4},{0,1,1,1},"[A>B]+C+D"},
	{{0x0,0x0,0x0,0x0},0xF,{0,2,4,6},{0,0,0,0},"A+B+C+D"},
} ;

const char *fm4ShapeNames[F4S_LAST]={
	"sin","sw2","sw3","sw4","sw5","sw6","tri","saw","squ","pul","imp","nse"
} ;

const char *fm4AlgoNames[FM4_ALGO_COUNT]={
	"00","01","02","03","04","05","06","07","08","09","0A","0B"
} ;

int Fm4CarrierCount(int algo) {
	if (algo<0 || algo>=FM4_ALGO_COUNT) return 1 ;
	int n=0 ;
	for (int op=0;op<FM4_OPS;op++) {
		if (fm4Algos[algo].carriers_&(1<<op)) n++ ;
	}
	return n>0?n:1 ;
}

void Fm4Start(Fm4Ops &o,unsigned int seed) {
	for (int op=0;op<FM4_OPS;op++) {
		// Every note starts its operators at phase zero (key sync), so an
		// attack sounds the same each time
		o.phase_[op]=0 ;
		o.inc_[op]=0 ;
		o.gain_[op]=0.0f ;
		o.gainStep_[op]=0.0f ;
		o.env_[op]=0.0f ;
		o.stage_[op]=0 ;
		o.fb_[op][0]=o.fb_[op][1]=0.0f ;
		o.noiseHold_[op]=0.0f ;
		o.noiseSeg_[op]=0xFF ;
	}
	o.noise_=seed|1u ;
}

void Fm4Block(Fm4Ops &o,const Fm4Params &p,float baseInc,int blockLength) {
	float invLength=1.0f/(float)(blockLength>0?blockLength:1) ;
	for (int op=0;op<FM4_OPS;op++) {
		float inc=baseInc*p.ratio_[op] ;
		if (inc>0.45f) inc=0.45f ;
		if (inc<0.0f) inc=0.0f ;
		o.inc_[op]=(unsigned int)(inc*4294967296.0f) ;

		float env=o.env_[op] ;
		if (o.stage_[op]==0) {
			env+=p.attackStep_[op] ;
			if (env>=1.0f) {
				env=1.0f ;
				o.stage_[op]=1 ;
			}
		} else {
			env=p.sustain_[op]+(env-p.sustain_[op])*p.decayCoef_[op] ;
		}
		o.env_[op]=env ;
		float target=p.level_[op]*env ;
		o.gainStep_[op]=(target-o.gain_[op])*invLength ;
	}
}

/***************************************************************
 HYPER
 ***************************************************************/

const char *hyperChordNames[HYPER_CHORD_COUNT]={
	"custom","unison","octaves","5th","major","minor","sus2","sus4",
	"maj7","min7","dom7","maj9","min9","add9","min11","quartal"
} ;

// First three notes, then the second three ("shift" fades between them)
const signed char hyperChords[HYPER_CHORD_COUNT][HYPER_NOTES]={
	{0,4,7,12,16,19},
	{0,0,0,12,12,12},
	{0,12,-12,0,12,24},
	{0,7,12,0,7,19},
	{0,4,7,12,16,19},
	{0,3,7,12,15,19},
	{0,2,7,12,14,19},
	{0,5,7,12,17,19},
	{0,4,7,11,16,19},
	{0,3,7,10,15,19},
	{0,4,7,10,16,19},
	{0,4,7,11,14,19},
	{0,3,7,10,14,19},
	{0,4,7,14,16,19},
	{0,3,7,10,14,17},
	{0,5,10,15,20,24},
} ;

void HyperStart(HyperOsc &o,unsigned int seed) {
	// Free-running saws at scattered phases: the pairs never start in step,
	// which is what makes the swarm sound wide from the first moment
	unsigned int s=seed|1u ;
	for (int n=0;n<HYPER_NOTES;n++) {
		for (int k=0;k<2;k++) {
			o.phase_[n][k]=(float)(synthXorshift(s)&0xFFFF)/65536.0f ;
		}
	}
	o.subPhase_=0.0f ;
}

float HyperDetuneCents(int swarm,int note) {
	float x=swarm/255.0f ;
	// Up to 60 cents apart; each note a little different so the pairs
	// beat at different speeds instead of pulsing together
	return 60.0f*x*x*(0.76f+0.08f*note) ;
}

void HyperShiftGains(int shift,float &first,float &second) {
	float a=(shift/255.0f)*1.5707963f ;
	first=(float)cos(a) ;
	second=(float)sin(a) ;
	if (first<0.0005f) first=0.0f ;
	if (second<0.0005f) second=0.0f ;
}

void HyperSub(int sub,int &octaves,float &level) {
	if (sub<=0) {
		octaves=2 ;
		level=0.0f ;
	} else if (sub<0x80) {
		octaves=2 ;
		level=sub/127.0f ;
	} else {
		octaves=1 ;
		level=(sub-0x7F)/128.0f ;
	}
}

/***************************************************************
 WAV
 ***************************************************************/

const char *wavShapeNames[WVS_LAST]={
	"pulse12","pulse25","pulse50","pulse75","saw","triangle","sine","tonenoise","noise"
} ;

const char *wavLimitNames[WVL_LAST]={
	"soft","clip","sin","fold","wrap"
} ;

void WavStart(WavOsc &o,unsigned int seed) {
	o.phase_=0.0f ;
	o.lfsr_=(seed&0x7FFFu)|1u ;
	o.noiseClock_=0.0f ;
	o.noiseValue_=1.0f ;
}

// 00 = 4 steps per cycle (crunchy) .. FF = 256 steps (smooth)
int WavStepsFromSize(int size) {
	if (size<0) size=0 ;
	if (size>255) size=255 ;
	return (int)(4.0*pow(2.0,size/255.0*6.0)+0.5) ;
}

// 00 = the shape once per cycle .. FF = 16 times
float WavMultFromParam(int mult) {
	if (mult<0) mult=0 ;
	if (mult>255) mult=255 ;
	return 1.0f+mult/255.0f*15.0f ;
}

void WavSetup(WavParams &p,int shape,int size,int mult,int warp,int mirror) {
	p.shape_=(shape>=0 && shape<WVS_LAST)?shape:WVS_SAW ;
	p.steps_=(float)WavStepsFromSize(size) ;
	p.invSteps_=1.0f/p.steps_ ;
	p.mult_=WavMultFromParam(mult) ;
	if (warp<0) warp=0 ;
	if (warp>255) warp=255 ;
	p.warpEnd_=1.0f-warp/255.0f*0.9f ;
	p.invWarpEnd_=1.0f/p.warpEnd_ ;
	if (mirror<0) mirror=0 ;
	if (mirror>255) mirror=255 ;
	float m=0.5f+(mirror-128)/127.0f*0.48f ;
	if (m<0.02f) m=0.02f ;
	if (m>0.98f) m=0.98f ;
	p.mirror_=m ;
	p.invMirrorA_=1.0f/m ;
	p.invMirrorB_=1.0f/(1.0f-m) ;
}
