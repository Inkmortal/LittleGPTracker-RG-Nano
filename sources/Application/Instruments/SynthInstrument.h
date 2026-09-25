#ifndef _SYNTH_INSTRUMENT_H_
#define _SYNTH_INSTRUMENT_H_

// Native synth instrument: an M8-style "one knob per idea" voice that needs
// no samples. One voice per tracker channel. The "engine" knob picks how the
// tone is made: the original subtractive/FM synth, FM4 (four operators),
// HYPER (a six-note detuned-saw chord) or WAV (a bendable 8-bit oscillator).
// Envelope, filter, LFO, MOD and MIX pages are shared by every engine.

#include "I_Instrument.h"
#include "SynthEngines.h"
#include "SRPUpdaters.h"
#include "ModSources.h"
#include "InstrumentEQ.h"
#include "Application/Model/Song.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/Variable.h"
#include <vector>

enum SynthWave {
	SW_SINE=0,
	SW_TRIANGLE,
	SW_SAW,
	SW_PULSE,
	SW_SUPERSAW,
	SW_NOISE,
	SW_METAL,
	SW_LAST
};

enum SynthFilterType {
	SFT_LOWPASS=0,
	SFT_HIGHPASS,
	SFT_BANDPASS,
	SFT_OFF,
	SFT_LAST
};

enum SynthLfoDest {
	SLD_PITCH=0,
	SLD_CUTOFF,
	SLD_VOLUME,
	SLD_SHAPE,
	SLD_LAST
};

#define SYNTH_CHORD_COUNT 10
#define SYNTH_MAX_PARTIALS 5
#define SYNTH_FM_RATIO_COUNT 16

#define SYP_PRESET    MAKE_FOURCC('S','Y','P','R')
#define SYP_WAVE      MAKE_FOURCC('S','Y','W','V')
#define SYP_SHAPE     MAKE_FOURCC('S','Y','S','H')
#define SYP_SUB       MAKE_FOURCC('S','Y','S','B')
#define SYP_NOISE     MAKE_FOURCC('S','Y','N','Z')
#define SYP_FMAMT     MAKE_FOURCC('S','Y','F','M')
#define SYP_FMRATIO   MAKE_FOURCC('S','Y','F','R')
#define SYP_CHORD     MAKE_FOURCC('S','Y','C','H')
#define SYP_TUNE      MAKE_FOURCC('S','Y','T','U')
#define SYP_FINE      MAKE_FOURCC('S','Y','F','N')
#define SYP_ATTACK    MAKE_FOURCC('S','Y','A','T')
#define SYP_DECAY     MAKE_FOURCC('S','Y','D','C')
#define SYP_SUSTAIN   MAKE_FOURCC('S','Y','S','U')
#define SYP_RELEASE   MAKE_FOURCC('S','Y','R','L')
#define SYP_PITCHENV  MAKE_FOURCC('S','Y','P','E')
#define SYP_PITCHDEC  MAKE_FOURCC('S','Y','P','D')
#define SYP_GLIDE     MAKE_FOURCC('S','Y','G','L')
#define SYP_FILTTYPE  MAKE_FOURCC('S','Y','F','T')
#define SYP_CUTOFF    MAKE_FOURCC('S','Y','C','U')
#define SYP_RESO      MAKE_FOURCC('S','Y','R','S')
#define SYP_ENVAMT    MAKE_FOURCC('S','Y','E','A')
#define SYP_ENVDEC    MAKE_FOURCC('S','Y','E','D')
#define SYP_DRIVE     MAKE_FOURCC('S','Y','D','R')
#define SYP_LFODEST   MAKE_FOURCC('S','Y','L','D')
#define SYP_LFORATE   MAKE_FOURCC('S','Y','L','R')
#define SYP_LFOAMT    MAKE_FOURCC('S','Y','L','A')
#define SYP_REVERB    MAKE_FOURCC('S','Y','R','V')
#define SYP_DELAY     MAKE_FOURCC('S','Y','D','L')
#define SYP_CHORUS    MAKE_FOURCC('S','Y','C','O')
#define SYP_VOLUME    MAKE_FOURCC('V','O','L','M')
#define SYP_PAN       MAKE_FOURCC('P','A','N','_')
#define SYP_TABLE     MAKE_FOURCC('T','A','B','L')
#define SYP_TABLEAUTO MAKE_FOURCC('T','B','L','A')
#define SYP_ENGINE    MAKE_FOURCC('S','Y','E','N')

// FM4: algorithm, then per operator (A..D) shape, ratio, level, feedback,
// attack, decay, sustain
#define FM4P_ALGO     MAKE_FOURCC('F','4','X','A')
#define FM4P_SHAPE    'S'
#define FM4P_RATIO    'R'
#define FM4P_LEVEL    'L'
#define FM4P_FEEDBACK 'B'
#define FM4P_ATTACK   'A'
#define FM4P_DECAY    'D'
#define FM4P_SUSTAIN  'U'
#define FM4_ID(op,param) MAKE_FOURCC('F','4',('A'+(op)),(param))
#define FM4_PARAM_COUNT 7

// HYPER: chord picker, six note offsets, shift, swarm, width, sub, scale
#define HYP_CHORD     MAKE_FOURCC('H','Y','C','H')
#define HYP_NOTE(n)   MAKE_FOURCC('H','Y','N',('1'+(n)))
#define HYP_SHIFT     MAKE_FOURCC('H','Y','S','H')
#define HYP_SWARM     MAKE_FOURCC('H','Y','S','W')
#define HYP_WIDTH     MAKE_FOURCC('H','Y','W','D')
#define HYP_SUB       MAKE_FOURCC('H','Y','S','B')
#define HYP_SCALE     MAKE_FOURCC('H','Y','S','C')

// WAV: shape, size, mult, warp, mirror, and the drive stage's limiter
#define WVP_SHAPE     MAKE_FOURCC('W','V','S','H')
#define WVP_SIZE      MAKE_FOURCC('W','V','S','Z')
#define WVP_MULT      MAKE_FOURCC('W','V','M','U')
#define WVP_WARP      MAKE_FOURCC('W','V','W','P')
#define WVP_MIRROR    MAKE_FOURCC('W','V','M','I')
#define WVP_LIMIT     MAKE_FOURCC('W','V','L','M')

struct SynthVoice {
	bool active_ ;          // producing sound
	int stage_ ;            // amp envelope stage
	float level_ ;          // amp envelope level
	bool pendingStart_ ;    // waiting for the steal fade to finish
	unsigned char midiNote_ ;
	unsigned char pendingNote_ ;
	bool pendingClean_ ;
	bool fastRelease_ ;     // transport stop: short release

	float phase_[SYNTH_MAX_PARTIALS][3] ;
	float fmPhase_[SYNTH_MAX_PARTIALS] ;
	float fbLast_[SYNTH_MAX_PARTIALS] ;
	float subPhase_ ;
	float metalPhase_[6] ;
	unsigned int noiseState_ ;
	float noiseHold_ ;
	float noiseCounter_ ;

	float pitchEnv_ ;
	float filterEnv_ ;
	float lfoPhase_ ;
	float glideNote_ ;
	bool hasPlayed_ ;

	float ic1eq_ ;
	float ic2eq_ ;
	float ic1eqR_ ;         // right channel filter state (stereo engines)
	float ic2eqR_ ;
	float fa1_,fa2_,fa3_,fk_ ;

	int chord_[SYNTH_MAX_PARTIALS] ;
	int chordCount_ ;

	// Engine oscillators (one set per chord partial where it applies)
	Fm4Ops fm_[SYNTH_MAX_PARTIALS] ;
	HyperOsc hyper_ ;
	WavOsc wav_[SYNTH_MAX_PARTIALS] ;
	bool hyperCmdChord_ ;          // CHRD replaced the six notes
	int hyperCmd_[HYPER_NOTES] ;
	int hyperSemis_[HYPER_NOTES] ;  // notes in use (after the scale)
	int hyperKey_ ;                 // what hyperSemis_/hyperRatio_ were made from
	float hyperRatio_[HYPER_NOTES] ;

	// Command-driven state, same units as SampleInstrument
	fixed baseVolume_ ;
	fixed volume_ ;
	fixed basePan_ ;
	fixed pan_ ;
	fixed baseCutoff_ ;
	fixed cutoff_ ;
	fixed baseReso_ ;
	fixed reso_ ;
	fixed speed_ ;          // pitch multiplier from ARPG/PTCH/LEGA/PFIN
	unsigned char drive_ ;

	int krateCount_ ;
	bool retrig_ ;
	int retrigLoop_ ;
	int retrigCount_ ;

	VolumeRamp volumeRamp_ ;
	Panner panner_ ;
	FCRamp cutRamp_ ;
	FRRamp resRamp_ ;
	LinSpeedRamp speedRamp_ ;
	LogSpeedRamp legato_ ;
	LogSpeedRamp pfin_ ;
	Arp arp_ ;
	ModSource mods_[MOD_SLOT_COUNT] ;
	float modVolScale_ ;           // MOD envelopes on volume
	float modExtra_[RUX_LAST] ;    // MOD drive/shape/FM/noise/sends
	std::vector<I_SRPUpdater *> updaters_ ;
	std::vector<I_SRPUpdater *> activeUpdaters_ ;
} ;

class SynthInstrument ;

// A knob whose change the instrument reacts to (engine, hyper chord)
class SynthHookVariable: public Variable {
public:
	SynthHookVariable(SynthInstrument *owner,int hook,const char *name,FourCC id,
	                  const char *const *list,int size,int index) ;
	SynthHookVariable(SynthInstrument *owner,int hook,const char *name,FourCC id,
	                  int value) ;
protected:
	virtual void onChange() ;
private:
	SynthInstrument *owner_ ;
	int hook_ ;
} ;

// Selecting a preset rewrites the other parameters. It is saved first so a
// restored project applies the preset, then overrides it with saved values.
class SynthPresetVariable: public Variable {
public:
	SynthPresetVariable(SynthInstrument *owner,const char *name,FourCC id,
	                    char **list,int size,int index) ;
protected:
	virtual void onChange() ;
private:
	SynthInstrument *owner_ ;
} ;

class SynthInstrument: public I_Instrument {
public:
	SynthInstrument() ;
	virtual ~SynthInstrument() ;

	virtual bool Init() ;
	virtual bool Start(int channel,unsigned char note,bool retrigger=true) ;
	virtual void Stop(int channel) ;
	virtual void OnStart() ;
	virtual bool Render(int channel,fixed *buffer,int size,bool updateTick) ;
	virtual bool IsInitialized() { return true ; } ;
	virtual bool IsEmpty() { return false ; } ;
	virtual InstrumentType GetType() { return IT_SYNTH ; } ;
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
	virtual InstrumentMods *GetMods() { return &mods_ ; } ;
	// Debug: voices shut off because their state went non-finite
	static int BrokenVoiceCount() ;
	void GetVoiceDebug(int channel,int &stage,float &level) ;
	static int synthBrokenVoices_ ;

	void ApplyPreset(int preset) ;
	void LoadPreset(const char *name) ;
	int GetPreset() ;
	int GetEngine() ;
	// A hooked knob changed (engine, hyper chord or note)
	void OnHook(int hook) ;
	// Presets of one engine sit together in the list: first..last
	static void GetPresetRange(int engine,int &first,int &last) ;
	static int GetPresetEngine(int preset) ;
	static const char *GetEngineName(int engine) ;
	// The six hyper notes this instrument plays on a root note (after the
	// song's scale when "scale" is on), in semitones above the root
	void GetHyperNotes(int root,int *semis) ;

	// Offline render of one oscillator cycle for the instrument screen.
	void RenderCycle(float *out,int count) ;
	// Offline render of 'cycles' cycles of the current engine (FM4, WAV
	// and HYPER draw what they really play), peak-normalised
	void RenderPreview(float *out,int count,float cycles) ;
	// Envelope shape helpers for the instrument screen (seconds).
	static float TimeFromParam(int value) ;
	static float LfoRateFromParam(int value) ;
	static float CutoffHzFromParam(float value) ;

	static int GetPresetCount() ;
	static const char *GetPresetName(int index) ;

private:
	void startVoice(int channel,unsigned char note,bool cleanStart) ;
	void updateFilter(SynthVoice &v,float cutoff,float reso,float sampleRate) ;
	void processUpdaters(SynthVoice &v,bool tick) ;
	void applyUpdaters(SynthVoice &v) ;
	void startMods(SynthVoice &v,int channel,unsigned char note) ;
	float renderPartial(SynthVoice &v,int p,float inc,float shape,float fmIndex,float fmRatio,int wave) ;
	void setupFm4(Fm4Params &p,float sampleRate) ;
	void updateHyperRatios(SynthVoice &v) ;
	void setupHyper(const float *ratio,HyperParams &p,float baseInc,int swarm) ;
	void resetEngineVariables() ;
	void syncHyperChordName() ;
	int getInt(FourCC id) ;
	void removeUpdater(SynthVoice &v,I_SRPUpdater *u) ;

	SynthVoice voices_[SONG_CHANNEL_COUNT] ;
	TableSaveState tableState_ ;
	bool applyingPreset_ ;

	SynthPresetVariable *preset_ ;
	Variable *wave_ ;
	Variable *shape_ ;
	Variable *sub_ ;
	Variable *noise_ ;
	Variable *fmAmt_ ;
	Variable *fmRatio_ ;
	Variable *chord_ ;
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
	Variable *reverb_ ;
	Variable *delay_ ;
	Variable *chorus_ ;
	Variable *volume_ ;
	Variable *pan_ ;
	Variable *table_ ;
	Variable *tableAuto_ ;
	InstrumentMods mods_ ;
	InstrumentEQ eq_ ;
	Variable *customName_ ;

	SynthHookVariable *engine_ ;
	Variable *fmAlgo_ ;
	Variable *fmOp_[FM4_OPS][FM4_PARAM_COUNT] ;
	SynthHookVariable *hyperChord_ ;
	SynthHookVariable *hyperNote_[HYPER_NOTES] ;
	Variable *hyperShift_ ;
	Variable *hyperSwarm_ ;
	Variable *hyperWidth_ ;
	Variable *hyperSub_ ;
	Variable *hyperScale_ ;
	Variable *wavShape_ ;
	Variable *wavSize_ ;
	Variable *wavMult_ ;
	Variable *wavWarp_ ;
	Variable *wavMirror_ ;
	Variable *wavLimit_ ;
	std::vector<Variable *> engineVars_ ;
} ;

#endif
