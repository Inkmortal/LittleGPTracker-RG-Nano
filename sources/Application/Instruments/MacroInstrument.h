#ifndef _MACRO_INSTRUMENT_H_
#define _MACRO_INSTRUMENT_H_

// Macro synth: the M8's MacroSynth idea - one "shape" (a whole synthesis
// model: analog waves, FM, formants, physical models, drums, wavetables,
// noise) played through two knobs, timbre and color. The oscillator is a
// port of Emilie Gillet's Braids macro oscillator (MIT, see NOTICE.md,
// sources/Externals/Braids). It runs at its native 96 kHz in 24-sample
// blocks; each voice resamples to the mixer rate with a polyphase
// windowed-sinc filter, so tuning, formants and drum decays are exactly
// the module's.
//
// Around it sits the same voice as the native synth: ADSR amp envelope,
// pitch envelope, glide, state-variable filter with its envelope, drive,
// the two MOD slots, pan/volume and the reverb/delay/chorus sends. Those
// knobs share the synth's variable ids (SYP_*), so the ENV, FILTER, MOD
// and MIX pages of the instrument screen are the synth's pages.

#include "I_Instrument.h"
#include "SRPUpdaters.h"
#include "ModSources.h"
#include "SynthInstrument.h"
#include "Application/Model/Song.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/Variable.h"
#include <vector>

namespace braids {
class MacroOscillator ;
}

// Shapes, in the module's order (index = braids::MacroOscillatorShape)
#define MACRO_SHAPE_COUNT 47
enum MacroShape {
	MS_CSAW=0,MS_MORPH,MS_SAWSQUARE,MS_FOLD,MS_BUZZ,
	MS_SQUARESUB,MS_SAWSUB,MS_SQUARESYNC,MS_SAWSYNC,
	MS_TRIPLESAW,MS_TRIPLESQUARE,MS_TRIPLETRI,MS_TRIPLESINE,MS_RING,
	MS_SWARM,MS_COMB,MS_TOY,
	MS_ZLPF,MS_ZPKF,MS_ZBPF,MS_ZHPF,MS_VOSIM,MS_VOWEL,MS_VFOF,
	MS_HARMONICS,
	MS_FM,MS_FBFM,MS_CHAOTICFM,
	MS_PLUCK,MS_BOWED,MS_BLOWN,MS_FLUTE,MS_BELL,MS_DRUM,MS_KICK,MS_CYMBAL,MS_SNARE,
	MS_WTBL,MS_WMAP,MS_WLINE,MS_WTX4,
	MS_NOISE,MS_TWINQ,MS_CLKN,MS_CLOUD,MS_PARTICLE,
	MS_QPSK
} ;

// What the macro's own LFO (MOTION page) moves
enum MacroLfoDest {
	MLD_PITCH=0,
	MLD_CUTOFF,
	MLD_VOLUME,
	MLD_TIMBRE,
	MLD_COLOR,
	MLD_LAST
} ;

// Macro-only knobs. Everything shared with the synth uses its SYP_* ids
// (preset, tune, fine, envelopes, filter, drive, lfo rate/depth, mix,
// table), so the same pages, helper texts and FX screen counters apply.
#define MCP_SHAPE    MAKE_FOURCC('M','C','S','H')
#define MCP_TIMBRE   MAKE_FOURCC('M','C','T','I')
#define MCP_COLOR    MAKE_FOURCC('M','C','C','O')
#define MCP_DEGRADE  MAKE_FOURCC('M','C','D','G')
#define MCP_REDUX    MAKE_FOURCC('M','C','R','D')
#define MCP_TENV     MAKE_FOURCC('M','C','T','E')
#define MCP_CENV     MAKE_FOURCC('M','C','C','E')
#define MCP_TCDECAY  MAKE_FOURCC('M','C','E','D')
#define MCP_LFODEST  MAKE_FOURCC('M','C','L','D')

// Timbre/color envelope amounts: -7F..+7F, full = the whole knob range
#define MACRO_ENV_MAX 127

// Resampler: taps per output sample (at 96 kHz input)
#define MACRO_TAPS 48
// Input FIFO: compacted once the read position passes this many samples
#define MACRO_FIFO_COMPACT 512
#define MACRO_FIFO_SIZE (MACRO_FIFO_COMPACT+MACRO_TAPS+64)

struct MacroVoice {
	braids::MacroOscillator *osc_ ;
	int shape_ ;            // shape the oscillator is set to
	bool active_ ;
	int stage_ ;
	float level_ ;
	bool pendingStart_ ;
	unsigned char midiNote_ ;
	unsigned char pendingNote_ ;
	bool pendingClean_ ;
	bool fastRelease_ ;
	bool strike_ ;          // restrike the model before its next block

	// 96 kHz stage: degrade (sample and hold) and redux (bit mask)
	unsigned int holdPhase_ ;
	unsigned int holdInc_ ;  // 16.16, 1.0 = every sample
	short held_ ;
	unsigned short bitMask_ ;

	// Resampler: 96 kHz samples waiting, and where the next output reads
	float fifo_[MACRO_FIFO_SIZE] ;
	int fifoCount_ ;
	int readPos_ ;
	int phaseAcc_ ;          // 0..L-1: fractional read position * L

	float pitchEnv_ ;
	float filterEnv_ ;
	float tcEnv_ ;
	float lfoPhase_ ;
	float glideNote_ ;
	bool hasPlayed_ ;

	float ic1eq_,ic2eq_ ;
	float fa1_,fa2_,fa3_,fk_ ;

	// Command-driven state, same units as the synth
	fixed baseVolume_,volume_ ;
	fixed basePan_,pan_ ;
	fixed baseCutoff_,cutoff_ ;
	fixed baseReso_,reso_ ;
	fixed baseTimbre_,timbre_ ;   // 0..1 (TIMB ramps it)
	fixed baseColor_,color_ ;     // 0..1 (COLR ramps it)
	fixed speed_ ;
	unsigned char drive_ ;

	int krateCount_ ;
	bool retrig_ ;
	int retrigLoop_ ;
	int retrigCount_ ;

	VolumeRamp volumeRamp_ ;
	Panner panner_ ;
	FCRamp cutRamp_ ;
	FRRamp resRamp_ ;
	FBMixRamp timbreRamp_ ;
	FBTunRamp colorRamp_ ;
	LinSpeedRamp speedRamp_ ;
	LogSpeedRamp legato_ ;
	LogSpeedRamp pfin_ ;
	Arp arp_ ;
	ModSource mods_[MOD_SLOT_COUNT] ;
	std::vector<I_SRPUpdater *> updaters_ ;
	std::vector<I_SRPUpdater *> activeUpdaters_ ;
} ;

class MacroInstrument ;

// Picking a preset rewrites the other knobs (saved first, like the synth's)
class MacroPresetVariable: public Variable {
public:
	MacroPresetVariable(MacroInstrument *owner,const char *name,FourCC id,
	                    char **list,int size,int index) ;
protected:
	virtual void onChange() ;
private:
	MacroInstrument *owner_ ;
} ;

// What the SOUND page picture shows for a shape
enum MacroPreviewKind {
	MPK_CYCLE=0,   // steady tone: two cycles
	MPK_HIT,       // struck/percussive: the first 120 ms after a note
	MPK_TEXTURE    // noise/granular: a 20 ms stretch
} ;

class MacroInstrument: public I_Instrument {
public:
	MacroInstrument() ;
	virtual ~MacroInstrument() ;

	virtual bool Init() ;
	virtual bool Start(int channel,unsigned char note,bool retrigger=true) ;
	virtual void Stop(int channel) ;
	virtual void OnStart() ;
	virtual bool Render(int channel,fixed *buffer,int size,bool updateTick) ;
	virtual bool IsInitialized() { return true ; } ;
	virtual bool IsEmpty() { return false ; } ;
	virtual InstrumentType GetType() { return IT_MACRO ; } ;
	virtual const char *GetName() ;
	virtual void ProcessCommand(int channel,FourCC cc,ushort value) ;
	virtual void Purge() ;
	virtual int GetTable() ;
	virtual bool GetTableAutomation() ;
	virtual void GetTableState(TableSaveState &state) ;
	virtual void SetTableState(TableSaveState &state) ;
	virtual bool IsReleasing(int channel) ;
	virtual void StopQuickly(int channel) ;
	virtual void AllNotesOff() ;

	void GetVoiceDebug(int channel,int &stage,float &level) ;
	static int BrokenVoiceCount() ;

	void ApplyPreset(int preset) ;
	void LoadPreset(const char *name) ;
	int GetPreset() ;
	static int GetPresetCount() ;
	static const char *GetPresetName(int index) ;

	// Shape names and what timbre/color do on each (for the screen)
	static const char *GetShapeName(int shape) ;
	static const char *GetTimbreHelp(int shape) ;
	static const char *GetColorHelp(int shape) ;
	static MacroPreviewKind GetPreviewKind(int shape) ;
	static float LfoRateFromParam(int value) { return SynthInstrument::LfoRateFromParam(value) ; } ;

	// Offline picture of the current shape/timbre/color/degrade/redux:
	// per-column min/max, -1..1. UI thread only.
	MacroPreviewKind RenderPreview(float *minv,float *maxv,int columns) ;

	// Resampler geometry (96 kHz in, mixer rate out): L phases, M input
	// samples per L outputs
	static int ResampleL() ;
	static int ResampleM() ;

private:
	void startVoice(int channel,unsigned char note,bool cleanStart) ;
	void updateFilter(MacroVoice &v,float cutoff,float reso,float sampleRate) ;
	void processUpdaters(MacroVoice &v,bool tick) ;
	void removeUpdater(MacroVoice &v,I_SRPUpdater *u) ;
	void fillBlock(MacroVoice &v) ;
	int getInt(FourCC id) ;
	static void setupResampler(float outRate) ;

	MacroVoice voices_[SONG_CHANNEL_COUNT] ;
	TableSaveState tableState_ ;
	bool applyingPreset_ ;
	static int brokenVoices_ ;

	MacroPresetVariable *preset_ ;
	Variable *shape_ ;
	Variable *timbre_ ;
	Variable *color_ ;
	Variable *degrade_ ;
	Variable *redux_ ;
	Variable *tune_ ;
	Variable *fine_ ;
	Variable *attack_ ;
	Variable *decay_ ;
	Variable *sustain_ ;
	Variable *release_ ;
	Variable *pitchEnv_ ;
	Variable *pitchDec_ ;
	Variable *glide_ ;
	Variable *filterType_ ;
	Variable *cutoff_ ;
	Variable *reso_ ;
	Variable *envAmt_ ;
	Variable *envDec_ ;
	Variable *drive_ ;
	Variable *lfoDest_ ;
	Variable *lfoRate_ ;
	Variable *lfoAmt_ ;
	Variable *tEnv_ ;
	Variable *cEnv_ ;
	Variable *tcDecay_ ;
	Variable *reverb_ ;
	Variable *delay_ ;
	Variable *chorus_ ;
	Variable *volume_ ;
	Variable *pan_ ;
	Variable *table_ ;
	Variable *tableAuto_ ;
	InstrumentMods mods_ ;
	Variable *customName_ ;
} ;

#endif
