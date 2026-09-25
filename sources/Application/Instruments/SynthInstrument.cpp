#include "SynthInstrument.h"
#include "CommandList.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Model/Table.h"
#include "Services/Audio/Audio.h"
#include "System/Console/Trace.h"
#include "Application/Mixer/SendFX.h"
#include "Application/Player/Player.h"
#include "Application/Model/Project.h"
#include "Application/Model/Scale.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

// Pan law shared with SampleInstrument (SampleInstrumentDatas.h)
extern fixed panlaw[] ;

#define SYNTH_PI 3.14159265358979f
#define SYNTH_TWO_PI 6.28318530717959f
#define SYNTH_BLOCK 16
#define SYNTH_KRATE 100
#define SYNTH_FADE_SECONDS 0.003f
#define SYNTH_SILENCE 0.00008f
// Release used when the transport stops (-40 dB in this time)
#define SYNTH_QUICK_RELEASE_SECONDS 0.08f

enum SynthStage {
	SS_OFF=0,
	SS_ATTACK,
	SS_DECAY,
	SS_RELEASE,
	SS_FADE
} ;

static char *synthWaveNames[SW_LAST]={
	"sine","triangle","saw","pulse","supersaw","noise","metal"
} ;

static char *synthFilterNames[SFT_LAST]={
	"lowpass","highpass","bandpass","off"
} ;

static char *synthLfoDestNames[SLD_LAST]={
	"pitch","cutoff","volume","shape"
} ;

static char *synthFmRatioNames[SYNTH_FM_RATIO_COUNT]={
	"0.25","0.5","1","1.5","2","2.5","3","3.5",
	"4","5","6","7","8","9","11","14"
} ;

static const float synthFmRatios[SYNTH_FM_RATIO_COUNT]={
	0.25f,0.5f,1.0f,1.5f,2.0f,2.5f,3.0f,3.5f,
	4.0f,5.0f,6.0f,7.0f,8.0f,9.0f,11.0f,14.0f
} ;

static char *synthChordNames[SYNTH_CHORD_COUNT]={
	"off","5th","octave","major","minor","sus2","sus4","maj7","min7","dom7"
} ;

// Semitone offsets above the root, terminated by -1
static const int synthChordNotes[SYNTH_CHORD_COUNT][SYNTH_MAX_PARTIALS]={
	{0,-1,-1,-1,-1},
	{0,7,-1,-1,-1},
	{0,12,-1,-1,-1},
	{0,4,7,-1,-1},
	{0,3,7,-1,-1},
	{0,2,7,-1,-1},
	{0,5,7,-1,-1},
	{0,4,7,11,-1},
	{0,3,7,10,-1},
	{0,4,7,10,-1}
} ;

// 808-style metallic oscillator ratios
static const float synthMetalRatios[6]={
	1.0f,1.4827f,1.8003f,2.5461f,2.6303f,3.8967f
} ;

/***************************************************************
 Presets
 ***************************************************************/

struct SynthPresetValue {
	FourCC id_ ;
	int value_ ;
} ;

#define SYNTH_PRESET_VALUES 48

struct SynthPreset {
	const char *name_ ;
	int engine_ ;
	SynthPresetValue values_[SYNTH_PRESET_VALUES] ;
} ;

#define PV(a,b) {a,b}
#define PEND {0,0}
#define OP(op,shape,ratio,level,fb,a,d,s) \
	PV(FM4_ID(op,FM4P_SHAPE),shape),PV(FM4_ID(op,FM4P_RATIO),ratio), \
	PV(FM4_ID(op,FM4P_LEVEL),level),PV(FM4_ID(op,FM4P_FEEDBACK),fb), \
	PV(FM4_ID(op,FM4P_ATTACK),a),PV(FM4_ID(op,FM4P_DECAY),d), \
	PV(FM4_ID(op,FM4P_SUSTAIN),s)

// Every preset starts from INIT, then applies its own values.
static const SynthPreset synthPresets[]={
	{"init",SE_SYNTH,{
		PV(SYP_WAVE,SW_PULSE),PV(SYP_SHAPE,0),PV(SYP_SUB,0),PV(SYP_NOISE,0),
		PV(SYP_FMAMT,0),PV(SYP_FMRATIO,2),PV(SYP_CHORD,0),PV(SYP_TUNE,0),PV(SYP_FINE,0),
		PV(SYP_ATTACK,0),PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0x80),PV(SYP_RELEASE,0x70),
		PV(SYP_PITCHENV,0),PV(SYP_PITCHDEC,0x80),PV(SYP_GLIDE,0),
		PV(SYP_FILTTYPE,SFT_LOWPASS),PV(SYP_CUTOFF,0xC0),PV(SYP_RESO,0x20),
		PV(SYP_ENVAMT,0x30),PV(SYP_ENVDEC,0x98),PV(SYP_DRIVE,0),
		PV(SYP_LFODEST,SLD_PITCH),PV(SYP_LFORATE,0xB0)}},
	{"kick",SE_SYNTH,{
		PV(SYP_WAVE,SW_SINE),PV(SYP_SHAPE,0x10),PV(SYP_TUNE,-27),
		PV(SYP_DECAY,0xA8),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x60),
		PV(SYP_PITCHENV,0x9C),PV(SYP_PITCHDEC,0x70),
		PV(SYP_FILTTYPE,SFT_OFF),PV(SYP_ENVAMT,0),PV(SYP_DRIVE,0x50),
		PV(SYP_VOLUME,0xB4),PEND}},
	{"snare",SE_SYNTH,{
		PV(SYP_WAVE,SW_TRIANGLE),PV(SYP_TUNE,-5),PV(SYP_NOISE,0xB0),
		PV(SYP_DECAY,0x98),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x70),
		PV(SYP_PITCHENV,0x28),PV(SYP_PITCHDEC,0x68),
		PV(SYP_CUTOFF,0xE8),PV(SYP_RESO,0),PV(SYP_ENVAMT,0),PV(SYP_DRIVE,0x30),
		PV(SYP_VOLUME,0xC8),PEND}},
	{"clap",SE_SYNTH,{
		PV(SYP_WAVE,SW_NOISE),
		PV(SYP_DECAY,0x94),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x70),
		PV(SYP_FILTTYPE,SFT_BANDPASS),PV(SYP_CUTOFF,0xB4),PV(SYP_RESO,0x60),
		PV(SYP_ENVAMT,0),PV(SYP_DRIVE,0x20),PV(SYP_VOLUME,0xF0),PEND}},
	{"hat",SE_SYNTH,{
		PV(SYP_WAVE,SW_METAL),PV(SYP_SHAPE,0x40),PV(SYP_NOISE,0x70),
		PV(SYP_DECAY,0x70),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x50),
		PV(SYP_FILTTYPE,SFT_HIGHPASS),PV(SYP_CUTOFF,0xB8),PV(SYP_RESO,0x30),
		PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0xD8),PV(SYP_PAN,0x90),PEND}},
	{"openhat",SE_SYNTH,{
		PV(SYP_WAVE,SW_METAL),PV(SYP_SHAPE,0x40),PV(SYP_NOISE,0x70),
		PV(SYP_DECAY,0xA8),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x90),
		PV(SYP_FILTTYPE,SFT_HIGHPASS),PV(SYP_CUTOFF,0xB4),PV(SYP_RESO,0x30),
		PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0xC0),PV(SYP_PAN,0x90),PEND}},
	{"tom",SE_SYNTH,{
		PV(SYP_WAVE,SW_SINE),PV(SYP_SHAPE,0x20),PV(SYP_TUNE,-17),PV(SYP_NOISE,0x10),
		PV(SYP_DECAY,0xA8),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x70),
		PV(SYP_PITCHENV,0x40),PV(SYP_PITCHDEC,0x8C),
		PV(SYP_CUTOFF,0xD0),PV(SYP_RESO,0),PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0xB0),PEND}},
	{"perc",SE_SYNTH,{
		PV(SYP_WAVE,SW_TRIANGLE),PV(SYP_TUNE,19),
		PV(SYP_DECAY,0x68),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x50),
		PV(SYP_PITCHENV,0x20),PV(SYP_PITCHDEC,0x40),
		PV(SYP_FILTTYPE,SFT_HIGHPASS),PV(SYP_CUTOFF,0x70),PV(SYP_RESO,0x10),
		PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0x90),PEND}},
	{"bass",SE_SYNTH,{
		PV(SYP_WAVE,SW_SAW),PV(SYP_SUB,0x70),PV(SYP_TUNE,-24),
		PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0x90),PV(SYP_RELEASE,0x60),
		PV(SYP_CUTOFF,0x58),PV(SYP_RESO,0x40),PV(SYP_ENVAMT,0x88),PV(SYP_ENVDEC,0x98),
		PV(SYP_DRIVE,0x30),PV(SYP_VOLUME,0x70),PEND}},
	{"subbass",SE_SYNTH,{
		PV(SYP_WAVE,SW_SINE),PV(SYP_SHAPE,0x08),PV(SYP_TUNE,-24),
		PV(SYP_ATTACK,0x10),PV(SYP_DECAY,0xC0),PV(SYP_SUSTAIN,0xD0),PV(SYP_RELEASE,0x70),
		PV(SYP_FILTTYPE,SFT_OFF),PV(SYP_ENVAMT,0),PV(SYP_DRIVE,0x28),PV(SYP_VOLUME,0x58),PEND}},
	{"acid",SE_SYNTH,{
		PV(SYP_WAVE,SW_SAW),PV(SYP_TUNE,-24),
		PV(SYP_DECAY,0xA0),PV(SYP_SUSTAIN,0x60),PV(SYP_RELEASE,0x60),PV(SYP_GLIDE,0x60),
		PV(SYP_CUTOFF,0x48),PV(SYP_RESO,0xD0),PV(SYP_ENVAMT,0xA8),PV(SYP_ENVDEC,0x88),
		PV(SYP_DRIVE,0x70),PV(SYP_VOLUME,0x48),PEND}},
	{"pluck",SE_SYNTH,{
		PV(SYP_WAVE,SW_SAW),
		PV(SYP_DECAY,0xA8),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x90),
		PV(SYP_CUTOFF,0x40),PV(SYP_RESO,0x50),PV(SYP_ENVAMT,0xB8),PV(SYP_ENVDEC,0x8C),
		PV(SYP_VOLUME,0xE0),PEND}},
	{"lead",SE_SYNTH,{
		PV(SYP_WAVE,SW_PULSE),PV(SYP_SHAPE,0x40),
		PV(SYP_ATTACK,0x18),PV(SYP_DECAY,0xA0),PV(SYP_SUSTAIN,0xA8),PV(SYP_RELEASE,0x88),
		PV(SYP_GLIDE,0x30),
		PV(SYP_CUTOFF,0xB8),PV(SYP_RESO,0x28),PV(SYP_ENVAMT,0x38),PV(SYP_ENVDEC,0x98),
		PV(SYP_LFODEST,SLD_PITCH),PV(SYP_LFORATE,0xB0),PV(SYP_LFOAMT,0x14),
		PV(SYP_VOLUME,0x50),PEND}},
	{"pad",SE_SYNTH,{
		PV(SYP_WAVE,SW_SUPERSAW),PV(SYP_SHAPE,0x70),
		PV(SYP_ATTACK,0xB8),PV(SYP_DECAY,0xC8),PV(SYP_SUSTAIN,0xD0),PV(SYP_RELEASE,0xC8),
		PV(SYP_CUTOFF,0x98),PV(SYP_RESO,0x20),PV(SYP_ENVAMT,0x20),PV(SYP_ENVDEC,0xC0),
		PV(SYP_LFODEST,SLD_CUTOFF),PV(SYP_LFORATE,0x48),PV(SYP_LFOAMT,0x28),
		PV(SYP_VOLUME,0x50),PEND}},
	{"keys",SE_SYNTH,{
		PV(SYP_WAVE,SW_SINE),PV(SYP_FMAMT,0x40),PV(SYP_FMRATIO,2),
		PV(SYP_ATTACK,0x08),PV(SYP_DECAY,0xC4),PV(SYP_SUSTAIN,0x50),PV(SYP_RELEASE,0x98),
		PV(SYP_CUTOFF,0xD8),PV(SYP_RESO,0),PV(SYP_ENVAMT,0x70),PV(SYP_ENVDEC,0xA8),
		PV(SYP_LFODEST,SLD_VOLUME),PV(SYP_LFORATE,0xA0),PV(SYP_LFOAMT,0x10),
		PV(SYP_VOLUME,0x7C),PEND}},
	{"bell",SE_SYNTH,{
		PV(SYP_WAVE,SW_SINE),PV(SYP_FMAMT,0x68),PV(SYP_FMRATIO,7),
		PV(SYP_DECAY,0xD4),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0xC8),
		PV(SYP_FILTTYPE,SFT_OFF),PV(SYP_ENVAMT,0x90),PV(SYP_ENVDEC,0xB8),
		PV(SYP_VOLUME,0x58),PEND}},
	{"chip",SE_SYNTH,{
		PV(SYP_WAVE,SW_PULSE),PV(SYP_SHAPE,0x90),
		PV(SYP_DECAY,0x90),PV(SYP_SUSTAIN,0x90),PV(SYP_RELEASE,0x40),
		PV(SYP_FILTTYPE,SFT_OFF),PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0x48),PEND}},

	// FM4. OP(operator, shape, ratio x100, level, feedback, attack, decay,
	// sustain). The first entry of each engine is its starting point: every
	// other preset of that engine is applied on top of it.
	{"fm init",SE_FM4,{
		PV(FM4P_ALGO,0),
		OP(0,F4S_SIN,100,0,0,0,0x80,0xFF),OP(1,F4S_SIN,100,0,0,0,0x80,0xFF),
		OP(2,F4S_SIN,100,0x60,0,0,0x80,0xFF),OP(3,F4S_SIN,100,0xFF,0,0,0x80,0xFF),
		PV(SYP_FILTTYPE,SFT_OFF),PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0x80),PEND}},
	{"epiano",SE_FM4,{
		PV(FM4P_ALGO,7),
		OP(0,F4S_SIN,1400,0x60,0,0,0x6C,0),OP(1,F4S_SIN,100,0xD8,0,0,0xC8,0x50),
		OP(2,F4S_SIN,100,0x74,0x30,0,0xB0,0x28),OP(3,F4S_SIN,100,0xFF,0,0,0xD0,0x60),
		PV(SYP_DECAY,0xD8),PV(SYP_SUSTAIN,0x50),PV(SYP_RELEASE,0x98),
		PV(SYP_LFODEST,SLD_VOLUME),PV(SYP_LFORATE,0xA0),PV(SYP_LFOAMT,0x10),
		PV(SYP_CHORUS,0x40),PV(SYP_VOLUME,0x78),PEND}},
	{"fm bell",SE_FM4,{
		PV(FM4P_ALGO,7),
		OP(0,F4S_SIN,350,0x98,0,0,0xD0,0),OP(1,F4S_SIN,100,0xFF,0,0,0xE0,0),
		OP(2,F4S_SIN,141,0x78,0,0,0xC8,0),OP(3,F4S_SIN,100,0xB0,0,0,0xD8,0),
		PV(SYP_DECAY,0xE4),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0xD0),
		PV(SYP_REVERB,0x60),PV(SYP_VOLUME,0x70),PEND}},
	{"tubular",SE_FM4,{
		PV(FM4P_ALGO,5),
		OP(0,F4S_SIN,700,0x48,0,0,0xB0,0),OP(1,F4S_SIN,350,0x88,0,0,0xD8,0x20),
		OP(2,F4S_SIN,100,0xFF,0,0,0xE8,0),OP(3,F4S_SIN,276,0x70,0,0,0xD8,0),
		PV(SYP_DECAY,0xEC),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0xD8),
		PV(SYP_REVERB,0x70),PV(SYP_VOLUME,0x70),PEND}},
	{"fm bass",SE_FM4,{
		PV(FM4P_ALGO,7),PV(SYP_TUNE,-24),
		OP(0,F4S_SIN,100,0x98,0x90,0,0x98,0x30),OP(1,F4S_SIN,100,0xFF,0,0,0xC0,0xC0),
		OP(2,F4S_SIN,100,0x40,0,0,0xA0,0x40),OP(3,F4S_SIN,50,0xB0,0,0,0xC0,0xC0),
		PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0xA0),PV(SYP_RELEASE,0x60),
		PV(SYP_VOLUME,0x70),PEND}},
	{"slap bass",SE_FM4,{
		PV(FM4P_ALGO,3),PV(SYP_TUNE,-24),
		OP(0,F4S_SIN,700,0x50,0,0,0x50,0),OP(1,F4S_SIN,100,0x88,0x40,0,0x90,0x18),
		OP(2,F4S_SIN,300,0x70,0,0,0x70,0),OP(3,F4S_SIN,100,0xFF,0,0,0xC0,0x90),
		PV(SYP_DECAY,0xB8),PV(SYP_SUSTAIN,0x58),PV(SYP_RELEASE,0x60),
		PV(SYP_VOLUME,0x70),PEND}},
	{"brass",SE_FM4,{
		PV(FM4P_ALGO,2),
		OP(0,F4S_SIN,100,0x40,0,0x60,0xB0,0x80),OP(1,F4S_SIN,100,0x70,0,0x60,0xB0,0xA0),
		OP(2,F4S_SIN,100,0x90,0x70,0x68,0xB0,0xB0),OP(3,F4S_SIN,100,0xFF,0,0x40,0x80,0xFF),
		PV(SYP_ATTACK,0x48),PV(SYP_DECAY,0xC0),PV(SYP_SUSTAIN,0xC8),PV(SYP_RELEASE,0x90),
		PV(SYP_FILTTYPE,SFT_LOWPASS),PV(SYP_CUTOFF,0xD0),PV(SYP_RESO,0),
		PV(SYP_LFODEST,SLD_PITCH),PV(SYP_LFORATE,0xB0),PV(SYP_LFOAMT,0x0C),
		PV(SYP_REVERB,0x40),PV(SYP_VOLUME,0x68),PEND}},
	{"organ",SE_FM4,{
		PV(FM4P_ALGO,11),
		OP(0,F4S_SIN,50,0xB0,0,0,0x80,0xFF),OP(1,F4S_SIN,100,0xFF,0,0,0x80,0xFF),
		OP(2,F4S_SIN,200,0xC0,0,0,0x80,0xFF),OP(3,F4S_SIN,300,0x98,0,0,0x80,0xFF),
		PV(SYP_ATTACK,0x10),PV(SYP_DECAY,0x80),PV(SYP_SUSTAIN,0xFF),PV(SYP_RELEASE,0x58),
		PV(SYP_LFODEST,SLD_VOLUME),PV(SYP_LFORATE,0xB8),PV(SYP_LFOAMT,0x0C),
		PV(SYP_CHORUS,0x60),PV(SYP_VOLUME,0x80),PEND}},
	{"marimba",SE_FM4,{
		PV(FM4P_ALGO,10),
		OP(0,F4S_SIN,300,0x60,0,0,0x70,0),OP(1,F4S_SIN,100,0xFF,0,0,0xB8,0),
		OP(2,F4S_SIN,400,0x60,0,0,0x98,0),OP(3,F4S_SIN,1000,0x30,0,0,0x70,0),
		PV(SYP_DECAY,0xBC),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0xB0),
		PV(SYP_REVERB,0x40),PV(SYP_VOLUME,0xA0),PEND}},
	{"fm pluck",SE_FM4,{
		PV(FM4P_ALGO,0),
		OP(2,F4S_SIN,200,0xA8,0x20,0,0x90,0),OP(3,F4S_SIN,100,0xFF,0,0,0xB8,0),
		PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x90),
		PV(SYP_DELAY,0x50),PV(SYP_VOLUME,0x90),PEND}},
	{"glass",SE_FM4,{
		PV(FM4P_ALGO,8),
		OP(0,F4S_SIN,100,0x58,0,0x80,0xC8,0x40),OP(1,F4S_SIN,100,0xFF,0,0,0xD0,0xA0),
		OP(2,F4S_SIN,300,0x90,0,0,0xC8,0x50),OP(3,F4S_SIN,500,0x68,0,0,0xB8,0x30),
		PV(SYP_ATTACK,0x30),PV(SYP_DECAY,0xD0),PV(SYP_SUSTAIN,0x70),PV(SYP_RELEASE,0xC8),
		PV(SYP_REVERB,0x90),PV(SYP_CHORUS,0x40),PV(SYP_VOLUME,0x70),PEND}},
	{"fm lead",SE_FM4,{
		PV(FM4P_ALGO,1),
		OP(0,F4S_SIN,100,0x60,0xB0,0,0xA0,0x80),OP(1,F4S_SIN,200,0x48,0,0,0x80,0xFF),
		OP(2,F4S_SIN,100,0x80,0,0,0xA0,0xB0),OP(3,F4S_SIN,100,0xFF,0,0,0x80,0xFF),
		PV(SYP_ATTACK,0x10),PV(SYP_DECAY,0xA0),PV(SYP_SUSTAIN,0xC0),PV(SYP_RELEASE,0x80),
		PV(SYP_GLIDE,0x30),PV(SYP_FILTTYPE,SFT_LOWPASS),PV(SYP_CUTOFF,0xC8),PV(SYP_RESO,0x10),
		PV(SYP_LFODEST,SLD_PITCH),PV(SYP_LFORATE,0xB0),PV(SYP_LFOAMT,0x14),
		PV(SYP_DELAY,0x40),PV(SYP_VOLUME,0x58),PEND}},
	{"clav",SE_FM4,{
		PV(FM4P_ALGO,7),
		OP(0,F4S_SIN,100,0xA0,0x80,0,0x88,0x28),OP(1,F4S_SIN,100,0xFF,0,0,0xB0,0x50),
		OP(2,F4S_SIN,700,0x50,0,0,0x60,0),OP(3,F4S_SIN,300,0x70,0,0,0x98,0),
		PV(SYP_DECAY,0xA8),PV(SYP_SUSTAIN,0x40),PV(SYP_RELEASE,0x60),
		PV(SYP_FILTTYPE,SFT_HIGHPASS),PV(SYP_CUTOFF,0x50),PV(SYP_RESO,0),
		PV(SYP_VOLUME,0x68),PEND}},

	// HYPER. Chords: 1 unison 2 octaves 3 5th 4 major 5 minor 6 sus2 7 sus4
	// 8 maj7 9 min7 10 dom7 11 maj9 12 min9 13 add9 14 min11 15 quartal
	{"hyper init",SE_HYPER,{
		PV(HYP_CHORD,4),PV(HYP_SHIFT,0),PV(HYP_SWARM,0x60),PV(HYP_WIDTH,0x80),
		PV(HYP_SUB,0),PV(SYP_CUTOFF,0xC0),PV(SYP_RESO,0x10),PV(SYP_ENVAMT,0),
		PV(SYP_VOLUME,0x70),PEND}},
	{"hyper pad",SE_HYPER,{
		PV(HYP_CHORD,12),PV(HYP_SHIFT,0x60),PV(HYP_SWARM,0x70),PV(HYP_WIDTH,0xE0),PV(HYP_SUB,0x30),
		PV(SYP_ATTACK,0xB8),PV(SYP_DECAY,0xC8),PV(SYP_SUSTAIN,0xD0),PV(SYP_RELEASE,0xC8),
		PV(SYP_CUTOFF,0x98),PV(SYP_RESO,0x20),PV(SYP_ENVAMT,0x20),PV(SYP_ENVDEC,0xC0),
		PV(SYP_LFODEST,SLD_CUTOFF),PV(SYP_LFORATE,0x48),PV(SYP_LFOAMT,0x28),
		PV(SYP_REVERB,0xA0),PV(SYP_VOLUME,0x60),PEND}},
	{"trance lead",SE_HYPER,{
		PV(HYP_CHORD,1),PV(HYP_SHIFT,0x40),PV(HYP_SWARM,0xA0),PV(HYP_WIDTH,0xC0),
		PV(SYP_ATTACK,0x08),PV(SYP_DECAY,0xA0),PV(SYP_SUSTAIN,0xB0),PV(SYP_RELEASE,0x90),
		PV(SYP_CUTOFF,0xC8),PV(SYP_RESO,0x30),PV(SYP_ENVAMT,0x30),PV(SYP_ENVDEC,0x98),
		PV(SYP_DELAY,0x50),PV(SYP_REVERB,0x50),PV(SYP_VOLUME,0x58),PEND}},
	{"hoover",SE_HYPER,{
		PV(HYP_CHORD,2),PV(HYP_SHIFT,0x20),PV(HYP_SWARM,0xE0),PV(HYP_WIDTH,0xFF),PV(HYP_SUB,0xA0),
		PV(SYP_PITCHENV,0x18),PV(SYP_PITCHDEC,0x98),PV(SYP_GLIDE,0x50),
		PV(SYP_ATTACK,0x20),PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0xC0),PV(SYP_RELEASE,0x90),
		PV(SYP_CUTOFF,0xD0),PV(SYP_RESO,0x18),PV(SYP_ENVAMT,0x10),
		PV(SYP_REVERB,0x40),PV(SYP_VOLUME,0x58),PEND}},
	{"strings",SE_HYPER,{
		PV(HYP_CHORD,3),PV(HYP_SHIFT,0x80),PV(HYP_SWARM,0x50),PV(HYP_WIDTH,0xD0),
		PV(SYP_ATTACK,0xA8),PV(SYP_DECAY,0xC0),PV(SYP_SUSTAIN,0xD0),PV(SYP_RELEASE,0xB8),
		PV(SYP_CUTOFF,0x90),PV(SYP_RESO,0x10),PV(SYP_ENVAMT,0),
		PV(SYP_LFODEST,SLD_PITCH),PV(SYP_LFORATE,0xA8),PV(SYP_LFOAMT,0x0C),
		PV(SYP_REVERB,0x80),PV(SYP_VOLUME,0x60),PEND}},
	{"stab",SE_HYPER,{
		PV(HYP_CHORD,9),PV(HYP_SHIFT,0),PV(HYP_SWARM,0x50),PV(HYP_WIDTH,0xA0),
		PV(SYP_DECAY,0xA0),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x80),
		PV(SYP_CUTOFF,0x70),PV(SYP_RESO,0x50),PV(SYP_ENVAMT,0xA0),PV(SYP_ENVDEC,0x90),
		PV(SYP_REVERB,0x60),PV(SYP_DELAY,0x40),PV(SYP_VOLUME,0x68),PEND}},
	{"dream",SE_HYPER,{
		PV(HYP_CHORD,11),PV(HYP_SHIFT,0xA0),PV(HYP_SWARM,0x60),PV(HYP_WIDTH,0xFF),PV(HYP_SUB,0x40),
		PV(SYP_ATTACK,0xC0),PV(SYP_DECAY,0xD0),PV(SYP_SUSTAIN,0xE0),PV(SYP_RELEASE,0xD8),
		PV(SYP_CUTOFF,0xA8),PV(SYP_RESO,0x10),PV(SYP_ENVAMT,0),
		PV(SYP_REVERB,0xC0),PV(SYP_CHORUS,0x60),PV(SYP_VOLUME,0x58),PEND}},
	{"hyper bass",SE_HYPER,{
		PV(HYP_CHORD,1),PV(HYP_SHIFT,0),PV(HYP_SWARM,0x40),PV(HYP_WIDTH,0x40),PV(HYP_SUB,0xC0),
		PV(SYP_TUNE,-24),
		PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0x90),PV(SYP_RELEASE,0x60),
		PV(SYP_CUTOFF,0x60),PV(SYP_RESO,0x40),PV(SYP_ENVAMT,0x80),PV(SYP_ENVDEC,0x98),
		PV(SYP_DRIVE,0x20),PV(SYP_VOLUME,0x68),PEND}},

	// WAV
	{"wav init",SE_WAV,{
		PV(WVP_SHAPE,WVS_SAW),PV(WVP_SIZE,0xFF),PV(WVP_MULT,0),PV(WVP_WARP,0),
		PV(WVP_MIRROR,0x80),PV(WVP_LIMIT,WVL_SOFT),
		PV(SYP_FILTTYPE,SFT_OFF),PV(SYP_ENVAMT,0),PV(SYP_VOLUME,0x60),PEND}},
	{"chip lead",SE_WAV,{
		PV(WVP_SHAPE,WVS_PULSE25),
		PV(SYP_DECAY,0xA0),PV(SYP_SUSTAIN,0xB0),PV(SYP_RELEASE,0x60),PV(SYP_GLIDE,0x20),
		PV(SYP_LFODEST,SLD_PITCH),PV(SYP_LFORATE,0xB4),PV(SYP_LFOAMT,0x14),
		PV(SYP_DELAY,0x40),PV(SYP_VOLUME,0x48),PEND}},
	{"chip bass",SE_WAV,{
		PV(WVP_SHAPE,WVS_TRIANGLE),PV(WVP_SIZE,0x80),PV(SYP_TUNE,-24),
		PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0xFF),PV(SYP_RELEASE,0x40),
		PV(SYP_VOLUME,0x90),PEND}},
	{"pwm pad",SE_WAV,{
		PV(WVP_SHAPE,WVS_PULSE50),
		PV(SYP_ATTACK,0xB0),PV(SYP_DECAY,0xC8),PV(SYP_SUSTAIN,0xD0),PV(SYP_RELEASE,0xC0),
		PV(SYP_FILTTYPE,SFT_LOWPASS),PV(SYP_CUTOFF,0xA0),PV(SYP_RESO,0x18),
		PV(SYP_LFODEST,SLD_SHAPE),PV(SYP_LFORATE,0x60),PV(SYP_LFOAMT,0x70),
		PV(SYP_CHORUS,0x80),PV(SYP_REVERB,0x80),PV(SYP_VOLUME,0x40),PEND}},
	{"sync lead",SE_WAV,{
		PV(WVP_SHAPE,WVS_SAW),PV(WVP_MULT,0x38),PV(WVP_LIMIT,WVL_CLIP),PV(SYP_DRIVE,0x30),
		PV(SYP_ATTACK,0x08),PV(SYP_DECAY,0xA8),PV(SYP_SUSTAIN,0xA0),PV(SYP_RELEASE,0x80),
		PV(SYP_FILTTYPE,SFT_LOWPASS),PV(SYP_CUTOFF,0xC8),PV(SYP_RESO,0x20),
		PV(SYP_GLIDE,0x28),PV(SYP_DELAY,0x40),PV(SYP_VOLUME,0x40),PEND}},
	{"fold bass",SE_WAV,{
		PV(WVP_SHAPE,WVS_SINE),PV(WVP_LIMIT,WVL_FOLD),PV(SYP_DRIVE,0x60),PV(SYP_TUNE,-24),
		PV(SYP_DECAY,0xB0),PV(SYP_SUSTAIN,0x90),PV(SYP_RELEASE,0x60),
		PV(SYP_FILTTYPE,SFT_LOWPASS),PV(SYP_CUTOFF,0x90),PV(SYP_RESO,0x20),
		PV(SYP_ENVAMT,0x60),PV(SYP_ENVDEC,0x98),PV(SYP_VOLUME,0x70),PEND}},
	{"zap",SE_WAV,{
		PV(WVP_SHAPE,WVS_SINE),PV(WVP_WARP,0x80),
		PV(SYP_PITCHENV,0xC0),PV(SYP_PITCHDEC,0x68),
		PV(SYP_DECAY,0x98),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x60),
		PV(SYP_VOLUME,0x80),PEND}},
	{"lofi bell",SE_WAV,{
		PV(WVP_SHAPE,WVS_SINE),PV(WVP_SIZE,0x20),PV(WVP_MULT,0x20),
		PV(SYP_DECAY,0xD0),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0xC0),
		PV(SYP_REVERB,0x70),PV(SYP_VOLUME,0x68),PEND}},
	{"noise hat",SE_WAV,{
		PV(WVP_SHAPE,WVS_NOISE),PV(SYP_TUNE,24),
		PV(SYP_DECAY,0x70),PV(SYP_SUSTAIN,0),PV(SYP_RELEASE,0x50),
		PV(SYP_FILTTYPE,SFT_HIGHPASS),PV(SYP_CUTOFF,0xB0),PV(SYP_RESO,0x20),
		PV(SYP_VOLUME,0xB0),PV(SYP_PAN,0x70),PEND}}
} ;

#define SYNTH_PRESET_COUNT ((int)(sizeof(synthPresets)/sizeof(SynthPreset)))

static char *synthPresetNames[SYNTH_PRESET_COUNT] ;
static bool synthPresetNamesReady=false ;

static char **getPresetNameList() {
	if (!synthPresetNamesReady) {
		for (int i=0;i<SYNTH_PRESET_COUNT;i++) {
			synthPresetNames[i]=(char *)synthPresets[i].name_ ;
		}
		synthPresetNamesReady=true ;
	}
	return synthPresetNames ;
}

int SynthInstrument::GetPresetCount() {
	return SYNTH_PRESET_COUNT ;
}

static const char *synthEngineNames[SE_LAST]={
	"synth","fm4","hyper","wav"
} ;

const char *SynthInstrument::GetEngineName(int engine) {
	if (engine<0 || engine>=SE_LAST) return "" ;
	return synthEngineNames[engine] ;
}

int SynthInstrument::GetPresetEngine(int preset) {
	if (preset<0 || preset>=SYNTH_PRESET_COUNT) return SE_SYNTH ;
	return synthPresets[preset].engine_ ;
}

void SynthInstrument::GetPresetRange(int engine,int &first,int &last) {
	first=-1 ;
	last=-1 ;
	for (int i=0;i<SYNTH_PRESET_COUNT;i++) {
		if (synthPresets[i].engine_!=engine) continue ;
		if (first<0) first=i ;
		last=i ;
	}
	if (first<0) {
		first=last=0 ;
	}
}

// Hooks of SynthHookVariable
enum {
	SYNTH_HOOK_ENGINE=0,
	SYNTH_HOOK_HYPER_CHORD,
	SYNTH_HOOK_HYPER_NOTE
} ;

const char *SynthInstrument::GetPresetName(int index) {
	if (index<0 || index>=SYNTH_PRESET_COUNT) return "" ;
	return synthPresets[index].name_ ;
}

/***************************************************************
 Parameter curves
 ***************************************************************/

// 00=1ms 40=10ms 80=100ms C0=1s FF=10s
float SynthInstrument::TimeFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.001f*(float)pow(10.0,(value/255.0)*4.0) ;
}

// 00=0.05Hz 80=1.6Hz C0=9Hz FF=50Hz
float SynthInstrument::LfoRateFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.05f*(float)pow(10.0,(value/255.0)*3.0) ;
}

// 0..1 -> 20Hz..20kHz, exponential
float SynthInstrument::CutoffHzFromParam(float value) {
	if (value<0.0f) value=0.0f ;
	if (value>1.0f) value=1.0f ;
	return 20.0f*(float)pow(1000.0,(double)value) ;
}

static inline float coefFromTime(float seconds,float sampleRate) {
	// One-pole coefficient reaching ~-40dB after 'seconds'
	float samples=seconds*sampleRate ;
	if (samples<1.0f) samples=1.0f ;
	return (float)exp(-4.6/samples) ;
}

static inline float polyBlep(float t,float dt) {
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

static inline float modClamp01(float x) {
	if (x<0.0f) return 0.0f ;
	if (x>1.0f) return 1.0f ;
	return x ;
}

static inline float wrap01(float x) {
	return x-(float)floor(x) ;
}

static inline float softSat(float x) {
	if (x>3.0f) return 1.0f ;
	if (x<-3.0f) return -1.0f ;
	float x2=x*x ;
	return x*(27.0f+x2)/(27.0f+9.0f*x2) ;
}

// Sine lookup (phase in cycles). Linear interpolation over 2048 points is
// far below audible error and much cheaper than libm sin() on the RG Nano.
// The table is shared with the engines (SynthEngines.cpp).
static void initSineTable() {
	SynthEnginesInit() ;
}

static inline float fastSin(float phase) {
	phase-=(float)floor(phase) ;
	float pos=phase*SYNTH_SINE_SIZE ;
	int i=(int)pos ;
	if (i>=SYNTH_SINE_SIZE) i=SYNTH_SINE_SIZE-1 ;
	float frac=pos-i ;
	return synthSineTable[i]+(synthSineTable[i+1]-synthSineTable[i])*frac ;
}

static inline unsigned int xorshift(unsigned int &state) {
	unsigned int x=state ;
	x^=x<<13 ;
	x^=x>>17 ;
	x^=x<<5 ;
	state=x ;
	return x ;
}

static inline float whiteNoise(unsigned int &state) {
	return ((float)(xorshift(state)&0xFFFF)/32768.0f)-1.0f ;
}

/***************************************************************
 Preset variable
 ***************************************************************/

SynthPresetVariable::SynthPresetVariable(SynthInstrument *owner,const char *name,
                                         FourCC id,char **list,int size,int index)
	:Variable(name,id,list,size,index),owner_(owner) {
}

void SynthPresetVariable::onChange() {
	if (owner_) {
		owner_->ApplyPreset(GetInt()) ;
	}
}

SynthHookVariable::SynthHookVariable(SynthInstrument *owner,int hook,const char *name,
                                     FourCC id,const char *const *list,int size,int index)
	:Variable(name,id,list,size,index),owner_(owner),hook_(hook) {
}

SynthHookVariable::SynthHookVariable(SynthInstrument *owner,int hook,const char *name,
                                     FourCC id,int value)
	:Variable(name,id,value,0),owner_(owner),hook_(hook) {
}

void SynthHookVariable::onChange() {
	if (owner_) {
		owner_->OnHook(hook_) ;
	}
}

/***************************************************************
 Engine knobs
 ***************************************************************/

static const char fm4ParamChars[FM4_PARAM_COUNT]={
	FM4P_SHAPE,FM4P_RATIO,FM4P_LEVEL,FM4P_FEEDBACK,FM4P_ATTACK,FM4P_DECAY,FM4P_SUSTAIN
} ;

static const char *fm4ParamNames[FM4_PARAM_COUNT]={
	"shape","ratio","level","feedback","attack","decay","sustain"
} ;

// Saved names: "fm a shape", "fm b ratio", ... (names must be unique per
// instrument: projects are saved and restored by name)
static char fm4VarNames[FM4_OPS][FM4_PARAM_COUNT][20] ;
static char hyperNoteNames[HYPER_NOTES][16] ;
static bool engineNamesReady=false ;

static void initEngineNames() {
	if (engineNamesReady) return ;
	for (int op=0;op<FM4_OPS;op++) {
		for (int k=0;k<FM4_PARAM_COUNT;k++) {
			sprintf(fm4VarNames[op][k],"fm %c %s",'a'+op,fm4ParamNames[k]) ;
		}
	}
	for (int n=0;n<HYPER_NOTES;n++) {
		sprintf(hyperNoteNames[n],"hyper note %d",n+1) ;
	}
	engineNamesReady=true ;
}

int SynthInstrument::GetEngine() {
	int e=engine_->GetInt() ;
	if (e<0 || e>=SE_LAST) e=SE_SYNTH ;
	return e ;
}

void SynthInstrument::OnHook(int hook) {
	switch(hook) {
		case SYNTH_HOOK_ENGINE: {
			if (applyingPreset_) return ;
			// A new engine starts from its own first preset; a restored or
			// undone project already has the matching preset in place
			int e=GetEngine() ;
			if (GetPresetEngine(preset_->GetInt())!=e) {
				int first,last ;
				GetPresetRange(e,first,last) ;
				preset_->SetInt(first) ;
			}
			break ;
		}
		case SYNTH_HOOK_HYPER_CHORD: {
			int c=hyperChord_->GetInt() ;
			if (c>0 && c<HYPER_CHORD_COUNT) {
				for (int n=0;n<HYPER_NOTES;n++) {
					hyperNote_[n]->SetInt(hyperChords[c][n],false) ;
				}
			}
			break ;
		}
		case SYNTH_HOOK_HYPER_NOTE:
			syncHyperChordName() ;
			break ;
		default:
			break ;
	}
}

// The chord knob names the six notes when they match a known chord
void SynthInstrument::syncHyperChordName() {
	int match=0 ;
	for (int c=1;c<HYPER_CHORD_COUNT && !match;c++) {
		bool same=true ;
		for (int n=0;n<HYPER_NOTES;n++) {
			if (hyperNote_[n]->GetInt()!=hyperChords[c][n]) {
				same=false ;
				break ;
			}
		}
		if (same) match=c ;
	}
	if (hyperChord_->GetInt()!=match) {
		hyperChord_->SetInt(match,false) ;
	}
}

void SynthInstrument::resetEngineVariables() {
	for (unsigned int i=0;i<engineVars_.size();i++) {
		engineVars_[i]->Reset() ;
	}
}

/***************************************************************
 Instrument
 ***************************************************************/

SynthInstrument::SynthInstrument() {
	initSineTable() ;
	applyingPreset_=true ;

	initEngineNames() ;
	preset_=new SynthPresetVariable(this,"preset",SYP_PRESET,getPresetNameList(),SYNTH_PRESET_COUNT,0) ;
	Insert(preset_) ;
	// Saved right after the preset: a restored song applies the preset
	// (which picks its engine), then the engine agrees with it
	engine_=new SynthHookVariable(this,SYNTH_HOOK_ENGINE,"engine",SYP_ENGINE,
	                              synthEngineNames,SE_LAST,SE_SYNTH) ;
	Insert(engine_) ;
	wave_=new Variable("wave",SYP_WAVE,synthWaveNames,SW_LAST,SW_PULSE) ;
	Insert(wave_) ;
	shape_=new Variable("shape",SYP_SHAPE,0) ;
	Insert(shape_) ;
	sub_=new Variable("sub",SYP_SUB,0) ;
	Insert(sub_) ;
	noise_=new Variable("noise",SYP_NOISE,0) ;
	Insert(noise_) ;
	fmAmt_=new Variable("fm amount",SYP_FMAMT,0) ;
	Insert(fmAmt_) ;
	fmRatio_=new Variable("fm ratio",SYP_FMRATIO,synthFmRatioNames,SYNTH_FM_RATIO_COUNT,2) ;
	Insert(fmRatio_) ;
	chord_=new Variable("chord",SYP_CHORD,synthChordNames,SYNTH_CHORD_COUNT,0) ;
	Insert(chord_) ;
	tune_=new Variable("tune",SYP_TUNE,0) ;
	Insert(tune_) ;
	fine_=new Variable("fine",SYP_FINE,0) ;
	Insert(fine_) ;
	attack_=new Variable("attack",SYP_ATTACK,0) ;
	Insert(attack_) ;
	decay_=new Variable("decay",SYP_DECAY,0xB0) ;
	Insert(decay_) ;
	sustain_=new Variable("sustain",SYP_SUSTAIN,0x80) ;
	Insert(sustain_) ;
	release_=new Variable("release",SYP_RELEASE,0x70) ;
	Insert(release_) ;
	pitchEnv_=new Variable("pitch env",SYP_PITCHENV,0) ;
	Insert(pitchEnv_) ;
	pitchDec_=new Variable("pitch decay",SYP_PITCHDEC,0x80) ;
	Insert(pitchDec_) ;
	glide_=new Variable("glide",SYP_GLIDE,0) ;
	Insert(glide_) ;
	filterType_=new Variable("filter",SYP_FILTTYPE,synthFilterNames,SFT_LAST,SFT_LOWPASS) ;
	Insert(filterType_) ;
	cutoff_=new Variable("cutoff",SYP_CUTOFF,0xC0) ;
	Insert(cutoff_) ;
	reso_=new Variable("resonance",SYP_RESO,0x20) ;
	Insert(reso_) ;
	envAmt_=new Variable("env amount",SYP_ENVAMT,0x30) ;
	Insert(envAmt_) ;
	envDec_=new Variable("env decay",SYP_ENVDEC,0x98) ;
	Insert(envDec_) ;
	drive_=new Variable("drive",SYP_DRIVE,0) ;
	Insert(drive_) ;
	lfoDest_=new Variable("lfo dest",SYP_LFODEST,synthLfoDestNames,SLD_LAST,SLD_PITCH) ;
	Insert(lfoDest_) ;
	lfoRate_=new Variable("lfo rate",SYP_LFORATE,0xB0) ;
	Insert(lfoRate_) ;
	lfoAmt_=new Variable("lfo amount",SYP_LFOAMT,0) ;
	Insert(lfoAmt_) ;
	reverb_=new Variable("reverb",SYP_REVERB,0) ;
	Insert(reverb_) ;
	delay_=new Variable("delay",SYP_DELAY,0) ;
	Insert(delay_) ;
	chorus_=new Variable("chorus",SYP_CHORUS,0) ;
	Insert(chorus_) ;
	volume_=new Variable("volume",SYP_VOLUME,0x80) ;
	Insert(volume_) ;
	pan_=new Variable("pan",SYP_PAN,0x7F) ;
	Insert(pan_) ;
	table_=new Variable("table",SYP_TABLE,-1) ;
	Insert(table_) ;
	tableAuto_=new Variable("table automation",SYP_TABLEAUTO,false) ;
	Insert(tableAuto_) ;
	mods_.Create(*this,MIK_SYNTH) ;
	eq_.Create(*this) ;
	customName_=new Variable("name",INSTRUMENT_NAME_ID,"") ;
	Insert(customName_) ;

	// FM4 (defaults = "fm init": D is heard, C modulates it gently)
	fmAlgo_=new Variable("fm algo",FM4P_ALGO,fm4AlgoNames,FM4_ALGO_COUNT,0) ;
	Insert(fmAlgo_) ;
	engineVars_.push_back(fmAlgo_) ;
	for (int op=0;op<FM4_OPS;op++) {
		for (int k=0;k<FM4_PARAM_COUNT;k++) {
			FourCC id=FM4_ID(op,fm4ParamChars[k]) ;
			const char *name=fm4VarNames[op][k] ;
			Variable *v ;
			switch(fm4ParamChars[k]) {
				case FM4P_SHAPE:
					v=new Variable(name,id,fm4ShapeNames,F4S_LAST,F4S_SIN) ;
					break ;
				case FM4P_RATIO:
					v=new Variable(name,id,100,0) ;
					break ;
				case FM4P_LEVEL:
					v=new Variable(name,id,op==3?0xFF:(op==2?0x60:0),0) ;
					break ;
				case FM4P_DECAY:
					v=new Variable(name,id,0x80,0) ;
					break ;
				case FM4P_SUSTAIN:
					v=new Variable(name,id,0xFF,0) ;
					break ;
				default:
					v=new Variable(name,id,0,0) ;
					break ;
			}
			fmOp_[op][k]=v ;
			Insert(v) ;
			engineVars_.push_back(v) ;
		}
	}

	// HYPER: the chord knob goes first, so a restored song's own notes
	// (saved after it) win
	hyperChord_=new SynthHookVariable(this,SYNTH_HOOK_HYPER_CHORD,"hyper chord",HYP_CHORD,
	                                  hyperChordNames,HYPER_CHORD_COUNT,4) ;
	Insert(hyperChord_) ;
	engineVars_.push_back(hyperChord_) ;
	for (int n=0;n<HYPER_NOTES;n++) {
		hyperNote_[n]=new SynthHookVariable(this,SYNTH_HOOK_HYPER_NOTE,hyperNoteNames[n],
		                                    HYP_NOTE(n),hyperChords[4][n]) ;
		Insert(hyperNote_[n]) ;
		engineVars_.push_back(hyperNote_[n]) ;
	}
	hyperShift_=new Variable("hyper shift",HYP_SHIFT,0,0) ;
	hyperSwarm_=new Variable("hyper swarm",HYP_SWARM,0x60,0) ;
	hyperWidth_=new Variable("hyper width",HYP_WIDTH,0x80,0) ;
	hyperSub_=new Variable("hyper sub",HYP_SUB,0,0) ;
	hyperScale_=new Variable("hyper scale",HYP_SCALE,false) ;
	Variable *hyperVars[5]={hyperShift_,hyperSwarm_,hyperWidth_,hyperSub_,hyperScale_} ;
	for (int k=0;k<5;k++) {
		Insert(hyperVars[k]) ;
		engineVars_.push_back(hyperVars[k]) ;
	}

	// WAV
	wavShape_=new Variable("wav shape",WVP_SHAPE,wavShapeNames,WVS_LAST,WVS_SAW) ;
	wavSize_=new Variable("wav size",WVP_SIZE,0xFF,0) ;
	wavMult_=new Variable("wav mult",WVP_MULT,0,0) ;
	wavWarp_=new Variable("wav warp",WVP_WARP,0,0) ;
	wavMirror_=new Variable("wav mirror",WVP_MIRROR,0x80,0) ;
	wavLimit_=new Variable("limit",WVP_LIMIT,wavLimitNames,WVL_LAST,WVL_SOFT) ;
	Variable *wavVars[6]={wavShape_,wavSize_,wavMult_,wavWarp_,wavMirror_,wavLimit_} ;
	for (int k=0;k<6;k++) {
		Insert(wavVars[k]) ;
		engineVars_.push_back(wavVars[k]) ;
	}

	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		SynthVoice &v=voices_[i] ;
		v.active_=false ;
		v.stage_=SS_OFF ;
		v.level_=0.0f ;
		v.pendingStart_=false ;
		v.midiNote_=60 ;
		v.pendingNote_=60 ;
		v.pendingClean_=true ;
		for (int p=0;p<SYNTH_MAX_PARTIALS;p++) {
			v.phase_[p][0]=v.phase_[p][1]=v.phase_[p][2]=0.0f ;
			v.fmPhase_[p]=0.0f ;
			v.fbLast_[p]=0.0f ;
			v.chord_[p]=0 ;
		}
		v.subPhase_=0.0f ;
		for (int m=0;m<6;m++) v.metalPhase_[m]=0.0f ;
		v.noiseState_=0x1234567u+i*7919u ;
		v.noiseHold_=0.0f ;
		v.noiseCounter_=0.0f ;
		v.pitchEnv_=0.0f ;
		v.filterEnv_=0.0f ;
		v.lfoPhase_=0.0f ;
		v.glideNote_=60.0f ;
		v.hasPlayed_=false ;
		v.fastRelease_=false ;
		v.ic1eq_=v.ic2eq_=0.0f ;
		v.ic1eqR_=v.ic2eqR_=0.0f ;
		v.fa1_=v.fa2_=v.fa3_=v.fk_=0.0f ;
		v.chordCount_=1 ;
		for (int p=0;p<SYNTH_MAX_PARTIALS;p++) {
			Fm4Start(v.fm_[p],0x9E3779B9u+i*977u+p) ;
			WavStart(v.wav_[p],0x1D872B41u+i*31u+p) ;
		}
		HyperStart(v.hyper_,0x2545F491u+i*7u) ;
		v.hyperCmdChord_=false ;
		for (int n=0;n<HYPER_NOTES;n++) {
			v.hyperCmd_[n]=0 ;
			v.hyperSemis_[n]=0 ;
			v.hyperRatio_[n]=1.0f ;
		}
		v.hyperKey_=-1 ;
		v.baseVolume_=v.volume_=i2fp(0x80) ;
		v.basePan_=v.pan_=i2fp(0x7F) ;
		v.baseCutoff_=v.cutoff_=fl2fp(0.75f) ;
		v.baseReso_=v.reso_=0 ;
		v.speed_=FP_ONE ;
		v.drive_=0 ;
		v.modVolScale_=1.0f ;
		for (int x=0;x<RUX_LAST;x++) v.modExtra_[x]=0.0f ;
		v.krateCount_=0 ;
		v.retrig_=false ;
		v.retrigLoop_=0 ;
		v.retrigCount_=0 ;
		v.updaters_.push_back(&v.volumeRamp_) ;
		v.updaters_.push_back(&v.panner_) ;
		v.updaters_.push_back(&v.cutRamp_) ;
		v.updaters_.push_back(&v.resRamp_) ;
		v.updaters_.push_back(&v.speedRamp_) ;
		v.updaters_.push_back(&v.legato_) ;
		v.updaters_.push_back(&v.pfin_) ;
		v.updaters_.push_back(&v.arp_) ;
		for (int m=0;m<MOD_SLOT_COUNT;m++) {
			v.updaters_.push_back(&v.mods_[m]) ;
		}
		for (unsigned int u=0;u<v.updaters_.size();u++) {
			v.updaters_[u]->Disable() ;
		}
	}
	tableState_.Reset() ;
	applyingPreset_=false ;
	ApplyPreset(0) ;
}

SynthInstrument::~SynthInstrument() {
}

bool SynthInstrument::Init() {
	tableState_.Reset() ;
	return true ;
}

void SynthInstrument::OnStart() {
	tableState_.Reset() ;
}

int SynthInstrument::getInt(FourCC id) {
	Variable *v=FindVariable(id) ;
	return v?v->GetInt():0 ;
}

int SynthInstrument::GetPreset() {
	return preset_->GetInt() ;
}

void SynthInstrument::ApplyPreset(int preset) {
	if (applyingPreset_) return ;
	if (preset<0 || preset>=SYNTH_PRESET_COUNT) return ;
	applyingPreset_=true ;

	// INIT is the base for every preset
	const SynthPreset &base=synthPresets[0] ;
	for (int i=0;i<SYNTH_PRESET_VALUES && base.values_[i].id_!=0;i++) {
		Variable *v=FindVariable(base.values_[i].id_) ;
		if (v) v->SetInt(base.values_[i].value_) ;
	}
	lfoAmt_->SetInt(0) ;
	mods_.Reset() ;
	eq_.Reset() ;
	reverb_->SetInt(0) ;
	delay_->SetInt(0) ;
	chorus_->SetInt(0) ;
	volume_->SetInt(0x80) ;
	pan_->SetInt(0x7F) ;
	resetEngineVariables() ;

	// Then the engine's own starting point (its first preset), then the
	// preset itself
	int engine=synthPresets[preset].engine_ ;
	engine_->SetInt(engine) ;
	int first,last ;
	GetPresetRange(engine,first,last) ;
	int steps[2]={first,preset} ;
	for (int k=0;k<2;k++) {
		int index=steps[k] ;
		if (index==0 || (k==1 && index==first)) continue ;
		const SynthPreset &p=synthPresets[index] ;
		for (int i=0;i<SYNTH_PRESET_VALUES && p.values_[i].id_!=0;i++) {
			Variable *v=FindVariable(p.values_[i].id_) ;
			if (v) v->SetInt(p.values_[i].value_) ;
		}
	}
	syncHyperChordName() ;
	if (preset_->GetInt()!=preset) {
		preset_->SetInt(preset,false) ;
	}
	applyingPreset_=false ;
	// No observer notification: the instrument screen binds its fields to
	// these same variables and simply redraws the new values.
}

void SynthInstrument::LoadPreset(const char *name) {
	for (int i=0;i<SYNTH_PRESET_COUNT;i++) {
		if (!strcmp(name,synthPresets[i].name_)) {
			preset_->SetInt(i) ;
			return ;
		}
	}
}

const char *SynthInstrument::GetName() {
	static char name[Variable::MAX_NAME_LENGTH+1] ;
	const char *custom=customName_->GetString() ;
	if (custom && custom[0]) {
		return custom ;
	}
	const char *preset=preset_->GetString() ;
	const char *wave=wave_->GetString() ;
	int first,last ;
	int engine=GetEngine() ;
	GetPresetRange(engine,first,last) ;
	if (preset_->GetInt()==0) {
		sprintf(name,"synth %s",wave) ;
	} else if (preset_->GetInt()==first && engine==SE_FM4) {
		sprintf(name,"fm4 algo %s",fmAlgo_->GetString()) ;
	} else if (preset_->GetInt()==first && engine==SE_HYPER) {
		sprintf(name,"hyper %s",hyperChord_->GetString()) ;
	} else if (preset_->GetInt()==first && engine==SE_WAV) {
		sprintf(name,"wav %s",wavShape_->GetString()) ;
	} else {
		sprintf(name,"%s",preset) ;
	}
	for (char *c=name;*c;c++) {
		if (*c>='a' && *c<='z') *c=*c-'a'+'A' ;
	}
	return name ;
}

void SynthInstrument::Purge() {
	preset_->SetInt(0,false) ;
	ApplyPreset(0) ;
	table_->SetInt(-1) ;
	tableAuto_->SetBool(false) ;
}

int SynthInstrument::GetTable() {
	int result=table_->GetInt() ;
	if (result>TABLE_COUNT) {
		return VAR_OFF ;
	}
	return result ;
}

bool SynthInstrument::GetTableAutomation() {
	return tableAuto_->GetBool() ;
}

void SynthInstrument::GetTableState(TableSaveState &state) {
	memcpy(state.hopCount_,tableState_.hopCount_,sizeof(uchar)*TABLE_STEPS*3) ;
	memcpy(state.position_,tableState_.position_,sizeof(int)*3) ;
}

void SynthInstrument::SetTableState(TableSaveState &state) {
	memcpy(tableState_.hopCount_,state.hopCount_,sizeof(uchar)*TABLE_STEPS*3) ;
	memcpy(tableState_.position_,state.position_,sizeof(int)*3) ;
}

int SynthInstrument::synthBrokenVoices_=0 ;

int SynthInstrument::BrokenVoiceCount() {
	return synthBrokenVoices_ ;
}

void SynthInstrument::GetVoiceDebug(int channel,int &stage,float &level) {
	stage=voices_[channel].active_?voices_[channel].stage_:-1 ;
	level=voices_[channel].level_ ;
}

bool SynthInstrument::IsReleasing(int channel) {
	SynthVoice &v=voices_[channel] ;
	return v.active_ && v.stage_==SS_RELEASE ;
}

/***************************************************************
 Note start / stop
 ***************************************************************/

bool SynthInstrument::Start(int channel,unsigned char note,bool cleanStart) {
	SynthVoice &v=voices_[channel] ;

	v.midiNote_=note ;

	if (cleanStart) {
		v.baseVolume_=v.volume_=i2fp(volume_->GetInt()) ;
		v.basePan_=v.pan_=i2fp(pan_->GetInt()) ;
		v.baseCutoff_=v.cutoff_=fl2fp(cutoff_->GetInt()/255.0f) ;
		v.baseReso_=v.reso_=fl2fp(reso_->GetInt()/255.0f) ;
		v.drive_=drive_->GetInt() ;
		v.retrig_=false ;
		v.retrigLoop_=0 ;
		v.retrigCount_=0 ;
		for (unsigned int u=0;u<v.updaters_.size();u++) {
			v.updaters_[u]->Disable() ;
		}
		v.activeUpdaters_.clear() ;
		v.speed_=FP_ONE ;

		int chord=chord_->GetInt() ;
		if (chord<0 || chord>=SYNTH_CHORD_COUNT) chord=0 ;
		v.chordCount_=0 ;
		for (int p=0;p<SYNTH_MAX_PARTIALS;p++) {
			if (synthChordNotes[chord][p]<0) break ;
			v.chord_[p]=synthChordNotes[chord][p] ;
			v.chordCount_++ ;
		}
		if (v.chordCount_<1) {
			v.chord_[0]=0 ;
			v.chordCount_=1 ;
		}
		v.hyperCmdChord_=false ;
	}
	v.krateCount_=0 ;
	// Envelopes and LFOs from the MOD page restart with every note, with or
	// without an instrument number on the step
	startMods(v,channel,note) ;

	bool legato=(glide_->GetInt()>0) && v.active_ && v.stage_!=SS_RELEASE && v.hasPlayed_ ;
	if (legato) {
		// Mono-synth slide: keep the amp envelope, retrigger brightness
		v.filterEnv_=1.0f ;
		v.pendingStart_=false ;
		return true ;
	}

	if (v.active_ && v.level_>0.01f) {
		// Short fade to avoid a click before restarting the voice
		v.stage_=SS_FADE ;
		v.pendingStart_=true ;
		v.pendingNote_=note ;
		v.pendingClean_=cleanStart ;
	} else {
		startVoice(channel,note,cleanStart) ;
	}
	return true ;
}

void SynthInstrument::startMods(SynthVoice &v,int channel,unsigned char note) {
	float sampleRate=(float)Audio::GetInstance()->GetSampleRate() ;
	if (sampleRate<8000.0f) sampleRate=44100.0f ;
	mods_.StartVoice(v.mods_,v.activeUpdaters_,
	                 sampleRate/(float)((SYNTH_KRATE/SYNTH_BLOCK)*SYNTH_BLOCK),
	                 note,channel,channel*131+note) ;
	// The note starts with the slots' first values, not the last note's
	applyUpdaters(v) ;
}

void SynthInstrument::startVoice(int channel,unsigned char note,bool cleanStart) {
	SynthVoice &v=voices_[channel] ;
	v.pendingStart_=false ;
	v.active_=true ;
	v.stage_=SS_ATTACK ;
	v.fastRelease_=false ;
	v.level_=0.0f ;
	v.pitchEnv_=1.0f ;
	v.filterEnv_=1.0f ;
	int wave=wave_->GetInt() ;
	for (int p=0;p<SYNTH_MAX_PARTIALS;p++) {
		if (wave==SW_SUPERSAW) {
			// Free-running detuned saws sound wider with spread phases
			v.phase_[p][0]=wrap01(v.phase_[p][0]+0.37f*p) ;
			v.phase_[p][1]=wrap01(v.phase_[p][1]+0.21f) ;
			v.phase_[p][2]=wrap01(v.phase_[p][2]+0.53f) ;
		} else {
			v.phase_[p][0]=v.phase_[p][1]=v.phase_[p][2]=0.0f ;
		}
		v.fmPhase_[p]=0.0f ;
		v.fbLast_[p]=0.0f ;
	}
	v.subPhase_=0.0f ;
	for (int m=0;m<6;m++) v.metalPhase_[m]=0.13f*m ;
	v.ic1eq_=v.ic2eq_=0.0f ;
	v.ic1eqR_=v.ic2eqR_=0.0f ;
	unsigned int seed=(unsigned int)(channel*7919+note*131)+v.noiseState_ ;
	switch(GetEngine()) {
		case SE_FM4:
			for (int p=0;p<SYNTH_MAX_PARTIALS;p++) Fm4Start(v.fm_[p],seed+p) ;
			break ;
		case SE_HYPER:
			HyperStart(v.hyper_,seed) ;
			v.hyperKey_=-1 ;
			break ;
		case SE_WAV:
			for (int p=0;p<SYNTH_MAX_PARTIALS;p++) WavStart(v.wav_[p],seed+p) ;
			break ;
		default:
			break ;
	}
	eq_.ResetVoice(channel) ;
	if (!v.hasPlayed_ || glide_->GetInt()==0) {
		v.glideNote_=(float)note ;
	}
	v.hasPlayed_=true ;
}

void SynthInstrument::Stop(int channel) {
	SynthVoice &v=voices_[channel] ;
	if (!v.active_) return ;
	if (v.pendingStart_) {
		v.pendingStart_=false ;
	}
	v.stage_=SS_RELEASE ;
	// Note-off / KILL: ADSR slots go to their release too
	for (int m=0;m<MOD_SLOT_COUNT;m++) {
		v.mods_[m].NoteOff() ;
	}
}

void SynthInstrument::AllNotesOff() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		SynthVoice &v=voices_[i] ;
		v.active_=false ;
		v.pendingStart_=false ;
		v.level_=0.0f ;
		v.stage_=SS_OFF ;
		v.fastRelease_=false ;
	}
}

void SynthInstrument::StopQuickly(int channel) {
	SynthVoice &v=voices_[channel] ;
	if (!v.active_) return ;
	v.pendingStart_=false ;
	v.stage_=SS_RELEASE ;
	v.fastRelease_=true ;
	for (int m=0;m<MOD_SLOT_COUNT;m++) {
		v.mods_[m].NoteOff() ;
	}
}

/***************************************************************
 Commands
 ***************************************************************/

void SynthInstrument::removeUpdater(SynthVoice &v,I_SRPUpdater *u) {
	if (!u->Enabled()) return ;
	u->Disable() ;
	std::vector<I_SRPUpdater *>::iterator it=v.activeUpdaters_.begin() ;
	while (it!=v.activeUpdaters_.end()) {
		if (*it==u) {
			v.activeUpdaters_.erase(it) ;
			return ;
		}
		it++ ;
	}
}

void SynthInstrument::ProcessCommand(int channel,FourCC cc,ushort value) {
	SynthVoice &v=voices_[channel] ;
	int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
	if (sampleCount<1) sampleCount=1 ;

	switch(cc) {
		case I_CMD_ARPG:
			v.arp_.SetData(value) ;
			if (!v.arp_.Enabled()) {
				v.arp_.Enable() ;
				v.activeUpdaters_.push_back(&v.arp_) ;
			}
			break ;

		case I_CMD_CHRD: {
			// Same nibble layout as ARPG, but all notes sound together
			int count=1 ;
			v.chord_[0]=0 ;
			for (int shift=12;shift>=0;shift-=4) {
				int n=(value>>shift)&0xF ;
				if (n!=0 && count<SYNTH_MAX_PARTIALS) {
					v.chord_[count++]=n ;
				}
			}
			v.chordCount_=count ;
			// HYPER: the chord's notes, then the same notes an octave up,
			// fill the six slots (one or two notes: three each side)
			for (int n=0;n<HYPER_NOTES;n++) {
				if (count>=3) {
					v.hyperCmd_[n]=v.chord_[n%count]+12*(n/count) ;
				} else {
					v.hyperCmd_[n]=v.chord_[(n%3)%count]+(n>=3?12:0) ;
				}
			}
			v.hyperCmdChord_=true ;
			v.hyperKey_=-1 ;
			break ;
		}

		case I_CMD_VOLM: {
			float targetVolume=float(value&0xFF) ;
			float speed=float(value>>8) ;
			float startVolume=fp2fl(v.volume_) ;
			float baseVolume=fp2fl(v.baseVolume_) ;
			speed=(speed==0)?0:fabs(targetVolume-startVolume)*SYNTH_KRATE/float(speed)/sampleCount ;
			v.volumeRamp_.SetData(targetVolume-baseVolume,speed,startVolume-baseVolume) ;
			if (!v.volumeRamp_.Enabled()) {
				v.volumeRamp_.Enable() ;
				v.activeUpdaters_.push_back(&v.volumeRamp_) ;
			}
			if (speed==0) {
				v.volume_=i2fp((int)targetVolume) ;
			}
			break ;
		}

		case I_CMD_PAN_: {
			float targetPan=float(value&0xFF) ;
			if (targetPan==0xFF) targetPan=0xFE ;
			float basePan=fp2fl(v.basePan_) ;
			float speed=float(value>>8) ;
			float startPan=fp2fl(v.pan_) ;
			speed=(speed==0)?0:fabs(targetPan-startPan)*SYNTH_KRATE/float(speed)/sampleCount ;
			v.panner_.SetData(targetPan-basePan,speed,startPan-basePan) ;
			if (!v.panner_.Enabled()) {
				v.panner_.Enable() ;
				v.activeUpdaters_.push_back(&v.panner_) ;
			}
			if (speed==0) {
				v.pan_=i2fp((int)targetPan) ;
			}
			break ;
		}

		case I_CMD_FCUT: {
			float target=float(value&0xFF)/255.0f ;
			float speed=float(value>>8) ;
			float start=fp2fl(v.cutoff_) ;
			float baseCut=fp2fl(v.baseCutoff_) ;
			speed=(speed==0)?0:fabs(target-start)*SYNTH_KRATE/float(speed)/sampleCount ;
			v.cutRamp_.SetData(target-baseCut,speed,start-baseCut) ;
			if (!v.cutRamp_.Enabled()) {
				v.cutRamp_.Enable() ;
				v.activeUpdaters_.push_back(&v.cutRamp_) ;
			}
			if (speed==0) {
				v.cutoff_=fl2fp(target) ;
			}
			break ;
		}

		case I_CMD_FRES: {
			float target=float(value&0xFF)/255.0f ;
			float speed=float(value>>8) ;
			float start=fp2fl(v.reso_) ;
			float baseRes=fp2fl(v.baseReso_) ;
			speed=(speed==0)?0:fabs(target-start)*SYNTH_KRATE/float(speed)/sampleCount ;
			v.resRamp_.SetData(target-baseRes,speed,start-baseRes) ;
			if (!v.resRamp_.Enabled()) {
				v.resRamp_.Enable() ;
				v.activeUpdaters_.push_back(&v.resRamp_) ;
			}
			if (speed==0) {
				v.reso_=fl2fp(target) ;
			}
			break ;
		}

		case I_CMD_FLTR: {
			float cut=(value>>8)/255.0f ;
			float res=(value&0xFF)/255.0f ;
			v.cutoff_=v.baseCutoff_=fl2fp(cut) ;
			v.reso_=v.baseReso_=fl2fp(res) ;
			removeUpdater(v,&v.cutRamp_) ;
			removeUpdater(v,&v.resRamp_) ;
			break ;
		}

		case I_CMD_PTCH: {
			int pitch=(char)(value&0xFF) ;
			float speed=float(value>>8) ;
			if (pitch>127) pitch=pitch-256 ;
			float targetSpeed=float(pow(2.0,(pitch)/12.0)) ;
			float srcSpeed=fp2fl(v.speed_) ;
			if (srcSpeed<=0.0f) srcSpeed=1.0f ;
			speed=(speed==0)?0.0f:srcSpeed*255.0f/speed/SYNTH_KRATE/32.0f ;
			v.speedRamp_.SetData(targetSpeed,speed,srcSpeed) ;
			if (!v.speedRamp_.Enabled()) {
				v.speedRamp_.Enable() ;
				v.activeUpdaters_.push_back(&v.speedRamp_) ;
			}
			break ;
		}

		case I_CMD_LEGA: {
			// Slide from a pitch offset to the note, like SampleInstrument
			int pitch=(char)(value&0xFF) ;
			float speed=float(value>>8) ;
			if (pitch>127) pitch=pitch-256 ;
			float initSpeed ;
			if (pitch==0) {
				initSpeed=(float)pow(2.0,(v.glideNote_-v.midiNote_)/12.0) ;
				v.glideNote_=(float)v.midiNote_ ;
			} else {
				initSpeed=(float)pow(2.0,pitch/12.0) ;
			}
			speed=(speed==0)?0.0f:float(1+50.0/SYNTH_KRATE/speed) ;
			v.legato_.SetData(1.0f,speed,initSpeed) ;
			if (!v.legato_.Enabled()) {
				v.legato_.Enable() ;
				v.activeUpdaters_.push_back(&v.legato_) ;
			}
			break ;
		}

		case I_CMD_PFIN: {
			float semi=(value&0xFF)/float(0x80) ;
			if (semi>1) semi=semi-2 ;
			float speed=float(value>>8) ;
			float initSpeed=v.pfin_.Enabled()?v.pfin_.GetCurrent():1 ;
			float targetSpeed=float(pow(2.0f,semi/12.0f)) ;
			speed=(speed==0)?0.0f:float(1+50.0/SYNTH_KRATE/speed) ;
			v.pfin_.SetData(targetSpeed,speed,initSpeed) ;
			if (!v.pfin_.Enabled()) {
				v.pfin_.Enable() ;
				v.activeUpdaters_.push_back(&v.pfin_) ;
			}
			break ;
		}

		case I_CMD_RTRG: {
			// Re-strike the envelopes every N ticks: rolls, stutters
			unsigned char loop=(value&0xFF) ;
			if (loop!=0) {
				v.retrig_=true ;
				v.retrigLoop_=loop ;
				v.retrigCount_=loop ;
			} else {
				v.retrig_=false ;
			}
			break ;
		}

		case I_CMD_CRSH: {
			unsigned char drive=(value>>8) ;
			v.drive_=drive ;
			break ;
		}

		default:
			break ;
	}
}

/***************************************************************
 Rendering
 ***************************************************************/

void SynthInstrument::processUpdaters(SynthVoice &v,bool tick) {
	if (v.activeUpdaters_.empty()) return ;
	std::vector<I_SRPUpdater *>::iterator it ;
	for (it=v.activeUpdaters_.begin();it!=v.activeUpdaters_.end();it++) {
		(*it)->Trigger(tick) ;
	}
	applyUpdaters(v) ;
}

// Sums what the command ramps and MOD slots do into the voice's values
void SynthInstrument::applyUpdaters(SynthVoice &v) {
	RUParams rup ;
	rup.Reset() ;
	std::vector<I_SRPUpdater *>::iterator it ;
	for (it=v.activeUpdaters_.begin();it!=v.activeUpdaters_.end();it++) {
		(*it)->UpdateSRP(rup) ;
	}
	v.volume_=v.baseVolume_+rup.volumeOffset_ ;
	v.pan_=v.basePan_+rup.panOffset_ ;
	v.cutoff_=v.baseCutoff_+rup.cutOffset_ ;
	v.reso_=v.baseReso_+rup.resOffset_ ;
	v.speed_=rup.speedOffset_ ;
	v.modVolScale_=rup.volumeScale_ ;
	for (int x=0;x<RUX_LAST;x++) {
		v.modExtra_[x]=rup.extra_[x] ;
	}
}

void SynthInstrument::updateFilter(SynthVoice &v,float cutoff,float reso,float sampleRate) {
	float hz=CutoffHzFromParam(cutoff) ;
	float maxHz=sampleRate*0.45f ;
	if (hz>maxHz) hz=maxHz ;
	if (reso<0.0f) reso=0.0f ;
	if (reso>1.0f) reso=1.0f ;
	float g=(float)tan(SYNTH_PI*hz/sampleRate) ;
	float k=2.0f-1.94f*reso ;
	v.fk_=k ;
	v.fa1_=1.0f/(1.0f+g*(g+k)) ;
	v.fa2_=g*v.fa1_ ;
	v.fa3_=g*v.fa2_ ;
}

float SynthInstrument::renderPartial(SynthVoice &v,int p,float inc,float shape,
                                     float fmIndex,float fmRatio,int wave) {
	float *ph=v.phase_[p] ;
	float t=ph[0] ;
	float out=0.0f ;

	// Phase modulation from a sine operator
	float pm=0.0f ;
	if (fmIndex>0.0f) {
		pm=fmIndex*fastSin(v.fmPhase_[p])/SYNTH_TWO_PI ;
		v.fmPhase_[p]=wrap01(v.fmPhase_[p]+inc*fmRatio) ;
	}
	float tm=wrap01(t+pm) ;

	switch(wave) {
		case SW_SINE: {
			// shape = feedback self-modulation, sine -> saw-like
			float fb=shape*1.4f ;
			float s=fastSin(tm+fb*v.fbLast_[p]/SYNTH_TWO_PI) ;
			v.fbLast_[p]=0.5f*(v.fbLast_[p]+s) ;
			out=s ;
			break ;
		}
		case SW_TRIANGLE: {
			float x=4.0f*(float)fabs(tm-0.5f)-1.0f ;
			// shape = wavefolder
			x*=1.0f+shape*4.0f ;
			for (int i=0;i<4;i++) {
				if (x>1.0f) x=2.0f-x ;
				else if (x<-1.0f) x=-2.0f-x ;
				else break ;
			}
			out=x ;
			break ;
		}
		case SW_SAW: {
			out=2.0f*tm-1.0f-polyBlep(tm,inc) ;
			// shape blends in a second saw an octave up for a brighter edge
			if (shape>0.0f) {
				float t2=wrap01(tm*2.0f) ;
				out=out*(1.0f-0.4f*shape)+0.4f*shape*(2.0f*t2-1.0f-polyBlep(t2,inc*2.0f)) ;
			}
			break ;
		}
		case SW_PULSE: {
			float pw=0.5f-shape*0.45f ;
			float s=(tm<pw)?1.0f:-1.0f ;
			s+=polyBlep(tm,inc) ;
			s-=polyBlep(wrap01(tm+1.0f-pw),inc) ;
			out=s ;
			break ;
		}
		case SW_SUPERSAW: {
			float detune=shape*0.012f+0.0015f ;
			float inc1=inc*(1.0f-detune) ;
			float inc2=inc*(1.0f+detune) ;
			float t1=ph[1] ;
			float t2=ph[2] ;
			float s0=2.0f*tm-1.0f-polyBlep(tm,inc) ;
			float s1=2.0f*t1-1.0f-polyBlep(t1,inc1) ;
			float s2=2.0f*t2-1.0f-polyBlep(t2,inc2) ;
			out=0.5f*s0+0.42f*(s1+s2) ;
			ph[1]=wrap01(t1+inc1) ;
			ph[2]=wrap01(t2+inc2) ;
			break ;
		}
		case SW_NOISE: {
			if (shape<=0.0f) {
				out=whiteNoise(v.noiseState_) ;
			} else {
				// Sample & hold noise pitched by the note: crunchy/tonal
				float holdInc=inc*(float)pow(2.0,(1.0-shape)*6.0) ;
				v.noiseCounter_+=holdInc ;
				if (v.noiseCounter_>=1.0f) {
					v.noiseCounter_-=(float)floor(v.noiseCounter_) ;
					v.noiseHold_=whiteNoise(v.noiseState_) ;
				}
				out=v.noiseHold_ ;
			}
			break ;
		}
		case SW_METAL: {
			if (p!=0) break ;
			float spread=0.5f+shape*1.5f ;
			float sum=0.0f ;
			for (int m=0;m<6;m++) {
				float r=1.0f+(synthMetalRatios[m]-1.0f)*spread ;
				float mt=v.metalPhase_[m] ;
				sum+=(mt<0.5f)?1.0f:-1.0f ;
				v.metalPhase_[m]=wrap01(mt+inc*r) ;
			}
			out=sum/6.0f ;
			break ;
		}
		default:
			break ;
	}
	ph[0]=wrap01(t+inc) ;
	return out ;
}

// FM4 settings that only change when a knob does (read once per buffer)
void SynthInstrument::setupFm4(Fm4Params &p,float sampleRate) {
	int algo=fmAlgo_->GetInt() ;
	if (algo<0 || algo>=FM4_ALGO_COUNT) algo=0 ;
	p.algo_=algo ;
	float blockRate=sampleRate/SYNTH_BLOCK ;
	for (int op=0;op<FM4_OPS;op++) {
		int shape=fmOp_[op][0]->GetInt() ;
		p.shape_[op]=(shape>=0 && shape<F4S_LAST)?shape:F4S_SIN ;
		int ratio=fmOp_[op][1]->GetInt() ;
		if (ratio<FM4_RATIO_MIN) ratio=FM4_RATIO_MIN ;
		if (ratio>FM4_RATIO_MAX) ratio=FM4_RATIO_MAX ;
		p.ratio_[op]=ratio/100.0f ;
		p.level_[op]=fm4LevelAmp(fmOp_[op][2]->GetInt()) ;
		p.fbDepth_[op]=fm4FeedbackDepth(fmOp_[op][3]->GetInt()) ;
		p.attackStep_[op]=1.0f/(TimeFromParam(fmOp_[op][4]->GetInt())*blockRate) ;
		p.decayCoef_[op]=coefFromTime(TimeFromParam(fmOp_[op][5]->GetInt()),blockRate) ;
		p.sustain_[op]=fmOp_[op][6]->GetInt()/255.0f ;
	}
	p.modScale_=1.0f ;
	p.carrierNorm_=1.0f/(float)Fm4CarrierCount(algo) ;
}

// The six notes of the hyper chord on a root note, in semitones. With
// "scale" on, each note moves down onto the song's key/scale.
void SynthInstrument::GetHyperNotes(int root,int *semis) {
	for (int n=0;n<HYPER_NOTES;n++) {
		semis[n]=hyperNote_[n]->GetInt() ;
	}
	if (!hyperScale_->GetBool()) return ;
	Project *project=Player::GetInstance()->GetProject() ;
	if (!project) return ;
	int key=project->GetScaleKey() ;
	if (key<0) return ;
	int scale=project->GetScale() ;
	if (scale<0 || scale>=scaleCount) return ;
	for (int n=0;n<HYPER_NOTES;n++) {
		int note=root+semis[n] ;
		for (int tries=0;tries<12;tries++) {
			int inKey=(note-key)%12 ;
			if (inKey<0) inKey+=12 ;
			if (scaleSteps[scale][inKey]) break ;
			note-- ;
		}
		semis[n]=note-root ;
	}
}

// The voice's six note ratios, recomputed (pow) only when a note changed
void SynthInstrument::updateHyperRatios(SynthVoice &v) {
	int semis[HYPER_NOTES] ;
	if (v.hyperCmdChord_) {
		for (int n=0;n<HYPER_NOTES;n++) semis[n]=v.hyperCmd_[n] ;
	} else {
		GetHyperNotes(v.midiNote_,semis) ;
	}
	int key=0 ;
	for (int n=0;n<HYPER_NOTES;n++) {
		key=key*61+(semis[n]+30) ;
	}
	if (key!=v.hyperKey_) {
		v.hyperKey_=key ;
		for (int n=0;n<HYPER_NOTES;n++) {
			v.hyperSemis_[n]=semis[n] ;
			v.hyperRatio_[n]=(float)pow(2.0,semis[n]/12.0) ;
		}
	}
}

// Per control block: pitch of every saw, shift, width and sub
void SynthInstrument::setupHyper(const float *ratio,HyperParams &p,float baseInc,int swarm) {
	if (swarm<0) swarm=0 ;
	if (swarm>255) swarm=255 ;
	for (int n=0;n<HYPER_NOTES;n++) {
		// Half the spread each way; 2^(c/1200) ~ 1 + c*ln2/1200 this close
		float half=HyperDetuneCents(swarm,n)*0.5f*0.000577623f ;
		float inc=baseInc*ratio[n] ;
		float a=inc*(1.0f-half) ;
		float b=inc*(1.0f+half) ;
		p.inc_[n][0]=a>0.45f?0.45f:a ;
		p.inc_[n][1]=b>0.45f?0.45f:b ;
	}
	float first,second ;
	HyperShiftGains(hyperShift_->GetInt(),first,second) ;
	for (int n=0;n<HYPER_NOTES;n++) {
		p.gain_[n]=(n<3)?first:second ;
	}
	// About the same loudness whatever the shift
	p.norm_=1.2f/(float)sqrt(6.0f*(first*first+second*second)+0.0001f) ;
	// Saw a of each pair leans left, saw b right; 00 = both centred
	float w=hyperWidth_->GetInt()/255.0f ;
	float angleA=(1.0f-w)*0.7853982f ;
	float angleB=(1.0f+w)*0.7853982f ;
	p.gainL_[0]=(float)cos(angleA)*1.4142136f ;
	p.gainR_[0]=(float)sin(angleA)*1.4142136f ;
	p.gainL_[1]=(float)cos(angleB)*1.4142136f ;
	p.gainR_[1]=(float)sin(angleB)*1.4142136f ;
	int octaves ;
	float level ;
	HyperSub(hyperSub_->GetInt(),octaves,level) ;
	p.subLevel_=level ;
	p.subInc_=baseInc/(float)(1<<octaves) ;
}

bool SynthInstrument::Render(int channel,fixed *buffer,int size,bool updateTick) {
	SynthVoice &v=voices_[channel] ;
	SYS_MEMSET(buffer,0,size*2*sizeof(fixed)) ;
	if (!v.active_) return false ;

	float sampleRate=(float)Audio::GetInstance()->GetSampleRate() ;
	if (sampleRate<8000.0f) sampleRate=44100.0f ;
	int engine=GetEngine() ;

	// Tick-level updates (arp steps, retrig)
	if (updateTick) {
		processUpdaters(v,true) ;
		if (v.retrig_) {
			if (--v.retrigCount_<=0) {
				v.retrigCount_=v.retrigLoop_ ;
				v.stage_=SS_ATTACK ;
				v.pitchEnv_=1.0f ;
				v.filterEnv_=1.0f ;
				if (engine==SE_FM4) {
					for (int p=0;p<SYNTH_MAX_PARTIALS;p++) {
						for (int op=0;op<FM4_OPS;op++) v.fm_[p].stage_[op]=0 ;
					}
				}
				// each re-strike restarts the MOD envelopes too
				for (int m=0;m<MOD_SLOT_COUNT;m++) {
					if (v.mods_[m].Enabled()) v.mods_[m].Retrigger() ;
				}
			}
		}
	}

	int wave=wave_->GetInt() ;
	float shapeBase=shape_->GetInt()/255.0f ;
	float subLevel=(engine==SE_SYNTH)?sub_->GetInt()/255.0f:0.0f ;
	// The noise knob belongs to the synth engine; a MOD slot aimed at
	// noise adds it to every engine
	float noiseBase=(engine==SE_SYNTH)?noise_->GetInt()/255.0f:0.0f ;
	float noiseMix=noiseBase ;
	float toneGain=(float)cos(noiseMix*SYNTH_PI*0.5f) ;
	float noiseGain=(float)sin(noiseMix*SYNTH_PI*0.5f) ;
	int ratioIndex=fmRatio_->GetInt() ;
	if (ratioIndex<0 || ratioIndex>=SYNTH_FM_RATIO_COUNT) ratioIndex=2 ;
	float fmRatio=synthFmRatios[ratioIndex] ;
	float fmBase=fmAmt_->GetInt()/255.0f*5.0f ;
	float envAmt=envAmt_->GetInt()/255.0f ;
	float tune=(float)tune_->GetInt()+fine_->GetInt()/100.0f ;
	float pitchEnvAmt=pitchEnv_->GetInt()*48.0f/255.0f ;
	float sustain=sustain_->GetInt()/255.0f ;
	float attackInc=1.0f/(TimeFromParam(attack_->GetInt())*sampleRate) ;
	float decayCoef=coefFromTime(TimeFromParam(decay_->GetInt()),sampleRate) ;
	float releaseCoef=coefFromTime(TimeFromParam(release_->GetInt()),sampleRate) ;
	float quickReleaseCoef=coefFromTime(SYNTH_QUICK_RELEASE_SECONDS,sampleRate) ;
	float pitchCoef=coefFromTime(TimeFromParam(pitchDec_->GetInt()),sampleRate) ;
	float envCoef=coefFromTime(TimeFromParam(envDec_->GetInt()),sampleRate) ;
	float fadeInc=1.0f/(SYNTH_FADE_SECONDS*sampleRate) ;
	int filterType=filterType_->GetInt() ;
	int lfoDest=lfoDest_->GetInt() ;
	float lfoInc=LfoRateFromParam(lfoRate_->GetInt())*SYNTH_BLOCK/sampleRate ;
	float lfoAmt=lfoAmt_->GetInt()/255.0f ;
	int glide=glide_->GetInt() ;
	float glideCoef=glide>0?coefFromTime(TimeFromParam(glide),sampleRate/SYNTH_BLOCK):0.0f ;
	int limit=wavLimit_->GetInt() ;

	// Engine settings
	Fm4Params fmp ;
	HyperParams hyp ;
	WavParams wvp ;
	int swarmBase=hyperSwarm_->GetInt() ;
	int wavShape=wavShape_->GetInt() ;
	int wavSize=wavSize_->GetInt() ;
	int wavMult=wavMult_->GetInt() ;
	int wavWarp=wavWarp_->GetInt() ;
	int wavMirror=wavMirror_->GetInt() ;
	if (engine==SE_FM4) {
		setupFm4(fmp,sampleRate) ;
	} else if (engine==SE_WAV) {
		WavSetup(wvp,wavShape,wavSize,wavMult,wavWarp,wavMirror) ;
	}
	bool stereo=(engine==SE_HYPER) ;

	float inc[SYNTH_MAX_PARTIALS] ;
	float subInc=0.0f ;
	float shape=shapeBase ;
	float fmIndex=0.0f ;
	float ampMod=1.0f ;
	float driveGain=1.0f ;
	float driveAmount=0.0f ;
	float partialNorm=1.0f ;
	float gainL=0.0f ;
	float gainR=0.0f ;
	static float sendBuffer[SENDFX_MAX_FRAMES*2] ;
	// Sends, moved by MOD slots aimed at them
	float reverbSend=modClamp01(reverb_->GetInt()/255.0f+v.modExtra_[RUX_REVERB]) ;
	float delaySend=modClamp01(delay_->GetInt()/255.0f+v.modExtra_[RUX_DELAY]) ;
	float chorusSend=modClamp01(chorus_->GetInt()/255.0f+v.modExtra_[RUX_CHORUS]) ;
	bool sending=(reverbSend>0.0f || delaySend>0.0f || chorusSend>0.0f) ;
	bool eqOn=eq_.Prepare() ;
	int rendered=0 ;

	fixed *out=buffer ;
	for (int i=0;i<size;i++) {

		// Control rate: pitch, filter, lfo, command ramps
		if ((i%SYNTH_BLOCK)==0) {
			if (--v.krateCount_<=0) {
				v.krateCount_=SYNTH_KRATE/SYNTH_BLOCK ;
				processUpdaters(v,false) ;
			}
			float lfo=fastSin(v.lfoPhase_) ;
			v.lfoPhase_=wrap01(v.lfoPhase_+lfoInc) ;

			if (glide>0) {
				v.glideNote_=v.midiNote_+(v.glideNote_-v.midiNote_)*glideCoef ;
			} else {
				v.glideNote_=(float)v.midiNote_ ;
			}
			float note=v.glideNote_+tune+pitchEnvAmt*v.pitchEnv_ ;
			if (lfoDest==SLD_PITCH) note+=lfo*lfoAmt*2.0f ;
			float freq=440.0f*(float)pow(2.0,(note-69.0f)/12.0f)*fp2fl(v.speed_) ;
			float baseInc=freq/sampleRate ;
			if (baseInc>0.45f) baseInc=0.45f ;
			if (baseInc<0.0f) baseInc=0.0f ;
			for (int p=0;p<v.chordCount_;p++) {
				float pi=baseInc*(float)pow(2.0,v.chord_[p]/12.0) ;
				inc[p]=pi>0.45f?0.45f:pi ;
			}
			subInc=baseInc*0.5f ;
			partialNorm=1.0f/(float)sqrt((float)v.chordCount_) ;
			// LFO "shape" and a MOD slot on shape (0..1 units) move each
			// engine's main timbre knob
			float shapeLfo=((lfoDest==SLD_SHAPE)?lfo*lfoAmt:0.0f)+v.modExtra_[RUX_SHAPE] ;

			switch(engine) {
				case SE_FM4: {
					// shape: FM brightness (every modulator's depth); a MOD
					// slot on "fm" does the same (+127 = twice as deep)
					fmp.modScale_=1.0f+shapeLfo+v.modExtra_[RUX_FM]/255.0f ;
					if (fmp.modScale_<0.0f) fmp.modScale_=0.0f ;
					for (int p=0;p<v.chordCount_;p++) {
						Fm4Block(v.fm_[p],fmp,inc[p],SYNTH_BLOCK) ;
					}
					break ;
				}
				case SE_HYPER:
					// LFO "shape": the swarm breathes
					updateHyperRatios(v) ;
					setupHyper(v.hyperRatio_,hyp,baseInc,swarmBase+(int)(shapeLfo*128.0f)) ;
					break ;
				case SE_WAV:
					// LFO "shape": moves the mirror (pulse width on pulse50)
					if (shapeLfo!=0.0f) {
						WavSetup(wvp,wavShape,wavSize,wavMult,wavWarp,wavMirror+(int)(shapeLfo*127.0f)) ;
					}
					break ;
				default:
					break ;
			}

			shape=shapeBase+v.modExtra_[RUX_SHAPE] ;
			if (lfoDest==SLD_SHAPE) {
				shape+=lfo*lfoAmt*0.5f ;
			}
			shape=modClamp01(shape) ;
			float fm=fmBase+v.modExtra_[RUX_FM]/255.0f*5.0f ;
			if (fm<0.0f) fm=0.0f ;
			fmIndex=fm*(1.0f+3.0f*envAmt*v.filterEnv_) ;
			if (v.modExtra_[RUX_NOISE]!=0.0f || noiseMix!=noiseBase) {
				float mix=modClamp01(noiseBase+v.modExtra_[RUX_NOISE]) ;
				if (mix!=noiseMix) {
					noiseMix=mix ;
					toneGain=(float)cos(noiseMix*SYNTH_PI*0.5f) ;
					noiseGain=(float)sin(noiseMix*SYNTH_PI*0.5f) ;
				}
			}

			ampMod=1.0f ;
			if (lfoDest==SLD_VOLUME) {
				ampMod=1.0f-lfoAmt*(0.5f+0.5f*lfo) ;
			}

			if (filterType!=SFT_OFF) {
				float cut=fp2fl(v.cutoff_)+envAmt*v.filterEnv_ ;
				if (lfoDest==SLD_CUTOFF) cut+=lfo*lfoAmt*0.5f ;
				updateFilter(v,cut,fp2fl(v.reso_),sampleRate) ;
			}

			driveAmount=v.drive_+v.modExtra_[RUX_DRIVE] ;
			if (driveAmount<0.0f) driveAmount=0.0f ;
			if (driveAmount>255.0f) driveAmount=255.0f ;
			driveGain=1.0f+driveAmount/255.0f*8.0f ;

			float vol=fp2fl(v.volume_)*v.modVolScale_ ;
			if (vol<0.0f) vol=0.0f ;
			if (vol>255.0f) vol=255.0f ;
			int pan=fp2i(v.pan_) ;
			if (pan<0) pan=0 ;
			if (pan>254) pan=254 ;
			float volf=vol/255.0f ;
			gainL=volf*fp2fl(panlaw[pan]) ;
			gainR=volf*fp2fl(panlaw[254-pan]) ;
		}

		// Amp envelope
		switch(v.stage_) {
			case SS_ATTACK:
				v.level_+=attackInc ;
				if (v.level_>=1.0f) {
					v.level_=1.0f ;
					v.stage_=SS_DECAY ;
				}
				break ;
			case SS_DECAY:
				v.level_=sustain+(v.level_-sustain)*decayCoef ;
				if (sustain<=0.0f && v.level_<SYNTH_SILENCE) {
					v.level_=0.0f ;
					v.stage_=SS_OFF ;
				}
				break ;
			case SS_RELEASE:
				v.level_*=v.fastRelease_?quickReleaseCoef:releaseCoef ;
				if (!(v.level_>=SYNTH_SILENCE)) {  // also ends a NaN level
					v.level_=0.0f ;
					v.stage_=SS_OFF ;
				}
				break ;
			case SS_FADE:
				v.level_-=fadeInc ;
				if (v.level_<=0.0f) {
					v.level_=0.0f ;
					if (v.pendingStart_) {
						startVoice(channel,v.pendingNote_,v.pendingClean_) ;
					} else {
						v.stage_=SS_OFF ;
					}
				}
				break ;
			default:
				break ;
		}
		if (v.stage_==SS_OFF) {
			v.active_=false ;
			// fill the rest with silence (already cleared)
			break ;
		}
		v.pitchEnv_*=pitchCoef ;
		v.filterEnv_*=envCoef ;

		// Oscillators
		float sig=0.0f ;
		float sigR=0.0f ;
		switch(engine) {
			case SE_FM4:
				for (int p=0;p<v.chordCount_;p++) {
					sig+=fm4Tick(v.fm_[p],fmp) ;
				}
				sig*=partialNorm ;
				break ;
			case SE_HYPER:
				hyperTick(v.hyper_,hyp,sig,sigR) ;
				break ;
			case SE_WAV:
				for (int p=0;p<v.chordCount_;p++) {
					sig+=wavTick(v.wav_[p],wvp,inc[p]) ;
				}
				sig*=partialNorm ;
				break ;
			default:
				for (int p=0;p<v.chordCount_;p++) {
					sig+=renderPartial(v,p,inc[p],shape,fmIndex,fmRatio,wave) ;
				}
				sig*=partialNorm ;

				if (subLevel>0.0f) {
					float st=v.subPhase_ ;
					float s=(st<0.5f)?1.0f:-1.0f ;
					s+=polyBlep(st,subInc) ;
					s-=polyBlep(wrap01(st+0.5f),subInc) ;
					sig+=s*subLevel*0.7f ;
					v.subPhase_=wrap01(st+subInc) ;
				}

				break ;
		}
		if (noiseMix>0.0f) {
			float nz=whiteNoise(v.noiseState_)*noiseGain ;
			sig=sig*toneGain+nz ;
			sigR=sigR*toneGain+nz ;
		}

		if (driveAmount>0.0f) {
			sig=synthLimit(limit,sig*driveGain) ;
			if (stereo) sigR=synthLimit(limit,sigR*driveGain) ;
		}

		// State variable filter (TPT), a second one for the right channel
		if (filterType!=SFT_OFF) {
			float v3=sig-v.ic2eq_ ;
			float v1=v.fa1_*v.ic1eq_+v.fa2_*v3 ;
			float v2=v.ic2eq_+v.fa2_*v.ic1eq_+v.fa3_*v3 ;
			v.ic1eq_=2.0f*v1-v.ic1eq_ ;
			v.ic2eq_=2.0f*v2-v.ic2eq_ ;
			switch(filterType) {
				case SFT_HIGHPASS:
					sig=sig-v.fk_*v1-v2 ;
					break ;
				case SFT_BANDPASS:
					sig=v1*v.fk_ ;
					break ;
				default:
					sig=v2 ;
					break ;
			}
			if (stereo) {
				float r3=sigR-v.ic2eqR_ ;
				float r1=v.fa1_*v.ic1eqR_+v.fa2_*r3 ;
				float r2=v.ic2eqR_+v.fa2_*v.ic1eqR_+v.fa3_*r3 ;
				v.ic1eqR_=2.0f*r1-v.ic1eqR_ ;
				v.ic2eqR_=2.0f*r2-v.ic2eqR_ ;
				switch(filterType) {
					case SFT_HIGHPASS:
						sigR=sigR-v.fk_*r1-r2 ;
						break ;
					case SFT_BANDPASS:
						sigR=r1*v.fk_ ;
						break ;
					default:
						sigR=r2 ;
						break ;
				}
			}
		}
		if (!stereo) sigR=sig ;

		if (sig!=sig || sig>1e6f || sig<-1e6f || sigR!=sigR || sigR>1e6f || sigR<-1e6f ||
		    v.level_!=v.level_) {
			// NaN / runaway state (e.g. the filter): silence this voice
			// instead of letting it ring on forever
			synthBrokenVoices_++ ;
			v.stage_=SS_OFF ;
			v.active_=false ;
			v.level_=0.0f ;
			v.ic1eq_=v.ic2eq_=0.0f ;
			v.ic1eqR_=v.ic2eqR_=0.0f ;
			break ;
		}
		float amp=v.level_*ampMod ;
		sig*=amp ;
		sigR*=amp ;
		// The instrument's own EQ (EQ page), before pan and the sends
		if (eqOn) {
			if (stereo) eq_.TickStereo(channel,sig,sigR) ;
			else sigR=sig=eq_.TickMono(channel,sig) ;
		}
		if (sig>2.0f) sig=2.0f ;
		if (sig<-2.0f) sig=-2.0f ;
		if (sigR>2.0f) sigR=2.0f ;
		if (sigR<-2.0f) sigR=-2.0f ;

		float outL=sig*gainL ;
		float outR=sigR*gainR ;
		*out++=fl2fp(outL*32767.0f) ;
		*out++=fl2fp(outR*32767.0f) ;
		if (sending && i<SENDFX_MAX_FRAMES) {
			sendBuffer[i*2]=outL ;
			sendBuffer[i*2+1]=outR ;
			rendered=i+1 ;
		}
	}
	if (sending && rendered>0) {
		SendFX::GetInstance()->AddSend(channel,sendBuffer,rendered,reverbSend,delaySend,chorusSend) ;
	}
	return true ;
}

// One cycle of the current oscillator setup, for the instrument screen.
void SynthInstrument::RenderCycle(float *out,int count) {
	if (count<=0) return ;
	SynthVoice v ;
	for (int p=0;p<SYNTH_MAX_PARTIALS;p++) {
		v.phase_[p][0]=v.phase_[p][1]=v.phase_[p][2]=0.0f ;
		v.fmPhase_[p]=0.0f ;
		v.fbLast_[p]=0.0f ;
	}
	for (int m=0;m<6;m++) v.metalPhase_[m]=0.0f ;
	v.noiseState_=0x2545F491u ;
	v.noiseHold_=0.0f ;
	v.noiseCounter_=0.0f ;
	int wave=wave_->GetInt() ;
	float shape=shape_->GetInt()/255.0f ;
	int ratioIndex=fmRatio_->GetInt() ;
	if (ratioIndex<0 || ratioIndex>=SYNTH_FM_RATIO_COUNT) ratioIndex=2 ;
	float fmIndex=fmAmt_->GetInt()/255.0f*5.0f*(1.0f+3.0f*envAmt_->GetInt()/255.0f) ;
	float inc=1.0f/(float)count ;
	float subLevel=sub_->GetInt()/255.0f ;
	float noiseMix=noise_->GetInt()/255.0f ;
	unsigned int noiseState=0x9E3779B9u ;
	// Run a few cycles first so feedback/supersaw settle
	float peak=0.0001f ;
	for (int pass=0;pass<3;pass++) {
		for (int i=0;i<count;i++) {
			float s=renderPartial(v,0,inc,shape,fmIndex,synthFmRatios[ratioIndex],wave) ;
			if (subLevel>0.0f) {
				float st=wrap01(i*inc*0.5f) ;
				s+=((st<0.5f)?1.0f:-1.0f)*subLevel*0.7f ;
			}
			if (noiseMix>0.0f) {
				s=s*(float)cos(noiseMix*SYNTH_PI*0.5f)+whiteNoise(noiseState)*(float)sin(noiseMix*SYNTH_PI*0.5f) ;
			}
			if (pass==2) {
				out[i]=s ;
				float a=(float)fabs(s) ;
				if (a>peak) peak=a ;
			}
		}
	}
	for (int i=0;i<count;i++) {
		out[i]/=peak ;
	}
}

// A few cycles of what the current engine plays, for the instrument screen:
// FM4 with every operator at the top of its envelope, WAV through the
// drive stage (so fold/wrap show), HYPER as the mono sum.
void SynthInstrument::RenderPreview(float *out,int count,float cycles) {
	if (count<=0) return ;
	int engine=GetEngine() ;
	float inc=cycles/(float)count ;
	float peak=0.0001f ;
	if (engine==SE_FM4) {
		Fm4Ops o ;
		Fm4Params p ;
		Fm4Start(o,0x13579BDu) ;
		setupFm4(p,44100.0f) ;
		for (int op=0;op<FM4_OPS;op++) {
			float r=inc*p.ratio_[op] ;
			if (r>0.45f) r=0.45f ;
			o.inc_[op]=(unsigned int)(r*4294967296.0f) ;
			o.env_[op]=1.0f ;
			o.stage_[op]=1 ;
			o.gain_[op]=p.level_[op] ;
			o.gainStep_[op]=0.0f ;
		}
		// One pass first so feedback settles
		for (int pass=0;pass<2;pass++) {
			for (int op=0;op<FM4_OPS;op++) o.phase_[op]=0 ;
			for (int i=0;i<count;i++) {
				float s=fm4Tick(o,p) ;
				if (pass==1) out[i]=s ;
			}
		}
	} else if (engine==SE_WAV) {
		WavParams p ;
		WavOsc o ;
		WavSetup(p,wavShape_->GetInt(),wavSize_->GetInt(),wavMult_->GetInt(),
		         wavWarp_->GetInt(),wavMirror_->GetInt()) ;
		WavStart(o,0x2468ACEu) ;
		int drive=drive_->GetInt() ;
		float driveGain=1.0f+drive/255.0f*8.0f ;
		int limit=wavLimit_->GetInt() ;
		for (int i=0;i<count;i++) {
			float s=wavTick(o,p,inc) ;
			if (drive>0) s=synthLimit(limit,s*driveGain) ;
			out[i]=s ;
		}
	} else if (engine==SE_HYPER) {
		HyperOsc o ;
		HyperParams p ;
		HyperStart(o,0x55AA55u) ;
		int semis[HYPER_NOTES] ;
		float ratio[HYPER_NOTES] ;
		GetHyperNotes(60,semis) ;
		for (int n=0;n<HYPER_NOTES;n++) {
			ratio[n]=(float)pow(2.0,semis[n]/12.0) ;
		}
		setupHyper(ratio,p,inc,hyperSwarm_->GetInt()) ;
		for (int i=0;i<count;i++) {
			float l,r ;
			hyperTick(o,p,l,r) ;
			out[i]=0.5f*(l+r) ;
		}
	} else {
		// The original synth: one cycle, repeated
		int per=(int)((float)count/(cycles>1.0f?cycles:1.0f)) ;
		if (per<1) per=1 ;
		float cycle[256] ;
		if (per>256) per=256 ;
		RenderCycle(cycle,per) ;
		for (int i=0;i<count;i++) out[i]=cycle[i%per] ;
		return ;
	}
	for (int i=0;i<count;i++) {
		float a=(float)fabs(out[i]) ;
		if (a>peak) peak=a ;
	}
	for (int i=0;i<count;i++) {
		out[i]/=peak ;
	}
}
