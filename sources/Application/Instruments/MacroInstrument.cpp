#include "MacroInstrument.h"
#include "CommandList.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Model/Table.h"
#include "Services/Audio/Audio.h"
#include "System/Console/Trace.h"
#include "Application/Mixer/SendFX.h"
#include "Externals/Braids/braids/macro_oscillator.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Pan law shared with SampleInstrument (SampleInstrumentDatas.h)
extern fixed panlaw[] ;

#define MACRO_PI 3.14159265358979f
#define MACRO_TWO_PI 6.28318530717959f
// Control rate (output samples) and command ramp rate, as the synth
#define MACRO_BLOCK 16
#define MACRO_KRATE 100
#define MACRO_FADE_SECONDS 0.003f
#define MACRO_SILENCE 0.00008f
#define MACRO_QUICK_RELEASE_SECONDS 0.08f
// The oscillator's own rate and block size
#define MACRO_OSC_RATE 96000
#define MACRO_OSC_BLOCK 24

enum MacroStage {
	MSS_OFF=0,
	MSS_ATTACK,
	MSS_DECAY,
	MSS_RELEASE,
	MSS_FADE
} ;

/***************************************************************
 Shapes
 ***************************************************************/

struct MacroShapeInfo {
	const char *name_ ;
	const char *timbre_ ;
	const char *color_ ;
	MacroPreviewKind preview_ ;
} ;

// Names as on the M8 / the module's display, then what the two knobs do
static const MacroShapeInfo macroShapes[MACRO_SHAPE_COUNT]={
	{"CSAW","notch width","notch depth/phase",MPK_CYCLE},
	{"MORPH","tri > saw > square","darker > fuzzier",MPK_CYCLE},
	{"SAW/SQR","phase/pulse width","saw to square",MPK_CYCLE},
	{"FOLD","wavefolder amount","sine to triangle",MPK_CYCLE},
	{"BUZZ","sine to buzzy comb","detune of 2 buzzes",MPK_CYCLE},
	{"SQR SUB","pulse width","sub -1 / -2 octaves",MPK_CYCLE},
	{"SAW SUB","saw width","sub -1 / -2 octaves",MPK_CYCLE},
	{"SQR SYNC","synced osc pitch","mix of the 2 oscs",MPK_CYCLE},
	{"SAW SYNC","synced osc pitch","mix of the 2 oscs",MPK_CYCLE},
	{"SAW x3","2nd saw interval","3rd saw interval",MPK_CYCLE},
	{"SQR x3","2nd sqr interval","3rd sqr interval",MPK_CYCLE},
	{"TRI x3","2nd tri interval","3rd tri interval",MPK_CYCLE},
	{"SIN x3","2nd sine interval","3rd sine interval",MPK_CYCLE},
	{"RING","2nd sine ratio","3rd sine ratio",MPK_CYCLE},
	{"SWARM","detune of 7 saws","highpass",MPK_CYCLE},
	{"COMB","comb pitch","feedback: 80 none",MPK_CYCLE},
	{"TOY","clock rate","glitches",MPK_CYCLE},
	{"ZLPF","lowpass cutoff","saw > square > tri",MPK_CYCLE},
	{"ZPKF","peak cutoff","saw > square > tri",MPK_CYCLE},
	{"ZBPF","bandpass cutoff","saw > square > tri",MPK_CYCLE},
	{"ZHPF","highpass cutoff","saw > square > tri",MPK_CYCLE},
	{"VOSIM","formant 1","formant 2",MPK_CYCLE},
	{"VOWEL","vowel a e i o u","formant shift",MPK_CYCLE},
	{"VFOF","vowel a e i o u","formant shift",MPK_CYCLE},
	{"HARM","centre harmonic","spread of harmonics",MPK_CYCLE},
	{"FM","FM amount","modulator ratio",MPK_CYCLE},
	{"FBFM","FM amount","ratio + feedback",MPK_CYCLE},
	{"CHAOFM","FM amount","ratio, chaotic",MPK_CYCLE},
	{"PLUCK","damping","pluck position",MPK_HIT},
	{"BOWED","bow friction","bow position",MPK_HIT},
	{"BLOWN","air pressure","reed geometry",MPK_HIT},
	{"FLUTE","air pressure","body geometry",MPK_HIT},
	{"BELL","damping","inharmonicity",MPK_HIT},
	{"DRUM","damping","brightness",MPK_HIT},
	{"KICK","decay","tone",MPK_HIT},
	{"CYMBAL","band cutoff","squares vs noise",MPK_TEXTURE},
	{"SNARE","tone balance","snappy noise",MPK_HIT},
	{"WTBL","sweep the table","which of 20 tables",MPK_CYCLE},
	{"WMAP","map across","map down",MPK_CYCLE},
	{"WLINE","scan all waves","smoothing",MPK_CYCLE},
	{"WTx4","morph the waves","chord of 4 voices",MPK_CYCLE},
	{"NOISE","resonance","lowpass to highpass",MPK_TEXTURE},
	{"TWINQ","resonance (Q)","peak spacing",MPK_TEXTURE},
	{"CLKN","loop length","number of steps",MPK_TEXTURE},
	{"CLOUD","grain density","pitch scatter",MPK_TEXTURE},
	{"PARTCL","density","pitch scatter",MPK_TEXTURE},
	{"QPSK","bit rate","data byte",MPK_TEXTURE}
} ;

static char *macroShapeNames[MACRO_SHAPE_COUNT] ;
static bool macroShapeNamesReady=false ;

static char **getShapeNameList() {
	if (!macroShapeNamesReady) {
		for (int i=0;i<MACRO_SHAPE_COUNT;i++) {
			macroShapeNames[i]=(char *)macroShapes[i].name_ ;
		}
		macroShapeNamesReady=true ;
	}
	return macroShapeNames ;
}

const char *MacroInstrument::GetShapeName(int shape) {
	if (shape<0 || shape>=MACRO_SHAPE_COUNT) return "" ;
	return macroShapes[shape].name_ ;
}

const char *MacroInstrument::GetTimbreHelp(int shape) {
	if (shape<0 || shape>=MACRO_SHAPE_COUNT) return "" ;
	return macroShapes[shape].timbre_ ;
}

const char *MacroInstrument::GetColorHelp(int shape) {
	if (shape<0 || shape>=MACRO_SHAPE_COUNT) return "" ;
	return macroShapes[shape].color_ ;
}

MacroPreviewKind MacroInstrument::GetPreviewKind(int shape) {
	if (shape<0 || shape>=MACRO_SHAPE_COUNT) return MPK_CYCLE ;
	return macroShapes[shape].preview_ ;
}

static char *macroFilterNames[SFT_LAST]={
	(char *)"lowpass",(char *)"highpass",(char *)"bandpass",(char *)"off"
} ;

static char *macroLfoDestNames[MLD_LAST]={
	(char *)"pitch",(char *)"cutoff",(char *)"volume",(char *)"timbre",(char *)"color"
} ;

/***************************************************************
 Presets
 ***************************************************************/

struct MacroPresetValue {
	FourCC id_ ;
	int value_ ;
} ;

#define MACRO_PRESET_VALUES 24

struct MacroPreset {
	const char *name_ ;
	MacroPresetValue values_[MACRO_PRESET_VALUES] ;
} ;

#define MV(a,b) {a,b}
#define MEND {0,0}

// Every preset starts from INIT, then applies its own values. Drums are
// tuned so C 3 sounds right, basses two octaves down (like the synth kit).
static const MacroPreset macroPresets[]={
	{"init",{
		MV(MCP_SHAPE,MS_CSAW),MV(MCP_TIMBRE,0x80),MV(MCP_COLOR,0x80),
		MV(MCP_DEGRADE,0),MV(MCP_REDUX,0),MV(SYP_TUNE,0),MV(SYP_FINE,0),
		MV(SYP_ATTACK,0),MV(SYP_DECAY,0xB0),MV(SYP_SUSTAIN,0xC0),MV(SYP_RELEASE,0x70),
		MV(SYP_PITCHENV,0),MV(SYP_PITCHDEC,0x80),MV(SYP_GLIDE,0),
		MV(SYP_FILTTYPE,SFT_OFF),MV(SYP_CUTOFF,0xC0),MV(SYP_RESO,0x20),
		MV(SYP_ENVAMT,0),MV(SYP_ENVDEC,0x98),MV(SYP_DRIVE,0),
		MV(MCP_TENV,0),MV(MCP_CENV,0),MV(MCP_TCDECAY,0x98),MV(MCP_LFODEST,MLD_PITCH)}},
	{"pluck",{
		MV(MCP_SHAPE,MS_PLUCK),MV(MCP_TIMBRE,0x70),MV(MCP_COLOR,0x40),
		MV(SYP_DECAY,0xD0),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x98),
		MV(SYP_DELAY,0x30),MV(SYP_VOLUME,0xF0),MEND}},
	{"bell",{
		MV(MCP_SHAPE,MS_BELL),MV(MCP_TIMBRE,0x60),MV(MCP_COLOR,0x38),
		MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0xC0),
		MV(SYP_REVERB,0x60),MV(SYP_VOLUME,0xE8),MEND}},
	{"vowels",{
		MV(MCP_SHAPE,MS_VOWEL),MV(MCP_TIMBRE,0x20),MV(MCP_COLOR,0x70),
		MV(SYP_ATTACK,0x30),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x90),
		MV(MCP_LFODEST,MLD_TIMBRE),MV(SYP_LFORATE,0x60),MV(SYP_LFOAMT,0x90),
		MV(SYP_VOLUME,0x90),MEND}},
	{"choir",{
		MV(MCP_SHAPE,MS_VFOF),MV(MCP_TIMBRE,0x30),MV(MCP_COLOR,0x68),
		MV(SYP_ATTACK,0xA0),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0xB8),
		MV(MCP_LFODEST,MLD_PITCH),MV(SYP_LFORATE,0xA8),MV(SYP_LFOAMT,0x10),
		MV(SYP_CHORUS,0x70),MV(SYP_REVERB,0x80),MV(SYP_VOLUME,0xC0),MEND}},
	{"flute",{
		MV(MCP_SHAPE,MS_FLUTE),MV(MCP_TIMBRE,0x98),MV(MCP_COLOR,0x70),
		MV(SYP_ATTACK,0x58),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x88),
		MV(MCP_LFODEST,MLD_PITCH),MV(SYP_LFORATE,0xB0),MV(SYP_LFOAMT,0x14),
		MV(SYP_REVERB,0x50),MV(SYP_VOLUME,0xA0),MEND}},
	{"strings",{
		MV(MCP_SHAPE,MS_BOWED),MV(MCP_TIMBRE,0x80),MV(MCP_COLOR,0x60),
		MV(SYP_ATTACK,0x88),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0xA8),
		MV(MCP_LFODEST,MLD_PITCH),MV(SYP_LFORATE,0xB0),MV(SYP_LFOAMT,0x10),
		MV(SYP_REVERB,0x60),MV(SYP_VOLUME,0xA0),MEND}},
	{"organ",{
		MV(MCP_SHAPE,MS_HARMONICS),MV(MCP_TIMBRE,0x30),MV(MCP_COLOR,0x50),
		MV(SYP_ATTACK,0x10),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x60),
		MV(SYP_CHORUS,0x50),MV(SYP_VOLUME,0x80),MEND}},
	{"chords",{
		MV(MCP_SHAPE,MS_WTX4),MV(MCP_TIMBRE,0x50),MV(MCP_COLOR,0x58),
		MV(SYP_ATTACK,0x70),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0xB0),
		MV(SYP_FILTTYPE,SFT_LOWPASS),MV(SYP_CUTOFF,0xB0),MV(SYP_RESO,0x10),
		MV(SYP_REVERB,0x70),MV(SYP_VOLUME,0xB8),MEND}},
	{"synclead",{
		MV(MCP_SHAPE,MS_SAWSYNC),MV(MCP_TIMBRE,0x50),MV(MCP_COLOR,0xC0),
		MV(MCP_TENV,0x40),MV(MCP_TCDECAY,0x98),
		MV(SYP_SUSTAIN,0xD0),MV(SYP_RELEASE,0x80),MV(SYP_GLIDE,0x30),
		MV(SYP_FILTTYPE,SFT_LOWPASS),MV(SYP_CUTOFF,0xC8),
		MV(SYP_DELAY,0x40),MV(SYP_VOLUME,0xA8),MEND}},
	{"foldlead",{
		MV(MCP_SHAPE,MS_FOLD),MV(MCP_TIMBRE,0x30),MV(MCP_COLOR,0x80),
		MV(MCP_TENV,0x50),MV(MCP_TCDECAY,0x90),
		MV(SYP_SUSTAIN,0xC0),MV(SYP_RELEASE,0x80),
		MV(MCP_LFODEST,MLD_PITCH),MV(SYP_LFORATE,0xB0),MV(SYP_LFOAMT,0x10),
		MV(SYP_VOLUME,0x80),MEND}},
	{"bass",{
		MV(MCP_SHAPE,MS_SAWSUB),MV(MCP_TIMBRE,0x30),MV(MCP_COLOR,0x40),MV(SYP_TUNE,-24),
		MV(SYP_DECAY,0xB0),MV(SYP_SUSTAIN,0xA0),MV(SYP_RELEASE,0x60),
		MV(SYP_FILTTYPE,SFT_LOWPASS),MV(SYP_CUTOFF,0x60),MV(SYP_RESO,0x40),
		MV(SYP_ENVAMT,0x70),MV(SYP_ENVDEC,0x90),MV(SYP_DRIVE,0x20),
		MV(SYP_VOLUME,0x70),MEND}},
	{"fmbass",{
		MV(MCP_SHAPE,MS_FM),MV(MCP_TIMBRE,0x40),MV(MCP_COLOR,0x40),MV(SYP_TUNE,-24),
		MV(MCP_TENV,0x48),MV(MCP_TCDECAY,0x88),
		MV(SYP_DECAY,0xB0),MV(SYP_SUSTAIN,0x90),MV(SYP_RELEASE,0x60),
		MV(SYP_VOLUME,0x80),MEND}},
	{"reese",{
		MV(MCP_SHAPE,MS_SWARM),MV(MCP_TIMBRE,0x48),MV(MCP_COLOR,0x10),MV(SYP_TUNE,-24),
		MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x70),
		MV(SYP_FILTTYPE,SFT_LOWPASS),MV(SYP_CUTOFF,0x70),MV(SYP_RESO,0x30),MV(SYP_DRIVE,0x30),
		MV(SYP_VOLUME,0x60),MEND}},
	{"acid",{
		MV(MCP_SHAPE,MS_ZLPF),MV(MCP_TIMBRE,0x30),MV(MCP_COLOR,0x00),MV(SYP_TUNE,-24),
		MV(MCP_TENV,0x60),MV(MCP_TCDECAY,0x88),
		MV(SYP_DECAY,0xA0),MV(SYP_SUSTAIN,0x80),MV(SYP_RELEASE,0x60),MV(SYP_GLIDE,0x60),
		MV(SYP_DRIVE,0x50),MV(SYP_VOLUME,0x70),MEND}},
	{"kick",{
		MV(MCP_SHAPE,MS_KICK),MV(MCP_TIMBRE,0x90),MV(MCP_COLOR,0x50),MV(SYP_TUNE,-15),
		MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x80),
		MV(SYP_DRIVE,0x20),MV(SYP_VOLUME,0xC0),MEND}},
	{"snare",{
		MV(MCP_SHAPE,MS_SNARE),MV(MCP_TIMBRE,0x80),MV(MCP_COLOR,0xA0),MV(SYP_TUNE,7),
		MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x70),
		MV(SYP_VOLUME,0xC0),MEND}},
	{"hat",{
		MV(MCP_SHAPE,MS_CYMBAL),MV(MCP_TIMBRE,0xC0),MV(MCP_COLOR,0xB0),MV(SYP_TUNE,24),
		MV(SYP_DECAY,0x70),MV(SYP_SUSTAIN,0),MV(SYP_RELEASE,0x50),
		MV(SYP_FILTTYPE,SFT_HIGHPASS),MV(SYP_CUTOFF,0xB0),MV(SYP_RESO,0x10),
		MV(SYP_PAN,0x90),MV(SYP_VOLUME,0xE0),MEND}},
	{"metaltom",{
		MV(MCP_SHAPE,MS_DRUM),MV(MCP_TIMBRE,0x70),MV(MCP_COLOR,0x60),MV(SYP_TUNE,-5),
		MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x90),
		MV(SYP_REVERB,0x30),MV(SYP_VOLUME,0xD0),MEND}},
	{"toy",{
		MV(MCP_SHAPE,MS_TOY),MV(MCP_TIMBRE,0x90),MV(MCP_COLOR,0x28),
		MV(SYP_DECAY,0x98),MV(SYP_SUSTAIN,0x80),MV(SYP_RELEASE,0x60),
		MV(SYP_VOLUME,0x70),MEND}},
	{"wavescan",{
		MV(MCP_SHAPE,MS_WMAP),MV(MCP_TIMBRE,0x40),MV(MCP_COLOR,0x80),
		MV(SYP_ATTACK,0x40),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0x98),
		MV(MCP_LFODEST,MLD_TIMBRE),MV(SYP_LFORATE,0x50),MV(SYP_LFOAMT,0x80),
		MV(SYP_CHORUS,0x40),MV(SYP_VOLUME,0x70),MEND}},
	{"cloud",{
		MV(MCP_SHAPE,MS_CLOUD),MV(MCP_TIMBRE,0x90),MV(MCP_COLOR,0x40),
		MV(SYP_ATTACK,0xA8),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0xC8),
		MV(SYP_REVERB,0xA0),MV(SYP_VOLUME,0x90),MEND}},
	{"wind",{
		MV(MCP_SHAPE,MS_TWINQ),MV(MCP_TIMBRE,0xA8),MV(MCP_COLOR,0x40),
		MV(SYP_ATTACK,0xA0),MV(SYP_SUSTAIN,0xFF),MV(SYP_RELEASE,0xC0),
		MV(MCP_LFODEST,MLD_COLOR),MV(SYP_LFORATE,0x40),MV(SYP_LFOAMT,0x60),
		MV(SYP_REVERB,0x80),MV(SYP_VOLUME,0xC0),MEND}}
} ;

#define MACRO_PRESET_COUNT ((int)(sizeof(macroPresets)/sizeof(MacroPreset)))

static char *macroPresetNames[MACRO_PRESET_COUNT] ;
static bool macroPresetNamesReady=false ;

static char **getPresetNameList() {
	if (!macroPresetNamesReady) {
		for (int i=0;i<MACRO_PRESET_COUNT;i++) {
			macroPresetNames[i]=(char *)macroPresets[i].name_ ;
		}
		macroPresetNamesReady=true ;
	}
	return macroPresetNames ;
}

int MacroInstrument::GetPresetCount() {
	return MACRO_PRESET_COUNT ;
}

const char *MacroInstrument::GetPresetName(int index) {
	if (index<0 || index>=MACRO_PRESET_COUNT) return "" ;
	return macroPresets[index].name_ ;
}

/***************************************************************
 Helpers
 ***************************************************************/

static inline float coefFromTime(float seconds,float sampleRate) {
	// One-pole coefficient reaching ~-40dB after 'seconds'
	float samples=seconds*sampleRate ;
	if (samples<1.0f) samples=1.0f ;
	return (float)exp(-4.6/samples) ;
}

static inline float softSat(float x) {
	if (x>3.0f) return 1.0f ;
	if (x<-3.0f) return -1.0f ;
	float x2=x*x ;
	return x*(27.0f+x2)/(27.0f+9.0f*x2) ;
}

static inline float clamp01(float x) {
	if (x<0.0f) return 0.0f ;
	if (x>1.0f) return 1.0f ;
	return x ;
}

// Sine of a phase in cycles, wrapped first (no huge arguments to libm)
static inline float lfoSin(float phase) {
	phase-=(float)floor(phase) ;
	return (float)sin(MACRO_TWO_PI*phase) ;
}

// 16.16 hold increment for "degrade": 00 = every sample (96 kHz),
// FF = every 24th sample (4 kHz), like the module's rate setting but smooth
static unsigned int holdIncFromDegrade(int degrade) {
	if (degrade<=0) return 65536 ;
	if (degrade>255) degrade=255 ;
	double factor=pow(24.0,degrade/255.0) ;
	unsigned int inc=(unsigned int)(65536.0/factor+0.5) ;
	return inc<1?1:inc ;
}

// "redux": 00 = 16 bits .. FF = 2 bits, as the module's resolution setting
static unsigned short maskFromRedux(int redux) {
	if (redux<0) redux=0 ;
	if (redux>255) redux=255 ;
	int bits=16-(redux*14+127)/255 ;
	return (unsigned short)(0xFFFF<<(16-bits)) ;
}

/***************************************************************
 Resampler (96 kHz -> mixer rate), shared by every voice
 ***************************************************************/

static float *macroKernel=0 ;
static int macroL=0 ;
static int macroM=0 ;
static float macroKernelRate=0.0f ;

static int gcdInt(int a,int b) {
	while (b) {
		int t=a%b ;
		a=b ;
		b=t ;
	}
	return a ;
}

static double besselI0(double x) {
	double sum=1.0,term=1.0 ;
	for (int k=1;k<40;k++) {
		term*=(x/(2.0*k))*(x/(2.0*k)) ;
		sum+=term ;
		if (term<1e-12*sum) break ;
	}
	return sum ;
}

int MacroInstrument::ResampleL() {
	return macroL ;
}

int MacroInstrument::ResampleM() {
	return macroM ;
}

// Output n sits at input position n*M/L. Row p of the kernel is the
// Kaiser-windowed sinc for a read position p/L past a whole sample; the
// cutoff (0.476 of the output rate: 21 kHz at 44.1 kHz) keeps 0..16 kHz
// flat and everything that would fold back below 18 kHz 70 dB down.
void MacroInstrument::setupResampler(float outRate) {
	int rate=(int)(outRate+0.5f) ;
	if (rate<8000) rate=44100 ;
	int g=gcdInt(MACRO_OSC_RATE,rate) ;
	int L=rate/g ;
	int M=MACRO_OSC_RATE/g ;
	if (L>1024) {
		// Odd mixer rate: nearest 1024-phase ratio (well under a cent)
		L=1024 ;
		M=(int)((double)MACRO_OSC_RATE*1024.0/rate+0.5) ;
	}
	float *kernel=new float[L*MACRO_TAPS] ;
	double fc=0.476*rate ;
	if (fc>0.45*MACRO_OSC_RATE) fc=0.45*MACRO_OSC_RATE ;
	double wc=2.0*fc/MACRO_OSC_RATE ;
	const double beta=7.0 ;
	const double half=MACRO_TAPS/2.0 ;
	double i0beta=besselI0(beta) ;
	for (int p=0;p<L;p++) {
		double frac=(double)p/L ;
		double sum=0.0 ;
		float *row=kernel+p*MACRO_TAPS ;
		for (int t=0;t<MACRO_TAPS;t++) {
			double u=t-(half-1.0)-frac ;
			double x=wc*u ;
			double sinc=(fabs(x)<1e-9)?1.0:sin(M_PI*x)/(M_PI*x) ;
			double r=u/half ;
			double w=(r<=-1.0 || r>=1.0)?0.0:besselI0(beta*sqrt(1.0-r*r))/i0beta ;
			double h=sinc*w ;
			row[t]=(float)h ;
			sum+=h ;
		}
		for (int t=0;t<MACRO_TAPS;t++) {
			row[t]=(float)(row[t]/sum) ;
		}
	}
	delete[] macroKernel ;
	macroKernel=kernel ;
	macroL=L ;
	macroM=M ;
	macroKernelRate=outRate ;
	Trace::Log("MACRO","resampler 96000 -> %d Hz: %d phases, %d in per %d out",rate,L,M,L) ;
}

/***************************************************************
 Preset variable
 ***************************************************************/

MacroPresetVariable::MacroPresetVariable(MacroInstrument *owner,const char *name,
                                         FourCC id,char **list,int size,int index)
	:Variable(name,id,list,size,index),owner_(owner) {
}

void MacroPresetVariable::onChange() {
	if (owner_) {
		owner_->ApplyPreset(GetInt()) ;
	}
}

/***************************************************************
 Instrument
 ***************************************************************/

int MacroInstrument::brokenVoices_=0 ;

MacroInstrument::MacroInstrument() {
	applyingPreset_=true ;

	preset_=new MacroPresetVariable(this,"preset",SYP_PRESET,getPresetNameList(),MACRO_PRESET_COUNT,0) ;
	Insert(preset_) ;
	shape_=new Variable("shape",MCP_SHAPE,getShapeNameList(),MACRO_SHAPE_COUNT,MS_CSAW) ;
	Insert(shape_) ;
	timbre_=new Variable("timbre",MCP_TIMBRE,0x80) ;
	Insert(timbre_) ;
	color_=new Variable("color",MCP_COLOR,0x80) ;
	Insert(color_) ;
	degrade_=new Variable("degrade",MCP_DEGRADE,0) ;
	Insert(degrade_) ;
	redux_=new Variable("redux",MCP_REDUX,0) ;
	Insert(redux_) ;
	tune_=new Variable("tune",SYP_TUNE,0) ;
	Insert(tune_) ;
	fine_=new Variable("fine",SYP_FINE,0) ;
	Insert(fine_) ;
	attack_=new Variable("attack",SYP_ATTACK,0) ;
	Insert(attack_) ;
	decay_=new Variable("decay",SYP_DECAY,0xB0) ;
	Insert(decay_) ;
	sustain_=new Variable("sustain",SYP_SUSTAIN,0xC0) ;
	Insert(sustain_) ;
	release_=new Variable("release",SYP_RELEASE,0x70) ;
	Insert(release_) ;
	pitchEnv_=new Variable("pitch env",SYP_PITCHENV,0) ;
	Insert(pitchEnv_) ;
	pitchDec_=new Variable("pitch decay",SYP_PITCHDEC,0x80) ;
	Insert(pitchDec_) ;
	glide_=new Variable("glide",SYP_GLIDE,0) ;
	Insert(glide_) ;
	filterType_=new Variable("filter",SYP_FILTTYPE,macroFilterNames,SFT_LAST,SFT_OFF) ;
	Insert(filterType_) ;
	cutoff_=new Variable("cutoff",SYP_CUTOFF,0xC0) ;
	Insert(cutoff_) ;
	reso_=new Variable("resonance",SYP_RESO,0x20) ;
	Insert(reso_) ;
	envAmt_=new Variable("env amount",SYP_ENVAMT,0) ;
	Insert(envAmt_) ;
	envDec_=new Variable("env decay",SYP_ENVDEC,0x98) ;
	Insert(envDec_) ;
	drive_=new Variable("drive",SYP_DRIVE,0) ;
	Insert(drive_) ;
	lfoDest_=new Variable("lfo dest",MCP_LFODEST,macroLfoDestNames,MLD_LAST,MLD_PITCH) ;
	Insert(lfoDest_) ;
	lfoRate_=new Variable("lfo rate",SYP_LFORATE,0xB0) ;
	Insert(lfoRate_) ;
	lfoAmt_=new Variable("lfo amount",SYP_LFOAMT,0) ;
	Insert(lfoAmt_) ;
	tEnv_=new Variable("timbre env",MCP_TENV,0) ;
	Insert(tEnv_) ;
	cEnv_=new Variable("color env",MCP_CENV,0) ;
	Insert(cEnv_) ;
	tcDecay_=new Variable("tc decay",MCP_TCDECAY,0x98) ;
	Insert(tcDecay_) ;
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
	mods_.Create(*this) ;
	customName_=new Variable("name",INSTRUMENT_NAME_ID,"") ;
	Insert(customName_) ;

	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		MacroVoice &v=voices_[i] ;
		v.osc_=new braids::MacroOscillator() ;
		memset((void *)v.osc_,0,sizeof(braids::MacroOscillator)) ;
		v.osc_->Init() ;
		v.osc_->set_shape(braids::MACRO_OSC_SHAPE_CSAW) ;
		v.shape_=MS_CSAW ;
		v.active_=false ;
		v.stage_=MSS_OFF ;
		v.level_=0.0f ;
		v.pendingStart_=false ;
		v.midiNote_=60 ;
		v.pendingNote_=60 ;
		v.pendingClean_=true ;
		v.fastRelease_=false ;
		v.strike_=false ;
		v.holdPhase_=0 ;
		v.holdInc_=65536 ;
		v.held_=0 ;
		v.bitMask_=0xFFFF ;
		memset(v.fifo_,0,sizeof(v.fifo_)) ;
		v.fifoCount_=0 ;
		v.readPos_=0 ;
		v.phaseAcc_=0 ;
		v.pitchEnv_=0.0f ;
		v.filterEnv_=0.0f ;
		v.tcEnv_=0.0f ;
		v.lfoPhase_=0.0f ;
		v.glideNote_=60.0f ;
		v.hasPlayed_=false ;
		v.ic1eq_=v.ic2eq_=0.0f ;
		v.fa1_=v.fa2_=v.fa3_=v.fk_=0.0f ;
		v.baseVolume_=v.volume_=i2fp(0x80) ;
		v.basePan_=v.pan_=i2fp(0x7F) ;
		v.baseCutoff_=v.cutoff_=fl2fp(0.75f) ;
		v.baseReso_=v.reso_=0 ;
		v.baseTimbre_=v.timbre_=fl2fp(0.5f) ;
		v.baseColor_=v.color_=fl2fp(0.5f) ;
		v.speed_=FP_ONE ;
		v.drive_=0 ;
		v.krateCount_=0 ;
		v.retrig_=false ;
		v.retrigLoop_=0 ;
		v.retrigCount_=0 ;
		v.updaters_.push_back(&v.volumeRamp_) ;
		v.updaters_.push_back(&v.panner_) ;
		v.updaters_.push_back(&v.cutRamp_) ;
		v.updaters_.push_back(&v.resRamp_) ;
		v.updaters_.push_back(&v.timbreRamp_) ;
		v.updaters_.push_back(&v.colorRamp_) ;
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

MacroInstrument::~MacroInstrument() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		delete voices_[i].osc_ ;
		voices_[i].osc_=0 ;
	}
}

bool MacroInstrument::Init() {
	tableState_.Reset() ;
	return true ;
}

void MacroInstrument::OnStart() {
	tableState_.Reset() ;
}

int MacroInstrument::getInt(FourCC id) {
	Variable *v=FindVariable(id) ;
	return v?v->GetInt():0 ;
}

int MacroInstrument::GetPreset() {
	return preset_->GetInt() ;
}

void MacroInstrument::ApplyPreset(int preset) {
	if (applyingPreset_) return ;
	if (preset<0 || preset>=MACRO_PRESET_COUNT) return ;
	applyingPreset_=true ;

	// INIT is the base for every preset
	const MacroPreset &base=macroPresets[0] ;
	for (int i=0;i<MACRO_PRESET_VALUES && base.values_[i].id_!=0;i++) {
		Variable *v=FindVariable(base.values_[i].id_) ;
		if (v) v->SetInt(base.values_[i].value_) ;
	}
	lfoRate_->SetInt(0xB0) ;
	lfoAmt_->SetInt(0) ;
	mods_.Reset() ;
	reverb_->SetInt(0) ;
	delay_->SetInt(0) ;
	chorus_->SetInt(0) ;
	volume_->SetInt(0x80) ;
	pan_->SetInt(0x7F) ;

	if (preset!=0) {
		const MacroPreset &p=macroPresets[preset] ;
		for (int i=0;i<MACRO_PRESET_VALUES && p.values_[i].id_!=0;i++) {
			Variable *v=FindVariable(p.values_[i].id_) ;
			if (v) v->SetInt(p.values_[i].value_) ;
		}
	}
	if (preset_->GetInt()!=preset) {
		preset_->SetInt(preset,false) ;
	}
	applyingPreset_=false ;
}

void MacroInstrument::LoadPreset(const char *name) {
	for (int i=0;i<MACRO_PRESET_COUNT;i++) {
		if (!strcmp(name,macroPresets[i].name_)) {
			preset_->SetInt(i) ;
			return ;
		}
	}
}

const char *MacroInstrument::GetName() {
	static char name[Variable::MAX_NAME_LENGTH+1] ;
	const char *custom=customName_->GetString() ;
	if (custom && custom[0]) {
		return custom ;
	}
	if (preset_->GetInt()==0) {
		sprintf(name,"macro %s",GetShapeName(shape_->GetInt())) ;
	} else {
		sprintf(name,"%s",preset_->GetString()) ;
	}
	for (char *c=name;*c;c++) {
		if (*c>='a' && *c<='z') *c=*c-'a'+'A' ;
	}
	return name ;
}

void MacroInstrument::Purge() {
	preset_->SetInt(0,false) ;
	ApplyPreset(0) ;
	table_->SetInt(-1) ;
	tableAuto_->SetBool(false) ;
}

int MacroInstrument::GetTable() {
	int result=table_->GetInt() ;
	if (result>TABLE_COUNT) {
		return VAR_OFF ;
	}
	return result ;
}

bool MacroInstrument::GetTableAutomation() {
	return tableAuto_->GetBool() ;
}

void MacroInstrument::GetTableState(TableSaveState &state) {
	memcpy(state.hopCount_,tableState_.hopCount_,sizeof(uchar)*TABLE_STEPS*3) ;
	memcpy(state.position_,tableState_.position_,sizeof(int)*3) ;
}

void MacroInstrument::SetTableState(TableSaveState &state) {
	memcpy(tableState_.hopCount_,state.hopCount_,sizeof(uchar)*TABLE_STEPS*3) ;
	memcpy(tableState_.position_,state.position_,sizeof(int)*3) ;
}

int MacroInstrument::BrokenVoiceCount() {
	return brokenVoices_ ;
}

void MacroInstrument::GetVoiceDebug(int channel,int &stage,float &level) {
	stage=voices_[channel].active_?voices_[channel].stage_:-1 ;
	level=voices_[channel].level_ ;
}

bool MacroInstrument::IsReleasing(int channel) {
	MacroVoice &v=voices_[channel] ;
	return v.active_ && v.stage_==MSS_RELEASE ;
}

/***************************************************************
 Note start / stop
 ***************************************************************/

bool MacroInstrument::Start(int channel,unsigned char note,bool cleanStart) {
	MacroVoice &v=voices_[channel] ;

	v.midiNote_=note ;

	if (cleanStart) {
		v.baseVolume_=v.volume_=i2fp(volume_->GetInt()) ;
		v.basePan_=v.pan_=i2fp(pan_->GetInt()) ;
		v.baseCutoff_=v.cutoff_=fl2fp(cutoff_->GetInt()/255.0f) ;
		v.baseReso_=v.reso_=fl2fp(reso_->GetInt()/255.0f) ;
		v.baseTimbre_=v.timbre_=fl2fp(timbre_->GetInt()/255.0f) ;
		v.baseColor_=v.color_=fl2fp(color_->GetInt()/255.0f) ;
		v.drive_=drive_->GetInt() ;
		v.retrig_=false ;
		v.retrigLoop_=0 ;
		v.retrigCount_=0 ;
		for (unsigned int u=0;u<v.updaters_.size();u++) {
			v.updaters_[u]->Disable() ;
		}
		v.activeUpdaters_.clear() ;
		v.speed_=FP_ONE ;
		float sampleRate=(float)Audio::GetInstance()->GetSampleRate() ;
		if (sampleRate<8000.0f) sampleRate=44100.0f ;
		mods_.StartVoice(v.mods_,v.activeUpdaters_,
		                 sampleRate/(float)((MACRO_KRATE/MACRO_BLOCK)*MACRO_BLOCK),
		                 channel*131+note) ;
	}
	v.krateCount_=0 ;

	bool legato=(glide_->GetInt()>0) && v.active_ && v.stage_!=MSS_RELEASE && v.hasPlayed_ ;
	if (legato) {
		// Mono-synth slide: keep the envelopes and the model running
		v.filterEnv_=1.0f ;
		v.pendingStart_=false ;
		return true ;
	}

	if (v.active_ && v.level_>0.01f) {
		// Short fade to avoid a click before restarting the voice
		v.stage_=MSS_FADE ;
		v.pendingStart_=true ;
		v.pendingNote_=note ;
		v.pendingClean_=cleanStart ;
	} else {
		startVoice(channel,note,cleanStart) ;
	}
	return true ;
}

void MacroInstrument::startVoice(int channel,unsigned char note,bool cleanStart) {
	MacroVoice &v=voices_[channel] ;
	v.pendingStart_=false ;
	v.active_=true ;
	v.stage_=MSS_ATTACK ;
	v.fastRelease_=false ;
	v.level_=0.0f ;
	v.pitchEnv_=1.0f ;
	v.filterEnv_=1.0f ;
	v.tcEnv_=1.0f ;
	v.ic1eq_=v.ic2eq_=0.0f ;
	// Fresh input: the new note's first samples come straight from the model
	v.fifoCount_=0 ;
	v.readPos_=0 ;
	v.phaseAcc_=0 ;
	v.holdPhase_=0 ;
	v.strike_=true ;
	if (!v.hasPlayed_ || glide_->GetInt()==0) {
		v.glideNote_=(float)note ;
	}
	v.hasPlayed_=true ;
}

void MacroInstrument::Stop(int channel) {
	MacroVoice &v=voices_[channel] ;
	if (!v.active_) return ;
	if (v.pendingStart_) {
		v.pendingStart_=false ;
	}
	v.stage_=MSS_RELEASE ;
}

void MacroInstrument::AllNotesOff() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		MacroVoice &v=voices_[i] ;
		v.active_=false ;
		v.pendingStart_=false ;
		v.level_=0.0f ;
		v.stage_=MSS_OFF ;
		v.fastRelease_=false ;
	}
}

void MacroInstrument::StopQuickly(int channel) {
	MacroVoice &v=voices_[channel] ;
	if (!v.active_) return ;
	v.pendingStart_=false ;
	v.stage_=MSS_RELEASE ;
	v.fastRelease_=true ;
}

/***************************************************************
 Commands (same meaning and ramp speeds as the synth)
 ***************************************************************/

void MacroInstrument::removeUpdater(MacroVoice &v,I_SRPUpdater *u) {
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

// Linear 0..1 ramps (cutoff, resonance, timbre, color): target bb at speed aa
template <class RAMP>
static void startUnitRamp(std::vector<I_SRPUpdater *> &active,RAMP &ramp,
                          fixed &current,fixed base,ushort value,int sampleCount) {
	float target=float(value&0xFF)/255.0f ;
	float speed=float(value>>8) ;
	float start=fp2fl(current) ;
	float baseValue=fp2fl(base) ;
	speed=(speed==0)?0:fabs(target-start)*MACRO_KRATE/float(speed)/sampleCount ;
	ramp.SetData(target-baseValue,speed,start-baseValue) ;
	if (!ramp.Enabled()) {
		ramp.Enable() ;
		active.push_back(&ramp) ;
	}
	if (speed==0) {
		current=fl2fp(target) ;
	}
}

void MacroInstrument::ProcessCommand(int channel,FourCC cc,ushort value) {
	MacroVoice &v=voices_[channel] ;
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

		case I_CMD_VOLM: {
			float targetVolume=float(value&0xFF) ;
			float speed=float(value>>8) ;
			float startVolume=fp2fl(v.volume_) ;
			float baseVolume=fp2fl(v.baseVolume_) ;
			speed=(speed==0)?0:fabs(targetVolume-startVolume)*MACRO_KRATE/float(speed)/sampleCount ;
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
			speed=(speed==0)?0:fabs(targetPan-startPan)*MACRO_KRATE/float(speed)/sampleCount ;
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

		case I_CMD_FCUT:
			startUnitRamp(v.activeUpdaters_,v.cutRamp_,v.cutoff_,v.baseCutoff_,value,sampleCount) ;
			break ;

		case I_CMD_FRES:
			startUnitRamp(v.activeUpdaters_,v.resRamp_,v.reso_,v.baseReso_,value,sampleCount) ;
			break ;

		case I_CMD_TIMB:
			startUnitRamp(v.activeUpdaters_,v.timbreRamp_,v.timbre_,v.baseTimbre_,value,sampleCount) ;
			break ;

		case I_CMD_COLR:
			startUnitRamp(v.activeUpdaters_,v.colorRamp_,v.color_,v.baseColor_,value,sampleCount) ;
			break ;

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
			speed=(speed==0)?0.0f:srcSpeed*255.0f/speed/MACRO_KRATE/32.0f ;
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
			speed=(speed==0)?0.0f:float(1+50.0/MACRO_KRATE/speed) ;
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
			speed=(speed==0)?0.0f:float(1+50.0/MACRO_KRATE/speed) ;
			v.pfin_.SetData(targetSpeed,speed,initSpeed) ;
			if (!v.pfin_.Enabled()) {
				v.pfin_.Enable() ;
				v.activeUpdaters_.push_back(&v.pfin_) ;
			}
			break ;
		}

		case I_CMD_RTRG: {
			// Re-strike the model and the envelopes every N ticks
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

		case I_CMD_CRSH:
			v.drive_=(unsigned char)(value>>8) ;
			break ;

		default:
			// CHRD would need a whole model per chord note (4x the CPU):
			// the WTx4 shape plays chords instead
			break ;
	}
}

/***************************************************************
 Rendering
 ***************************************************************/

void MacroInstrument::processUpdaters(MacroVoice &v,bool tick) {
	if (v.activeUpdaters_.empty()) return ;
	std::vector<I_SRPUpdater *>::iterator it ;
	for (it=v.activeUpdaters_.begin();it!=v.activeUpdaters_.end();it++) {
		(*it)->Trigger(tick) ;
	}
	RUParams rup ;
	rup.cutOffset_=rup.resOffset_=rup.volumeOffset_=rup.panOffset_=0 ;
	rup.fbMixOffset_=rup.fbTunOffset_=0 ;
	rup.speedOffset_=FP_ONE ;
	for (it=v.activeUpdaters_.begin();it!=v.activeUpdaters_.end();it++) {
		(*it)->UpdateSRP(rup) ;
	}
	v.volume_=v.baseVolume_+rup.volumeOffset_ ;
	v.pan_=v.basePan_+rup.panOffset_ ;
	v.cutoff_=v.baseCutoff_+rup.cutOffset_ ;
	v.reso_=v.baseReso_+rup.resOffset_ ;
	// TIMB / COLR ride the two generic ramps the sampler uses for feedback
	v.timbre_=v.baseTimbre_+rup.fbMixOffset_ ;
	v.color_=v.baseColor_+rup.fbTunOffset_ ;
	v.speed_=rup.speedOffset_ ;
}

void MacroInstrument::updateFilter(MacroVoice &v,float cutoff,float reso,float sampleRate) {
	float hz=SynthInstrument::CutoffHzFromParam(cutoff) ;
	float maxHz=sampleRate*0.45f ;
	if (hz>maxHz) hz=maxHz ;
	if (reso<0.0f) reso=0.0f ;
	if (reso>1.0f) reso=1.0f ;
	float g=(float)tan(MACRO_PI*hz/sampleRate) ;
	float k=2.0f-1.94f*reso ;
	v.fk_=k ;
	v.fa1_=1.0f/(1.0f+g*(g+k)) ;
	v.fa2_=g*v.fa1_ ;
	v.fa3_=g*v.fa2_ ;
}

// One 24-sample block from the model at 96 kHz, through degrade and redux,
// onto the voice's resampler input
void MacroInstrument::fillBlock(MacroVoice &v) {
	static const uint8_t sync[MACRO_OSC_BLOCK]={0} ;
	int16_t block[MACRO_OSC_BLOCK] ;
	if (v.strike_) {
		v.osc_->Strike() ;
		v.strike_=false ;
	}
	v.osc_->Render(sync,block,MACRO_OSC_BLOCK) ;
	// Room: blocks are only added while fifoCount_ < readPos_+TAPS, and
	// readPos_ is compacted as soon as it passes MACRO_FIFO_COMPACT (it
	// moves at most 12 samples per output even at an 8 kHz mixer rate)
	typedef char fifo_room_check[(MACRO_FIFO_SIZE>=MACRO_FIFO_COMPACT+12+MACRO_TAPS+MACRO_OSC_BLOCK)?1:-1] __attribute__((unused)) ;
	float *out=v.fifo_+v.fifoCount_ ;
	const float scale=1.0f/32768.0f ;
	if (v.holdInc_>=65536) {
		for (int i=0;i<MACRO_OSC_BLOCK;i++) {
			out[i]=(short)(block[i]&v.bitMask_)*scale ;
		}
	} else {
		for (int i=0;i<MACRO_OSC_BLOCK;i++) {
			v.holdPhase_+=v.holdInc_ ;
			if (v.holdPhase_>=65536) {
				v.holdPhase_-=65536 ;
				v.held_=(short)(block[i]&v.bitMask_) ;
			}
			out[i]=v.held_*scale ;
		}
	}
	v.fifoCount_+=MACRO_OSC_BLOCK ;
}

bool MacroInstrument::Render(int channel,fixed *buffer,int size,bool updateTick) {
	MacroVoice &v=voices_[channel] ;
	SYS_MEMSET(buffer,0,size*2*sizeof(fixed)) ;
	if (!v.active_) return false ;

	float sampleRate=(float)Audio::GetInstance()->GetSampleRate() ;
	if (sampleRate<8000.0f) sampleRate=44100.0f ;
	if (!macroKernel || sampleRate!=macroKernelRate) {
		setupResampler(sampleRate) ;
	}
	const int L=macroL ;
	const int M=macroM ;

	// Tick-level updates (arp steps, retrig)
	if (updateTick) {
		processUpdaters(v,true) ;
		if (v.retrig_) {
			if (--v.retrigCount_<=0) {
				v.retrigCount_=v.retrigLoop_ ;
				v.stage_=MSS_ATTACK ;
				v.pitchEnv_=1.0f ;
				v.filterEnv_=1.0f ;
				v.tcEnv_=1.0f ;
				v.strike_=true ;
			}
		}
	}

	int shape=shape_->GetInt() ;
	if (shape<0 || shape>=MACRO_SHAPE_COUNT) shape=MS_CSAW ;
	if (shape!=v.shape_) {
		v.osc_->set_shape((braids::MacroOscillatorShape)shape) ;
		v.shape_=shape ;
	}
	v.holdInc_=holdIncFromDegrade(degrade_->GetInt()) ;
	v.bitMask_=maskFromRedux(redux_->GetInt()) ;

	float envAmt=envAmt_->GetInt()/255.0f ;
	float tune=(float)tune_->GetInt()+fine_->GetInt()/100.0f ;
	float pitchEnvAmt=pitchEnv_->GetInt()*48.0f/255.0f ;
	float sustain=sustain_->GetInt()/255.0f ;
	float attackInc=1.0f/(SynthInstrument::TimeFromParam(attack_->GetInt())*sampleRate) ;
	float decayCoef=coefFromTime(SynthInstrument::TimeFromParam(decay_->GetInt()),sampleRate) ;
	float releaseCoef=coefFromTime(SynthInstrument::TimeFromParam(release_->GetInt()),sampleRate) ;
	float quickReleaseCoef=coefFromTime(MACRO_QUICK_RELEASE_SECONDS,sampleRate) ;
	// Pitch, filter and timbre/color envelopes move once per control block
	float blockRate=sampleRate/MACRO_BLOCK ;
	float pitchCoef=coefFromTime(SynthInstrument::TimeFromParam(pitchDec_->GetInt()),blockRate) ;
	float envCoef=coefFromTime(SynthInstrument::TimeFromParam(envDec_->GetInt()),blockRate) ;
	float tcCoef=coefFromTime(SynthInstrument::TimeFromParam(tcDecay_->GetInt()),blockRate) ;
	float fadeInc=1.0f/(MACRO_FADE_SECONDS*sampleRate) ;
	int filterType=filterType_->GetInt() ;
	int lfoDest=lfoDest_->GetInt() ;
	float lfoInc=SynthInstrument::LfoRateFromParam(lfoRate_->GetInt())*MACRO_BLOCK/sampleRate ;
	float lfoAmt=lfoAmt_->GetInt()/255.0f ;
	float tEnv=tEnv_->GetInt()/(float)MACRO_ENV_MAX ;
	float cEnv=cEnv_->GetInt()/(float)MACRO_ENV_MAX ;
	int glide=glide_->GetInt() ;
	float glideCoef=glide>0?coefFromTime(SynthInstrument::TimeFromParam(glide),blockRate):0.0f ;

	float ampMod=1.0f ;
	float driveGain=1.0f ;
	float gainL=0.0f ;
	float gainR=0.0f ;
	static float sendBuffer[SENDFX_MAX_FRAMES*2] ;
	float reverbSend=reverb_->GetInt()/255.0f ;
	float delaySend=delay_->GetInt()/255.0f ;
	float chorusSend=chorus_->GetInt()/255.0f ;
	bool sending=(reverbSend>0.0f || delaySend>0.0f || chorusSend>0.0f) ;
	int rendered=0 ;

	fixed *out=buffer ;
	for (int i=0;i<size;i++) {

		// Control rate: pitch, timbre/color, filter, lfo, command ramps
		if ((i%MACRO_BLOCK)==0) {
			if (--v.krateCount_<=0) {
				v.krateCount_=MACRO_KRATE/MACRO_BLOCK ;
				processUpdaters(v,false) ;
			}
			float lfo=lfoSin(v.lfoPhase_) ;
			v.lfoPhase_+=lfoInc ;
			v.lfoPhase_-=(float)floor(v.lfoPhase_) ;

			if (glide>0) {
				v.glideNote_=v.midiNote_+(v.glideNote_-v.midiNote_)*glideCoef ;
			} else {
				v.glideNote_=(float)v.midiNote_ ;
			}
			float note=v.glideNote_+tune+pitchEnvAmt*v.pitchEnv_ ;
			if (lfoDest==MLD_PITCH) note+=lfo*lfoAmt*2.0f ;
			float speed=fp2fl(v.speed_) ;
			if (speed>0.0f && speed!=1.0f) {
				note+=12.0f*(float)(log(speed)*1.4426950408889634) ;
			}
			// The model's pitch: MIDI note in 1/128 semitones at 96 kHz
			int pitch=(int)(note*128.0f+0.5f) ;
			if (pitch<0) pitch=0 ;
			if (pitch>16383) pitch=16383 ;
			v.osc_->set_pitch((int16_t)pitch) ;

			float timbre=fp2fl(v.timbre_)+tEnv*v.tcEnv_ ;
			float color=fp2fl(v.color_)+cEnv*v.tcEnv_ ;
			if (lfoDest==MLD_TIMBRE) timbre+=lfo*lfoAmt*0.5f ;
			if (lfoDest==MLD_COLOR) color+=lfo*lfoAmt*0.5f ;
			v.osc_->set_parameters((int16_t)(clamp01(timbre)*32767.0f),
			                       (int16_t)(clamp01(color)*32767.0f)) ;

			ampMod=1.0f ;
			if (lfoDest==MLD_VOLUME) {
				ampMod=1.0f-lfoAmt*(0.5f+0.5f*lfo) ;
			}
			if (filterType!=SFT_OFF) {
				float cut=fp2fl(v.cutoff_)+envAmt*v.filterEnv_ ;
				if (lfoDest==MLD_CUTOFF) cut+=lfo*lfoAmt*0.5f ;
				updateFilter(v,cut,fp2fl(v.reso_),sampleRate) ;
			}
			v.pitchEnv_*=pitchCoef ;
			v.filterEnv_*=envCoef ;
			v.tcEnv_*=tcCoef ;

			driveGain=1.0f+v.drive_/255.0f*8.0f ;

			float vol=fp2fl(v.volume_) ;
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
			case MSS_ATTACK:
				v.level_+=attackInc ;
				if (v.level_>=1.0f) {
					v.level_=1.0f ;
					v.stage_=MSS_DECAY ;
				}
				break ;
			case MSS_DECAY:
				v.level_=sustain+(v.level_-sustain)*decayCoef ;
				if (sustain<=0.0f && v.level_<MACRO_SILENCE) {
					v.level_=0.0f ;
					v.stage_=MSS_OFF ;
				}
				break ;
			case MSS_RELEASE:
				v.level_*=v.fastRelease_?quickReleaseCoef:releaseCoef ;
				if (!(v.level_>=MACRO_SILENCE)) {  // also ends a NaN level
					v.level_=0.0f ;
					v.stage_=MSS_OFF ;
				}
				break ;
			case MSS_FADE:
				v.level_-=fadeInc ;
				if (v.level_<=0.0f) {
					v.level_=0.0f ;
					if (v.pendingStart_) {
						startVoice(channel,v.pendingNote_,v.pendingClean_) ;
					} else {
						v.stage_=MSS_OFF ;
					}
				}
				break ;
			default:
				break ;
		}
		if (v.stage_==MSS_OFF) {
			v.active_=false ;
			break ;
		}

		// Next output sample of the model: polyphase sinc over 96 kHz input
		while (v.fifoCount_<v.readPos_+MACRO_TAPS) {
			fillBlock(v) ;
		}
		const float *x=v.fifo_+v.readPos_ ;
		const float *k=macroKernel+v.phaseAcc_*MACRO_TAPS ;
		float s0=0.0f,s1=0.0f,s2=0.0f,s3=0.0f ;
		for (int t=0;t<MACRO_TAPS;t+=4) {
			s0+=x[t]*k[t] ;
			s1+=x[t+1]*k[t+1] ;
			s2+=x[t+2]*k[t+2] ;
			s3+=x[t+3]*k[t+3] ;
		}
		float sig=(s0+s1)+(s2+s3) ;
		v.phaseAcc_+=M ;
		while (v.phaseAcc_>=L) {
			v.phaseAcc_-=L ;
			v.readPos_++ ;
		}
		if (v.readPos_>=MACRO_FIFO_COMPACT) {
			int keep=v.fifoCount_-v.readPos_ ;
			memmove(v.fifo_,v.fifo_+v.readPos_,keep*sizeof(float)) ;
			v.fifoCount_=keep ;
			v.readPos_=0 ;
		}

		if (v.drive_>0) {
			sig=softSat(sig*driveGain) ;
		}

		// State variable filter (TPT), as the synth
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
		}

		if (sig!=sig || sig>1e6f || sig<-1e6f || v.level_!=v.level_) {
			// NaN / runaway state: silence this voice instead of letting it
			// ring on forever
			brokenVoices_++ ;
			v.stage_=MSS_OFF ;
			v.active_=false ;
			v.level_=0.0f ;
			v.ic1eq_=v.ic2eq_=0.0f ;
			break ;
		}
		sig*=v.level_*ampMod ;
		if (sig>2.0f) sig=2.0f ;
		if (sig<-2.0f) sig=-2.0f ;

		float outL=sig*gainL ;
		float outR=sig*gainR ;
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

/***************************************************************
 Picture for the SOUND page
 ***************************************************************/

// A private oscillator and the voice's 96 kHz stage (degrade, redux), at
// C 3 plus the tune knob; the picture shows the real model's output
MacroPreviewKind MacroInstrument::RenderPreview(float *minv,float *maxv,int columns) {
	static braids::MacroOscillator *osc=0 ;
	if (!osc) {
		osc=new braids::MacroOscillator() ;
		memset((void *)osc,0,sizeof(braids::MacroOscillator)) ;
	}
	if (columns<=0) return MPK_CYCLE ;
	int shape=shape_->GetInt() ;
	if (shape<0 || shape>=MACRO_SHAPE_COUNT) shape=MS_CSAW ;
	MacroPreviewKind kind=GetPreviewKind(shape) ;

	osc->Init() ;
	osc->set_shape((braids::MacroOscillatorShape)shape) ;
	float note=48.0f+tune_->GetInt() ;
	int pitch=(int)(note*128.0f+0.5f) ;
	if (pitch<0) pitch=0 ;
	if (pitch>16383) pitch=16383 ;
	osc->set_pitch((int16_t)pitch) ;
	osc->set_parameters((int16_t)(timbre_->GetInt()/255.0f*32767.0f),
	                    (int16_t)(color_->GetInt()/255.0f*32767.0f)) ;
	osc->Strike() ;
	unsigned int holdInc=holdIncFromDegrade(degrade_->GetInt()) ;
	unsigned short mask=maskFromRedux(redux_->GetInt()) ;
	unsigned int holdPhase=0 ;
	short held=0 ;

	// How much to skip (let the tone settle) and how much to draw
	float freq=440.0f*(float)pow(2.0,(pitch/128.0f-69.0f)/12.0f) ;
	int skip=0,span=0 ;
	switch(kind) {
		case MPK_HIT:
			skip=0 ;
			span=MACRO_OSC_RATE*120/1000 ;
			break ;
		case MPK_TEXTURE:
			skip=MACRO_OSC_RATE*40/1000 ;
			span=MACRO_OSC_RATE*20/1000 ;
			break ;
		default: {
			skip=MACRO_OSC_RATE/10 ;
			span=(int)(2.0f*MACRO_OSC_RATE/(freq>1.0f?freq:1.0f)) ;
			if (span<columns) span=columns ;
			if (span>MACRO_OSC_RATE/5) span=MACRO_OSC_RATE/5 ;
			break ;
		}
	}
	static const uint8_t sync[MACRO_OSC_BLOCK]={0} ;
	int16_t block[MACRO_OSC_BLOCK] ;
	for (int c=0;c<columns;c++) {
		minv[c]=1.0f ;
		maxv[c]=-1.0f ;
	}
	int total=skip+span ;
	int n=0 ;
	float peak=0.0001f ;
	while (n<total) {
		osc->Render(sync,block,MACRO_OSC_BLOCK) ;
		for (int i=0;i<MACRO_OSC_BLOCK && n<total;i++,n++) {
			short s ;
			if (holdInc>=65536) {
				s=(short)(block[i]&mask) ;
			} else {
				holdPhase+=holdInc ;
				if (holdPhase>=65536) {
					holdPhase-=65536 ;
					held=(short)(block[i]&mask) ;
				}
				s=held ;
			}
			if (n<skip) continue ;
			float f=s/32768.0f ;
			int c=(int)((long long)(n-skip)*columns/span) ;
			if (c>=columns) c=columns-1 ;
			if (f<minv[c]) minv[c]=f ;
			if (f>maxv[c]) maxv[c]=f ;
			float a=(float)fabs(f) ;
			if (a>peak) peak=a ;
		}
	}
	// Fill any column no sample landed in, then scale to the box
	for (int c=0;c<columns;c++) {
		if (minv[c]>maxv[c]) {
			minv[c]=(c>0)?minv[c-1]:0.0f ;
			maxv[c]=(c>0)?maxv[c-1]:0.0f ;
		}
		minv[c]/=peak ;
		maxv[c]/=peak ;
	}
	return kind ;
}
