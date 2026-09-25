#ifndef _MOD_SOURCES_H_
#define _MOD_SOURCES_H_

// Four modulation slots per instrument, after the M8's instrument
// modulation view. Each slot is an envelope (AHD, ADSR, drum, trig), an LFO
// or key tracking aimed at one sound parameter. A running slot is an
// I_SRPUpdater, so it rides the same k-rate path as the VOLM/FCUT/PTCH
// command ramps in both the sampler and the synth.
//
// Envelope segments run from the level they start at to their target
// through a phase 0..1 with a shaped curve, the way Mutable Instruments'
// Braids envelope does (MIT, see NOTICE.md): a release or a retrigger
// always starts where the level is, so nothing jumps.

#include "I_SRPUpdater.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/Variable.h"
#include "Foundation/Variables/VariableContainer.h"
#include "Services/Audio/AudioModule.h"
#include <vector>

#define MOD_SLOT_COUNT 4
#define MOD_PARAM_COUNT 4

enum ModType {
	MT_OFF=0,
	MT_AHD,       // attack, hold, decay back to nothing
	MT_ADSR,      // attack, decay to sustain, release after note-off / KILL
	MT_DRUM,      // sharp peak, a dip, then the body holds and decays
	MT_LFO,       // repeating shape
	MT_TRIG,      // AHD fired by notes on another track
	MT_TRACK,     // the note number: low notes one value, high notes another
	MT_DECAY,     // (first release) full at the note start, falls away
	MT_SWELL,     // (first release) rises from nothing, then holds
	MT_LAST
} ;

// Every destination a slot can move. Each instrument kind offers the ones
// that make sense for it (see InstrumentMods::Create).
enum ModDest {
	MD_VOLUME=0,
	MD_CUTOFF,
	MD_RESO,
	MD_PITCH,
	MD_PAN,
	MD_FINE,
	MD_DRIVE,
	MD_CRUSH,
	MD_SHAPE,
	MD_FM,
	MD_NOISE,
	MD_FBMIX,
	MD_FBTUNE,
	MD_START,
	MD_LOOP,
	MD_REVERB,
	MD_DELAY,
	MD_CHORUS,
	MD_TIMBRE,    // macro synth
	MD_COLOR,
	MD_LAST
} ;

enum ModLfoShape {
	MLS_TRI=0,
	MLS_SINE,
	MLS_RAMP_DOWN,
	MLS_RAMP_UP,
	MLS_EXP_DOWN,
	MLS_EXP_UP,
	MLS_SQUARE_DOWN,
	MLS_SQUARE_UP,
	MLS_RANDOM,    // sample and hold: a new level every cycle
	MLS_DRUNK,     // random walk
	MLS_LAST
} ;

enum ModLfoTrig {
	MLT_FREE=0,    // keeps running, notes don't restart it
	MLT_RETRIG,    // restarts with every note
	MLT_HOLD,      // one cycle per note, then holds its last level
	MLT_ONCE,      // one cycle per note, then back to its start level
	MLT_LAST
} ;

enum ModInstrumentKind {
	MIK_SAMPLE=0,
	MIK_SYNTH,
	MIK_MACRO
} ;

// Variable ids: 'M' slot 'T'ype / 'D'est / 'A'mount / 'P'aram 1..4.
// Slots 1 and 2 keep the ids of the first release (their rate became P1).
#define MOD_TYPE_ID(s)   (MAKE_FOURCC('M',('1'+(s)),'T','Y'))
#define MOD_DEST_ID(s)   (MAKE_FOURCC('M',('1'+(s)),'D','S'))
#define MOD_AMOUNT_ID(s) (MAKE_FOURCC('M',('1'+(s)),'A','M'))
#define MOD_PARAM_ID(s,p) (MAKE_FOURCC('M',('1'+(s)),'P',('1'+(p))))

#define MOD_AMOUNT_MAX 127
// Full amount on pitch moves this many semitones, on fine pitch one
#define MOD_PITCH_RANGE 24.0f
#define MOD_FINE_RANGE 1.0f

// What a parameter slot of a type means (drives the screen and the engine)
enum ModParamKind {
	MPK_NONE=0,
	MPK_TIME,       // 00 = 0 ms, 01..FF = 1 ms .. 10 s
	MPK_LEVEL,      // 00..FF = 0..100 %
	MPK_RATE,       // 00..FF = 0.05 .. 50 Hz
	MPK_SHAPE,      // ModLfoShape
	MPK_TRIGMODE,   // ModLfoTrig
	MPK_PEAK,       // drum envelope transient: dip depth and time
	MPK_TRACKSRC,   // 0..7 = track 1..8
	MPK_NOTE,       // note number
	MPK_SIGNED,     // -127..127
	MPK_LEGACYENV   // first release decay/swell: higher = faster
} ;

struct ModParamDef {
	const char *label_ ;   // 6 letters or less, on the screen
	int kind_ ;
	int min_ ;
	int max_ ;
	int default_ ;
	int bigStep_ ;
} ;

// The settings of one slot as the engine reads them
struct ModSlotSettings {
	int type_ ;
	int dest_ ;       // ModDest
	int amount_ ;
	int param_[MOD_PARAM_COUNT] ;
	bool operator!=(const ModSlotSettings &o) const ;
} ;

class InstrumentMods ;

// One running slot on one voice
class ModSource: public I_SRPUpdater {
public:
	ModSource() ;
	virtual ~ModSource() {} ;
	// Starts the slot for a new note (reads the instrument's settings)
	void Start(InstrumentMods *owner,int slot,float krateHz,int note,
	           int channel,unsigned int seed) ;
	// Same, from explicit settings (tests, previews)
	void Start(const ModSlotSettings &settings,float krateHz,int note,
	           int channel,unsigned int seed) ;
	// Envelopes start over (RTRG); LFOs that follow notes restart too
	void Retrigger() ;
	// Note-off / KILL: an ADSR goes to its release
	void NoteOff() ;
	virtual void Trigger(bool tableTick) ;
	virtual void UpdateSRP(struct RUParams &rup) ;

	int GetType() { return s_.type_ ; } ;
	int GetDest() { return s_.dest_ ; } ;
	float GetAmount() { return amount_ ; } ;
	// Envelope/LFO level: envelopes 0..1, LFO -1..1, tracking -1..1
	float GetLevel() { return level_ ; } ;
	// An ADSR that finished its release (or any envelope that is done)
	bool IsDone() ;
	// Volume depth multiplier this slot applies (1 = none)
	float GetVolumeScale() ;

	// Parameter curves, shared with the screen
	static float TimeFromParam(int value) ;       // seconds, 0 at 00
	static float RateFromParam(int value) ;       // LFO Hz
	static float LegacyEnvTime(int rate) ;        // first release decay/swell
	static float PeakTime(int value) ;            // drum transient, seconds
	static float PeakDip(int value) ;             // drum dip level 0..1
	// LFO level for a phase 0..1, -1..1 (random/drunk use 'held')
	static float LfoShape(int shape,float phase,float held) ;
	static bool IsEnvelope(int type) ;
	// Key tracking level (-1..1) of a note
	static float TrackLevel(const ModSlotSettings &s,int note) ;
	// Offline curve of one slot for the screen: 'count' levels over
	// 'window' seconds from a note start, note-off at 'gate' seconds
	// (ADSR only, <0 = never)
	static void Preview(const ModSlotSettings &s,float window,float gate,
	                    int note,float *out,int count) ;
	// Channel note starts, for trig envelopes on other tracks
	static void NoteStarted(int channel) ;
private:
	void setup(bool noteStart) ;
	void enterStage(int stage) ;
	void advanceEnvelope(float dt) ;
	void advanceLfo() ;
	void computeOutput() ;
	float curve(int stage,float phase) ;
	float trackLevel() ;
	unsigned int nextRandom() ;

	InstrumentMods *owner_ ;
	int slot_ ;
	ModSlotSettings s_ ;
	float krate_ ;
	int note_ ;
	int channel_ ;
	float amount_ ;   // -1..1
	float level_ ;
	// envelope
	int stage_ ;
	float segPhase_ ;
	float segInc_ ;
	float segFrom_ ;
	float segTo_ ;
	int trigSeen_ ;
	// lfo
	float phase_ ;
	float step_ ;
	float held_ ;
	bool cycleDone_ ;
	unsigned int random_ ;
	// legacy decay/swell
	float legacyStep_ ;
	// output
	fixed offset_ ;
	fixed speedFactor_ ;
	float volScale_ ;
	float extra_ ;
	static int noteStarts_[8] ;
} ;

// A sample clock for free-running LFOs: rendered once per audio buffer
// from the master bus (it adds no sound)
class ModClock: public AudioModule {
public:
	static ModClock *GetInstance() ;
	virtual bool Render(fixed *buffer,int samplecount) ;
	static void Advance(int frames) ;
	static double Seconds() ;
private:
	static double frames_ ;
} ;

// The settings of an instrument's four slots
class InstrumentMods {
public:
	InstrumentMods() ;
	void Create(VariableContainer &owner,int kind) ;
	// Back to the defaults: every slot off
	void Reset() ;
	bool Active() ;
	// Reads one slot's settings
	void GetSlot(int slot,ModSlotSettings &out) ;
	// Starts every slot that is on for a new note, keeping the voice's
	// active updater list in step (slots switched off leave it)
	void StartVoice(ModSource *sources,std::vector<I_SRPUpdater *> &active,
	                float krateHz,int note,int channel,unsigned int seed) ;
	// True if a slot is an ADSR on volume that fades the note out
	bool HasVolumeRelease() ;
	// A type was picked on the screen: load that type's starting values
	void ApplyTypeDefaults(int slot) ;

	// Destination list of this instrument kind (index <-> ModDest)
	int DestCount() { return destCount_ ; } ;
	int DestAt(int index) ;
	int DestIndexOf(int dest) ;
	Variable *TypeVar(int slot) { return type_[slot] ; } ;
	Variable *DestVar(int slot) { return dest_[slot] ; } ;
	Variable *AmountVar(int slot) { return amount_[slot] ; } ;
	Variable *ParamVar(int slot,int p) { return param_[slot][p] ; } ;

	// Loading songs saved with the first two-slot release: returns true
	// when it took care of the parameter
	bool RestoreLegacy(const char *name,const char *value) ;
	void FinishRestore() ;

	// What each type's parameters mean
	static int ParamCount(int type) ;
	static const ModParamDef *Param(int type,int p) ;
	static const char *TypeName(int type) ;
	static const char *DestName(int dest) ;
	static const char *ShapeName(int shape) ;
	static const char *TrigName(int trig) ;
private:
	Variable *type_[MOD_SLOT_COUNT] ;
	Variable *dest_[MOD_SLOT_COUNT] ;
	Variable *amount_[MOD_SLOT_COUNT] ;
	Variable *param_[MOD_SLOT_COUNT][MOD_PARAM_COUNT] ;
	int kind_ ;
	int destCount_ ;
	const int *dests_ ;
	int legacyShape_[MOD_SLOT_COUNT] ;
	int legacyRate_[MOD_SLOT_COUNT] ;
	bool sawParam_[MOD_SLOT_COUNT] ;
} ;

#endif
