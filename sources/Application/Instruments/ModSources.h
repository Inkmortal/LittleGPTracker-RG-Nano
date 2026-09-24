#ifndef _MOD_SOURCES_H_
#define _MOD_SOURCES_H_

// Two modulation slots per instrument (like the M8's envelopes and LFOs).
// Each slot is an envelope or an LFO aimed at volume, cutoff, reso, pitch
// or pan. A running slot is an I_SRPUpdater, so it rides the same k-rate
// path as the VOLM/FCUT/PTCH command ramps in both the sampler and the synth.

#include "I_SRPUpdater.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/Variable.h"
#include "Foundation/Variables/VariableContainer.h"
#include <vector>

#define MOD_SLOT_COUNT 2

enum ModType {
	MT_OFF=0,
	MT_DECAY,     // envelope: full at the note start, falls away
	MT_SWELL,     // envelope: rises from nothing, then holds
	MT_SINE,
	MT_TRIANGLE,
	MT_SQUARE,
	MT_SAW,
	MT_RANDOM,    // a new random level every cycle (sample and hold)
	MT_LAST
} ;

enum ModDest {
	MD_VOLUME=0,
	MD_CUTOFF,
	MD_RESO,
	MD_PITCH,
	MD_PAN,
	MD_LAST
} ;

// Variable ids: 'M' slot 'T'ype / 'D'est / 'A'mount / 'S'peed (rate)
#define MOD1_TYPE   MAKE_FOURCC('M','1','T','Y')
#define MOD1_DEST   MAKE_FOURCC('M','1','D','S')
#define MOD1_AMOUNT MAKE_FOURCC('M','1','A','M')
#define MOD1_TIME   MAKE_FOURCC('M','1','S','P')
#define MOD2_TYPE   MAKE_FOURCC('M','2','T','Y')
#define MOD2_DEST   MAKE_FOURCC('M','2','D','S')
#define MOD2_AMOUNT MAKE_FOURCC('M','2','A','M')
#define MOD2_TIME   MAKE_FOURCC('M','2','S','P')

#define MOD_AMOUNT_MAX 127
// Full amount on pitch moves this many semitones
#define MOD_PITCH_RANGE 24.0f

// One running slot on one voice
class ModSource: public I_SRPUpdater {
public:
	ModSource() ;
	virtual ~ModSource() {} ;
	void Start(int type,int dest,int amount,int time,float krateHz,unsigned int seed) ;
	virtual void Trigger(bool tableTick) ;
	virtual void UpdateSRP(struct RUParams &rup) ;

	// "rate": higher is always faster. Envelopes: 00 = 10 s .. FF = 1 ms,
	// LFOs: 00 = 0.05 Hz .. FF = 50 Hz
	static float TimeFromParam(int value) ;
	static float EnvTimeFromRate(int rate) ;
	static float RateFromParam(int value) ;
	static bool IsLfo(int type) { return type>=MT_SINE ; } ;
	// LFO level for a phase 0..1, -1..1 (random uses 'held')
	static float LfoShape(int type,float phase,float held) ;
private:
	void computeOutput() ;
	int type_ ;
	int dest_ ;
	float amount_ ;   // -1..1
	float level_ ;    // envelope 0..1 or LFO -1..1
	float phase_ ;
	float step_ ;     // per k-rate step: phase increment or envelope rate
	float held_ ;
	unsigned int random_ ;
	fixed offset_ ;       // additive offset for volume/cutoff/reso/pan
	fixed speedFactor_ ;  // multiplier for pitch
} ;

// The eight settings of an instrument's two slots
class InstrumentMods {
public:
	InstrumentMods() ;
	void Create(VariableContainer &owner) ;
	// Back to the defaults: both slots off
	void Reset() ;
	bool Active() ;
	// Starts every slot that is on, adding it to the voice's active updaters
	void StartVoice(ModSource *sources,std::vector<I_SRPUpdater *> &active,
	                float krateHz,unsigned int seed) ;
private:
	Variable *type_[MOD_SLOT_COUNT] ;
	Variable *dest_[MOD_SLOT_COUNT] ;
	Variable *amount_[MOD_SLOT_COUNT] ;
	Variable *time_[MOD_SLOT_COUNT] ;
} ;

#endif
