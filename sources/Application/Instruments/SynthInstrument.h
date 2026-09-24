#ifndef _SYNTH_INSTRUMENT_H_
#define _SYNTH_INSTRUMENT_H_

// Native synth instrument: an M8-style "one knob per idea" subtractive/FM
// voice that needs no samples. One voice per tracker channel.

#include "I_Instrument.h"
#include "SRPUpdaters.h"
#include "ModSources.h"
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
	float fa1_,fa2_,fa3_,fk_ ;

	int chord_[SYNTH_MAX_PARTIALS] ;
	int chordCount_ ;

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
	std::vector<I_SRPUpdater *> updaters_ ;
	std::vector<I_SRPUpdater *> activeUpdaters_ ;
} ;

class SynthInstrument ;

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
	// Debug: voices shut off because their state went non-finite
	static int BrokenVoiceCount() ;
	void GetVoiceDebug(int channel,int &stage,float &level) ;
	static int synthBrokenVoices_ ;

	void ApplyPreset(int preset) ;
	void LoadPreset(const char *name) ;
	int GetPreset() ;

	// Offline render of one oscillator cycle for the instrument screen.
	void RenderCycle(float *out,int count) ;
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
	float renderPartial(SynthVoice &v,int p,float inc,float shape,float fmIndex,float fmRatio,int wave) ;
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
	Variable *customName_ ;
} ;

#endif
