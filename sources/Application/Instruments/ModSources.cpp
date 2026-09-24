#include "ModSources.h"
#include <math.h>

static char *modTypeNames[MT_LAST]={
	(char *)"off",(char *)"decay",(char *)"swell",(char *)"sine",
	(char *)"triangle",(char *)"square",(char *)"saw",(char *)"random"
} ;

static char *modDestNames[MD_LAST]={
	(char *)"volume",(char *)"cutoff",(char *)"reso",(char *)"pitch",(char *)"pan"
} ;

static const char *modVarNames[MOD_SLOT_COUNT][4]={
	{"mod1 type","mod1 dest","mod1 amount","mod1 rate"},
	{"mod2 type","mod2 dest","mod2 amount","mod2 rate"},
} ;

static const FourCC modVarIds[MOD_SLOT_COUNT][4]={
	{MOD1_TYPE,MOD1_DEST,MOD1_AMOUNT,MOD1_TIME},
	{MOD2_TYPE,MOD2_DEST,MOD2_AMOUNT,MOD2_TIME},
} ;

/***************************************************************
 ModSource
 ***************************************************************/

ModSource::ModSource() {
	enabled_=false ;
	type_=MT_OFF ;
	dest_=MD_CUTOFF ;
	amount_=0.0f ;
	level_=0.0f ;
	phase_=0.0f ;
	step_=0.0f ;
	held_=0.0f ;
	random_=1 ;
	offset_=0 ;
	speedFactor_=FP_ONE ;
}

float ModSource::TimeFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.001f*(float)pow(10.0,(value/255.0)*4.0) ;
}

float ModSource::EnvTimeFromRate(int rate) {
	return TimeFromParam(255-rate) ;
}

float ModSource::RateFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.05f*(float)pow(10.0,(value/255.0)*3.0) ;
}

float ModSource::LfoShape(int type,float phase,float held) {
	switch(type) {
		case MT_SINE: return (float)sin(6.28318530718f*phase) ;
		case MT_TRIANGLE: return 1.0f-4.0f*(float)fabs(phase-0.5f) ;
		case MT_SQUARE: return (phase<0.5f)?1.0f:-1.0f ;
		case MT_SAW: return 1.0f-2.0f*phase ;
		case MT_RANDOM: return held ;
		default: return 0.0f ;
	}
}

void ModSource::Start(int type,int dest,int amount,int time,float krateHz,unsigned int seed) {
	type_=type ;
	dest_=dest ;
	if (amount>MOD_AMOUNT_MAX) amount=MOD_AMOUNT_MAX ;
	if (amount<-MOD_AMOUNT_MAX) amount=-MOD_AMOUNT_MAX ;
	amount_=amount/(float)MOD_AMOUNT_MAX ;
	if (krateHz<1.0f) krateHz=1.0f ;
	random_=seed*2654435761u+1 ;
	phase_=0.0f ;
	if (IsLfo(type)) {
		step_=RateFromParam(time)/krateHz ;
		random_=random_*1664525u+1013904223u ;
		held_=((random_>>8)&0xFFFF)/32767.5f-1.0f ;
		level_=LfoShape(type_,0.0f,held_) ;
	} else {
		float steps=EnvTimeFromRate(time)*krateHz ;
		if (steps<1.0f) steps=1.0f ;
		if (type==MT_DECAY) {
			// Reaches -40 dB after 'time'
			step_=(float)exp(-4.6/steps) ;
			level_=1.0f ;
		} else {
			step_=1.0f/steps ;
			level_=0.0f ;
		}
	}
	computeOutput() ;
	enabled_=(type!=MT_OFF && amount!=0) ;
}

void ModSource::Trigger(bool tableTick) {
	if (!enabled_ || tableTick) return ;
	switch(type_) {
		case MT_DECAY:
			level_*=step_ ;
			if (level_<0.0001f) level_=0.0f ;
			break ;
		case MT_SWELL:
			level_+=step_ ;
			if (level_>1.0f) level_=1.0f ;
			break ;
		default:
			phase_+=step_ ;
			if (phase_>=1.0f) {
				phase_-=(float)floor(phase_) ;
				random_=random_*1664525u+1013904223u ;
				held_=((random_>>8)&0xFFFF)/32767.5f-1.0f ;
			}
			level_=LfoShape(type_,phase_,held_) ;
			break ;
	}
	computeOutput() ;
}

void ModSource::computeOutput() {
	float m=amount_*level_ ;
	switch(dest_) {
		case MD_VOLUME: offset_=fl2fp(m*255.0f) ; break ;
		case MD_PAN: offset_=fl2fp(m*127.0f) ; break ;
		case MD_PITCH:
			speedFactor_=fl2fp((float)pow(2.0,m*MOD_PITCH_RANGE/12.0f)) ;
			break ;
		default: offset_=fl2fp(m) ; break ;  // cutoff / reso: 0..1 scale
	}
}

void ModSource::UpdateSRP(struct RUParams &rup) {
	if (!enabled_) return ;
	switch(dest_) {
		case MD_VOLUME: rup.volumeOffset_+=offset_ ; break ;
		case MD_CUTOFF: rup.cutOffset_+=offset_ ; break ;
		case MD_RESO: rup.resOffset_+=offset_ ; break ;
		case MD_PAN: rup.panOffset_+=offset_ ; break ;
		case MD_PITCH: rup.speedOffset_=fp_mul(rup.speedOffset_,speedFactor_) ; break ;
		default: break ;
	}
}

/***************************************************************
 InstrumentMods
 ***************************************************************/

InstrumentMods::InstrumentMods() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		type_[s]=dest_[s]=amount_[s]=time_[s]=0 ;
	}
}

// Slot 1 aims at the filter (the classic sweep), slot 2 at pitch
// (vibrato), so switching a slot on does something useful right away
static const int modDefaultDest[MOD_SLOT_COUNT]={MD_CUTOFF,MD_PITCH} ;
static const int modDefaultAmount[MOD_SLOT_COUNT]={0x40,0x04} ;
static const int modDefaultTime=0x80 ;

void InstrumentMods::Create(VariableContainer &owner) {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		type_[s]=new Variable(modVarNames[s][0],modVarIds[s][0],modTypeNames,MT_LAST,MT_OFF) ;
		owner.Insert(type_[s]) ;
		dest_[s]=new Variable(modVarNames[s][1],modVarIds[s][1],modDestNames,MD_LAST,modDefaultDest[s]) ;
		owner.Insert(dest_[s]) ;
		amount_[s]=new Variable(modVarNames[s][2],modVarIds[s][2],modDefaultAmount[s]) ;
		owner.Insert(amount_[s]) ;
		time_[s]=new Variable(modVarNames[s][3],modVarIds[s][3],modDefaultTime) ;
		owner.Insert(time_[s]) ;
	}
}

void InstrumentMods::Reset() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		type_[s]->SetInt(MT_OFF) ;
		dest_[s]->SetInt(modDefaultDest[s]) ;
		amount_[s]->SetInt(modDefaultAmount[s]) ;
		time_[s]->SetInt(modDefaultTime) ;
	}
}

bool InstrumentMods::Active() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (type_[s] && type_[s]->GetInt()!=MT_OFF && amount_[s]->GetInt()!=0) {
			return true ;
		}
	}
	return false ;
}

void InstrumentMods::StartVoice(ModSource *sources,std::vector<I_SRPUpdater *> &active,
                                float krateHz,unsigned int seed) {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		ModSource &m=sources[s] ;
		m.Start(type_[s]->GetInt(),dest_[s]->GetInt(),amount_[s]->GetInt(),
		        time_[s]->GetInt(),krateHz,seed+s*7919) ;
		if (m.Enabled()) {
			active.push_back(&m) ;
		}
	}
}
