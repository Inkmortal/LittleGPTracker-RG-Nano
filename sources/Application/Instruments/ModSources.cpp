#include "ModSources.h"
#include "Services/Audio/Audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***************************************************************
 Names and parameter tables
 ***************************************************************/

static char *modTypeNames[MT_LAST]={
	(char *)"off",(char *)"ahd",(char *)"adsr",(char *)"drum",(char *)"lfo",
	(char *)"trig",(char *)"track",(char *)"decay",(char *)"swell"
} ;

static const char *modDestNames[MD_LAST]={
	"volume","cutoff","reso","pitch","pan","fine","drive","crush","shape",
	"fm amt","noise","fb mix","fb tune","start","loop st","reverb","delay","chorus"
} ;

static const char *modShapeNames[MLS_LAST]={
	"tri","sine","ramp dn","ramp up","exp dn","exp up","sqr dn","sqr up","random","drunk"
} ;

static const char *modTrigNames[MLT_LAST]={
	"free","retrig","hold","once"
} ;

// What each instrument kind can move (first five: the first release's list)
static const int synthDests[]={
	MD_VOLUME,MD_CUTOFF,MD_RESO,MD_PITCH,MD_PAN,MD_FINE,MD_DRIVE,MD_SHAPE,
	MD_FM,MD_NOISE,MD_REVERB,MD_DELAY,MD_CHORUS
} ;
static const int sampleDests[]={
	MD_VOLUME,MD_CUTOFF,MD_RESO,MD_PITCH,MD_PAN,MD_FINE,MD_DRIVE,MD_CRUSH,
	MD_FBMIX,MD_FBTUNE,MD_START,MD_LOOP,MD_REVERB,MD_DELAY,MD_CHORUS
} ;
#define SYNTH_DEST_COUNT ((int)(sizeof(synthDests)/sizeof(int)))
#define SAMPLE_DEST_COUNT ((int)(sizeof(sampleDests)/sizeof(int)))
static char *synthDestNames[SYNTH_DEST_COUNT] ;
static char *sampleDestNames[SAMPLE_DEST_COUNT] ;

#define NOPARAM {0,MPK_NONE,0,0,0,0}
#define TIMEPARAM(label,def) {label,MPK_TIME,0,0xFF,def,0x10}

static const ModParamDef modParams[MT_LAST][MOD_PARAM_COUNT]={
	// off
	{NOPARAM,NOPARAM,NOPARAM,NOPARAM},
	// ahd
	{TIMEPARAM("attack",0x00),TIMEPARAM("hold",0x00),TIMEPARAM("decay",0xA0),NOPARAM},
	// adsr
	{TIMEPARAM("attack",0x20),TIMEPARAM("decay",0xA0),
	 {"sustn",MPK_LEVEL,0,0xFF,0x80,0x10},TIMEPARAM("releas",0xA0)},
	// drum
	{{"peak",MPK_PEAK,0,0xFF,0x40,0x10},TIMEPARAM("body",0x60),TIMEPARAM("decay",0xA0),NOPARAM},
	// lfo
	{{"rate",MPK_RATE,0,0xFF,0xA0,0x10},{"shape",MPK_SHAPE,0,MLS_LAST-1,MLS_TRI,1},
	 {"trig",MPK_TRIGMODE,0,MLT_LAST-1,MLT_FREE,1},NOPARAM},
	// trig
	{TIMEPARAM("attack",0x00),TIMEPARAM("hold",0x00),TIMEPARAM("decay",0xA0),
	 {"source",MPK_TRACKSRC,0,7,0,1}},
	// track
	{{"from",MPK_NOTE,0,127,36,12},{"to",MPK_NOTE,0,127,96,12},
	 {"low",MPK_SIGNED,-MOD_AMOUNT_MAX,MOD_AMOUNT_MAX,-0x40,0x10},
	 {"high",MPK_SIGNED,-MOD_AMOUNT_MAX,MOD_AMOUNT_MAX,0x40,0x10}},
	// decay (first release)
	{{"rate",MPK_LEGACYENV,0,0xFF,0x80,0x10},NOPARAM,NOPARAM,NOPARAM},
	// swell (first release)
	{{"rate",MPK_LEGACYENV,0,0xFF,0x80,0x10},NOPARAM,NOPARAM,NOPARAM},
} ;

static const char *modVarNames[MOD_SLOT_COUNT][3+MOD_PARAM_COUNT]={
	{"mod1 type","mod1 dest","mod1 amount","mod1 p1","mod1 p2","mod1 p3","mod1 p4"},
	{"mod2 type","mod2 dest","mod2 amount","mod2 p1","mod2 p2","mod2 p3","mod2 p4"},
	{"mod3 type","mod3 dest","mod3 amount","mod3 p1","mod3 p2","mod3 p3","mod3 p4"},
	{"mod4 type","mod4 dest","mod4 amount","mod4 p1","mod4 p2","mod4 p3","mod4 p4"},
} ;

// Slot 1 aims at the filter (the classic sweep), slot 2 at pitch (vibrato),
// slot 3 at volume, slot 4 at pan, so switching a slot on does something
// useful right away
static const int modDefaultDest[MOD_SLOT_COUNT]={MD_CUTOFF,MD_PITCH,MD_VOLUME,MD_PAN} ;
static const int modDefaultAmount[MOD_SLOT_COUNT]={0x40,0x04,0x40,0x40} ;

bool ModSlotSettings::operator!=(const ModSlotSettings &o) const {
	if (type_!=o.type_ || dest_!=o.dest_ || amount_!=o.amount_) return true ;
	for (int p=0;p<MOD_PARAM_COUNT;p++) {
		if (param_[p]!=o.param_[p]) return true ;
	}
	return false ;
}

/***************************************************************
 Curves
 ***************************************************************/

// Decay/release shape: fast at first, then slower, landing exactly on the
// target when the segment ends
#define MOD_EXPO_K 5.0
#define MOD_LUT_SIZE 256
static float modExpoLut[MOD_LUT_SIZE+1] ;
static bool modLutReady=false ;

static void initModLut() {
	if (modLutReady) return ;
	double norm=1.0-exp(-MOD_EXPO_K) ;
	for (int i=0;i<=MOD_LUT_SIZE;i++) {
		double p=i/(double)MOD_LUT_SIZE ;
		modExpoLut[i]=(float)((1.0-exp(-MOD_EXPO_K*p))/norm) ;
	}
	modLutReady=true ;
}

static inline float expoCurve(float p) {
	if (p<=0.0f) return 0.0f ;
	if (p>=1.0f) return 1.0f ;
	float pos=p*MOD_LUT_SIZE ;
	int i=(int)pos ;
	float frac=pos-i ;
	return modExpoLut[i]+(modExpoLut[i+1]-modExpoLut[i])*frac ;
}

float ModSource::TimeFromParam(int value) {
	if (value<=0) return 0.0f ;
	if (value>255) value=255 ;
	return 0.001f*(float)pow(10.0,(value/255.0)*4.0) ;
}

float ModSource::RateFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.05f*(float)pow(10.0,(value/255.0)*3.0) ;
}

// First release: "rate", higher is faster, 00 = 10 s .. FF = 1 ms
float ModSource::LegacyEnvTime(int rate) {
	int value=255-rate ;
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.001f*(float)pow(10.0,(value/255.0)*4.0) ;
}

// Drum envelope PEAK: 00 = a plain hold (no dip, 1 ms), FF = a deep dip
// (to 10 %) over 40 ms, then the body swells back
float ModSource::PeakTime(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.001f+0.039f*(value/255.0f) ;
}

float ModSource::PeakDip(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 1.0f-0.9f*(value/255.0f) ;
}

float ModSource::LfoShape(int shape,float phase,float held) {
	switch(shape) {
		case MLS_TRI: return 1.0f-4.0f*(float)fabs(phase-0.5f) ;
		case MLS_SINE: return (float)sin(6.28318530718f*phase) ;
		case MLS_RAMP_DOWN: return 1.0f-2.0f*phase ;
		case MLS_RAMP_UP: return 2.0f*phase-1.0f ;
		case MLS_EXP_DOWN: return 1.0f-2.0f*expoCurve(phase) ;
		case MLS_EXP_UP: return 1.0f-2.0f*expoCurve(1.0f-phase) ;
		case MLS_SQUARE_DOWN: return (phase<0.5f)?1.0f:-1.0f ;
		case MLS_SQUARE_UP: return (phase<0.5f)?-1.0f:1.0f ;
		case MLS_RANDOM:
		case MLS_DRUNK: return held ;
		default: return 0.0f ;
	}
}

bool ModSource::IsEnvelope(int type) {
	return type==MT_AHD || type==MT_ADSR || type==MT_DRUM || type==MT_TRIG ;
}

// Same random level for a given free-running cycle on every voice
static float cycleRandom(unsigned int cycle,int slot) {
	unsigned int x=cycle*2654435761u+(unsigned int)slot*40503u+0x9E3779B9u ;
	x^=x>>16 ; x*=0x7feb352dU ; x^=x>>15 ; x*=0x846ca68bU ; x^=x>>16 ;
	return ((x>>8)&0xFFFF)/32767.5f-1.0f ;
}

/***************************************************************
 ModSource
 ***************************************************************/

enum ModStage {
	MS_IDLE=0,
	MS_ATTACK,
	MS_HOLD,
	MS_DECAY,
	MS_SUSTAIN,
	MS_RELEASE,
	MS_DIP,
	MS_RISE,
	MS_DONE
} ;

int ModSource::noteStarts_[8]={0,0,0,0,0,0,0,0} ;

void ModSource::NoteStarted(int channel) {
	if (channel>=0 && channel<8) {
		noteStarts_[channel]++ ;
	}
}

ModSource::ModSource() {
	initModLut() ;
	enabled_=false ;
	owner_=0 ;
	slot_=0 ;
	s_.type_=MT_OFF ;
	s_.dest_=MD_CUTOFF ;
	s_.amount_=0 ;
	for (int p=0;p<MOD_PARAM_COUNT;p++) s_.param_[p]=0 ;
	krate_=441.0f ;
	note_=60 ;
	channel_=0 ;
	amount_=0.0f ;
	level_=0.0f ;
	stage_=MS_IDLE ;
	segPhase_=segInc_=segFrom_=segTo_=0.0f ;
	trigSeen_=0 ;
	phase_=step_=held_=0.0f ;
	cycleDone_=false ;
	random_=1 ;
	legacyStep_=0.0f ;
	offset_=0 ;
	speedFactor_=FP_ONE ;
	volScale_=1.0f ;
	extra_=0.0f ;
}

unsigned int ModSource::nextRandom() {
	random_=random_*1664525u+1013904223u ;
	return random_ ;
}

void ModSource::Start(InstrumentMods *owner,int slot,float krateHz,int note,
                      int channel,unsigned int seed) {
	ModSlotSettings s ;
	owner->GetSlot(slot,s) ;
	Start(s,krateHz,note,channel,seed) ;
	owner_=owner ;
	slot_=slot ;
}

void ModSource::Start(const ModSlotSettings &settings,float krateHz,int note,
                      int channel,unsigned int seed) {
	owner_=0 ;
	s_=settings ;
	krate_=(krateHz<1.0f)?1.0f:krateHz ;
	note_=note ;
	channel_=channel ;
	random_=seed*2654435761u+1 ;
	setup(true) ;
	enabled_=(s_.type_!=MT_OFF && (s_.amount_!=0 || s_.type_==MT_TRACK)) ;
}

float ModSource::TrackLevel(const ModSlotSettings &s,int note) {
	int from=s.param_[0] ;
	int to=s.param_[1] ;
	float lo=s.param_[2]/(float)MOD_AMOUNT_MAX ;
	float hi=s.param_[3]/(float)MOD_AMOUNT_MAX ;
	float t ;
	if (to==from) {
		t=(note>=to)?1.0f:0.0f ;
	} else {
		t=(note-from)/(float)(to-from) ;
	}
	if (t<0.0f) t=0.0f ;
	if (t>1.0f) t=1.0f ;
	float v=lo+(hi-lo)*t ;
	if (v<-1.0f) v=-1.0f ;
	if (v>1.0f) v=1.0f ;
	return v ;
}

float ModSource::trackLevel() {
	return TrackLevel(s_,note_) ;
}

// Starts the envelope/LFO from the top. noteStart: a new note (trig
// envelopes then wait for their source), otherwise a live type change.
void ModSource::setup(bool noteStart) {
	int amount=s_.amount_ ;
	if (amount>MOD_AMOUNT_MAX) amount=MOD_AMOUNT_MAX ;
	if (amount<-MOD_AMOUNT_MAX) amount=-MOD_AMOUNT_MAX ;
	amount_=amount/(float)MOD_AMOUNT_MAX ;
	cycleDone_=false ;
	switch(s_.type_) {
		case MT_AHD:
		case MT_ADSR:
			level_=0.0f ;
			enterStage(MS_ATTACK) ;
			advanceEnvelope(0.0f) ;
			break ;
		case MT_DRUM:
			// The transient: full level right at the note start
			level_=1.0f ;
			enterStage(MS_DIP) ;
			advanceEnvelope(0.0f) ;
			break ;
		case MT_TRIG: {
			level_=0.0f ;
			stage_=MS_IDLE ;
			int src=s_.param_[3]&7 ;
			trigSeen_=noteStarts_[src] ;
			break ;
		}
		case MT_LFO: {
			float hz=RateFromParam(s_.param_[0]) ;
			step_=hz/krate_ ;
			int shape=s_.param_[1] ;
			if (s_.param_[2]==MLT_FREE) {
				double t=ModClock::Seconds()*hz ;
				double cycle=floor(t) ;
				phase_=(float)(t-cycle) ;
				held_=cycleRandom((unsigned int)cycle,slot_) ;
			} else {
				phase_=0.0f ;
				held_=((nextRandom()>>8)&0xFFFF)/32767.5f-1.0f ;
			}
			if (shape==MLS_DRUNK) {
				held_*=0.5f ;
			}
			level_=LfoShape(shape,phase_,held_) ;
			break ;
		}
		case MT_TRACK:
			amount_=1.0f ;
			level_=trackLevel() ;
			break ;
		case MT_DECAY:
		case MT_SWELL: {
			float steps=LegacyEnvTime(s_.param_[0])*krate_ ;
			if (steps<1.0f) steps=1.0f ;
			if (s_.type_==MT_DECAY) {
				// Reaches -40 dB after 'rate'
				legacyStep_=(float)exp(-4.6/steps) ;
				level_=1.0f ;
			} else {
				legacyStep_=1.0f/steps ;
				level_=0.0f ;
			}
			break ;
		}
		default:
			level_=0.0f ;
			break ;
	}
	(void)noteStart ;
	computeOutput() ;
}

// A segment runs from the current level to its target over its time
void ModSource::enterStage(int stage) {
	stage_=stage ;
	segPhase_=0.0f ;
	segFrom_=level_ ;
	float time=0.0f ;
	switch(stage) {
		case MS_ATTACK:
			segTo_=1.0f ;
			time=TimeFromParam(s_.param_[0]) ;
			break ;
		case MS_HOLD:
			segTo_=1.0f ;
			time=TimeFromParam(s_.param_[1]) ;   // hold, or the drum's body
			break ;
		case MS_DECAY:
			if (s_.type_==MT_ADSR) {
				segTo_=s_.param_[2]/255.0f ;
				time=TimeFromParam(s_.param_[1]) ;
			} else {
				segTo_=0.0f ;
				time=TimeFromParam(s_.param_[2]) ;
			}
			break ;
		case MS_SUSTAIN:
			segTo_=s_.param_[2]/255.0f ;
			level_=segTo_ ;
			time=-1.0f ;
			break ;
		case MS_RELEASE:
			segTo_=0.0f ;
			time=TimeFromParam(s_.param_[3]) ;
			break ;
		case MS_DIP:
			segTo_=PeakDip(s_.param_[0]) ;
			time=PeakTime(s_.param_[0]) ;
			break ;
		case MS_RISE:
			segTo_=1.0f ;
			time=PeakTime(s_.param_[0]) ;
			break ;
		default:
			// idle / done
			segTo_=level_ ;
			time=-1.0f ;
			break ;
	}
	// segInc_: segment length in seconds (0 = instant, <0 = stays)
	segInc_=time ;
}

static int nextStage(int type,int stage) {
	switch(stage) {
		case MS_ATTACK:
			return type==MT_ADSR?MS_DECAY:MS_HOLD ;
		case MS_HOLD:
			return MS_DECAY ;
		case MS_DECAY:
			return type==MT_ADSR?MS_SUSTAIN:MS_DONE ;
		case MS_RELEASE:
			return MS_DONE ;
		case MS_DIP:
			return MS_RISE ;
		case MS_RISE:
			return MS_HOLD ;
		default:
			return stage ;
	}
}

float ModSource::curve(int stage,float phase) {
	if (stage==MS_DECAY || stage==MS_RELEASE) {
		return expoCurve(phase) ;
	}
	return phase ;
}

// Moves the envelope on by dt seconds, carrying what is left of a finished
// segment into the next one so the timing stays exact
void ModSource::advanceEnvelope(float dt) {
	for (int guard=0;guard<16;guard++) {
		if (stage_==MS_IDLE || stage_==MS_DONE) {
			return ;
		}
		if (stage_==MS_SUSTAIN) {
			// follows the sustain knob while it holds
			level_=s_.param_[2]/255.0f ;
			return ;
		}
		float length=segInc_ ;
		float remaining=(1.0f-segPhase_)*length ;
		if (length<=0.0f || dt>=remaining) {
			if (length>0.0f) dt-=remaining ;
			level_=segTo_ ;
			enterStage(nextStage(s_.type_,stage_)) ;
			if (stage_==MS_DONE) level_=0.0f ;
			continue ;
		}
		segPhase_+=dt/length ;
		level_=segFrom_+(segTo_-segFrom_)*curve(stage_,segPhase_) ;
		return ;
	}
}

void ModSource::advanceLfo() {
	int shape=s_.param_[1] ;
	int trig=s_.param_[2] ;
	if (cycleDone_) {
		return ;
	}
	if (shape==MLS_DRUNK) {
		// Random walk, wider at higher rates, kept inside -1..1
		float stepSize=2.0f*(float)sqrt(step_) ;
		float r=((nextRandom()>>8)&0xFFFF)/32767.5f-1.0f ;
		held_+=r*stepSize ;
		if (held_>1.0f) held_=2.0f-held_ ;
		if (held_<-1.0f) held_=-2.0f-held_ ;
	}
	phase_+=step_ ;
	if (phase_>=1.0f) {
		if (trig==MLT_HOLD) {
			phase_=0.99999f ;
			cycleDone_=true ;
		} else if (trig==MLT_ONCE) {
			phase_=0.0f ;
			cycleDone_=true ;
		} else {
			phase_-=(float)floor(phase_) ;
			if (shape==MLS_RANDOM) {
				if (trig==MLT_FREE) {
					double cycle=floor(ModClock::Seconds()*RateFromParam(s_.param_[0])) ;
					held_=cycleRandom((unsigned int)cycle,slot_) ;
				} else {
					held_=((nextRandom()>>8)&0xFFFF)/32767.5f-1.0f ;
				}
			}
		}
	}
	level_=LfoShape(shape,phase_,held_) ;
}

void ModSource::Retrigger() {
	switch(s_.type_) {
		case MT_AHD:
		case MT_ADSR:
			enterStage(MS_ATTACK) ;
			advanceEnvelope(0.0f) ;
			break ;
		case MT_DRUM:
			level_=1.0f ;
			enterStage(MS_DIP) ;
			advanceEnvelope(0.0f) ;
			break ;
		case MT_LFO:
			if (s_.param_[2]!=MLT_FREE) {
				phase_=0.0f ;
				cycleDone_=false ;
				level_=LfoShape(s_.param_[1],phase_,held_) ;
			}
			break ;
		case MT_DECAY:
		case MT_SWELL:
			setup(true) ;
			break ;
		default:
			break ;
	}
	computeOutput() ;
}

void ModSource::NoteOff() {
	if (s_.type_==MT_ADSR && stage_!=MS_RELEASE && stage_!=MS_DONE) {
		enterStage(MS_RELEASE) ;
		advanceEnvelope(0.0f) ;
		computeOutput() ;
	}
}

bool ModSource::IsDone() {
	if (!enabled_) return true ;
	if (IsEnvelope(s_.type_)) {
		return stage_==MS_DONE ;
	}
	return false ;
}

float ModSource::GetVolumeScale() {
	return volScale_ ;
}

void ModSource::Trigger(bool tableTick) {
	if (!enabled_ || tableTick) return ;
	// Knobs turned while the note plays take effect right away
	if (owner_) {
		ModSlotSettings now ;
		owner_->GetSlot(slot_,now) ;
		if (now!=s_) {
			bool restart=(now.type_!=s_.type_) ;
			s_=now ;
			if (restart) {
				setup(false) ;
			} else {
				int amount=s_.amount_ ;
				if (amount>MOD_AMOUNT_MAX) amount=MOD_AMOUNT_MAX ;
				if (amount<-MOD_AMOUNT_MAX) amount=-MOD_AMOUNT_MAX ;
				amount_=(s_.type_==MT_TRACK)?1.0f:amount/(float)MOD_AMOUNT_MAX ;
				if (s_.type_==MT_LFO) {
					step_=RateFromParam(s_.param_[0])/krate_ ;
				} else if (IsEnvelope(s_.type_) && stage_!=MS_IDLE &&
				           stage_!=MS_DONE && stage_!=MS_SUSTAIN) {
					// new segment time, same position in it
					float phase=segPhase_ ;
					float from=segFrom_ ;
					enterStage(stage_) ;
					segPhase_=phase ;
					segFrom_=from ;
				}
			}
		}
	}
	float dt=1.0f/krate_ ;
	switch(s_.type_) {
		case MT_AHD:
		case MT_ADSR:
		case MT_DRUM:
			advanceEnvelope(dt) ;
			break ;
		case MT_TRIG: {
			int src=s_.param_[3]&7 ;
			if (noteStarts_[src]!=trigSeen_) {
				// a note on the source track: fire from where it is
				trigSeen_=noteStarts_[src] ;
				enterStage(MS_ATTACK) ;
				advanceEnvelope(0.0f) ;
			} else {
				advanceEnvelope(dt) ;
			}
			break ;
		}
		case MT_LFO:
			advanceLfo() ;
			break ;
		case MT_TRACK:
			level_=trackLevel() ;
			break ;
		case MT_DECAY:
			level_*=legacyStep_ ;
			if (level_<0.0001f) level_=0.0f ;
			break ;
		case MT_SWELL:
			level_+=legacyStep_ ;
			if (level_>1.0f) level_=1.0f ;
			break ;
		default:
			level_=0.0f ;
			break ;
	}
	computeOutput() ;
}

static const int destExtra[MD_LAST]={
	-1,-1,-1,-1,-1,-1,          // volume cutoff reso pitch pan fine
	RUX_DRIVE,RUX_CRUSH,RUX_SHAPE,RUX_FM,RUX_NOISE,
	-1,-1,                      // fb mix, fb tune
	RUX_START,RUX_LOOP,RUX_REVERB,RUX_DELAY,RUX_CHORUS
} ;

void ModSource::computeOutput() {
	float m=amount_*level_ ;
	offset_=0 ;
	speedFactor_=FP_ONE ;
	volScale_=1.0f ;
	extra_=0.0f ;
	switch(s_.dest_) {
		case MD_VOLUME:
			if (IsEnvelope(s_.type_)) {
				// Envelopes shape the note: +100 % follows the envelope from
				// silence, -100 % ducks it away while the envelope is up
				volScale_=(amount_>=0.0f)?1.0f-amount_*(1.0f-level_):1.0f+amount_*level_ ;
				if (volScale_<0.0f) volScale_=0.0f ;
			} else {
				offset_=fl2fp(m*255.0f) ;
			}
			break ;
		case MD_PAN: offset_=fl2fp(m*127.0f) ; break ;
		case MD_PITCH:
			speedFactor_=fl2fp((float)pow(2.0,m*MOD_PITCH_RANGE/12.0f)) ;
			break ;
		case MD_FINE:
			speedFactor_=fl2fp((float)pow(2.0,m*MOD_FINE_RANGE/12.0f)) ;
			break ;
		case MD_DRIVE: extra_=m*255.0f ; break ;
		case MD_FM: extra_=m*255.0f ; break ;
		case MD_CRUSH: extra_=m*15.0f ; break ;
		case MD_CUTOFF:
		case MD_RESO:
		case MD_FBMIX:
		case MD_FBTUNE:
			offset_=fl2fp(m) ;   // 0..1 scale
			break ;
		default:
			extra_=m ;           // shape, noise, start, loop, sends: 0..1
			break ;
	}
}

void ModSource::UpdateSRP(struct RUParams &rup) {
	if (!enabled_) return ;
	switch(s_.dest_) {
		case MD_VOLUME:
			rup.volumeOffset_+=offset_ ;
			rup.volumeScale_*=volScale_ ;
			break ;
		case MD_CUTOFF: rup.cutOffset_+=offset_ ; break ;
		case MD_RESO: rup.resOffset_+=offset_ ; break ;
		case MD_PAN: rup.panOffset_+=offset_ ; break ;
		case MD_FBMIX: rup.fbMixOffset_+=offset_ ; break ;
		case MD_FBTUNE: rup.fbTunOffset_+=offset_ ; break ;
		case MD_PITCH:
		case MD_FINE:
			rup.speedOffset_=fp_mul(rup.speedOffset_,speedFactor_) ;
			break ;
		default:
			if (s_.dest_>=0 && s_.dest_<MD_LAST && destExtra[s_.dest_]>=0) {
				rup.extra_[destExtra[s_.dest_]]+=extra_ ;
			}
			break ;
	}
}

void ModSource::Preview(const ModSlotSettings &settings,float window,float gate,
                        int note,float *out,int count) {
	if (count<=0) return ;
	ModSlotSettings s=settings ;
	// Drawn from a note start: a trig envelope as if its source just
	// played, a free LFO from the top of its cycle
	if (s.type_==MT_TRIG) s.type_=MT_AHD ;
	if (s.type_==MT_LFO && s.param_[2]==MLT_FREE) s.param_[2]=MLT_RETRIG ;
	s.amount_=MOD_AMOUNT_MAX ;
	float krate=(window>0.0f)?count/window:1000.0f ;
	ModSource m ;
	m.Start(s,krate,note,-1,12345u) ;
	m.slot_=0 ;
	float dt=(count>1)?window/(count-1):window ;
	bool released=false ;
	out[0]=m.GetLevel() ;
	for (int i=1;i<count;i++) {
		if (!released && gate>=0.0f && i*dt>=gate) {
			m.NoteOff() ;
			released=true ;
		}
		m.Trigger(false) ;
		out[i]=m.GetLevel() ;
	}
}

/***************************************************************
 ModClock
 ***************************************************************/

double ModClock::frames_=0.0 ;

ModClock *ModClock::GetInstance() {
	static ModClock clock ;
	return &clock ;
}

bool ModClock::Render(fixed *buffer,int samplecount) {
	Advance(samplecount) ;
	return false ;   // no sound of its own
}

void ModClock::Advance(int frames) {
	frames_+=frames ;
}

double ModClock::Seconds() {
	Audio *audio=Audio::GetInstance() ;
	double rate=audio?audio->GetSampleRate():44100.0 ;
	if (rate<8000.0) rate=44100.0 ;
	return frames_/rate ;
}

/***************************************************************
 InstrumentMods
 ***************************************************************/

InstrumentMods::InstrumentMods() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		type_[s]=dest_[s]=amount_[s]=0 ;
		for (int p=0;p<MOD_PARAM_COUNT;p++) param_[s][p]=0 ;
		legacyShape_[s]=-1 ;
		legacyRate_[s]=-1 ;
		sawParam_[s]=false ;
	}
	kind_=MIK_SAMPLE ;
	destCount_=SAMPLE_DEST_COUNT ;
	dests_=sampleDests ;
}

int InstrumentMods::DestAt(int index) {
	if (index<0 || index>=destCount_) return MD_VOLUME ;
	return dests_[index] ;
}

int InstrumentMods::DestIndexOf(int dest) {
	for (int i=0;i<destCount_;i++) {
		if (dests_[i]==dest) return i ;
	}
	return 0 ;
}

void InstrumentMods::Create(VariableContainer &owner,int kind) {
	kind_=kind ;
	char **names ;
	if (kind==MIK_SYNTH) {
		dests_=synthDests ;
		destCount_=SYNTH_DEST_COUNT ;
		names=synthDestNames ;
	} else {
		dests_=sampleDests ;
		destCount_=SAMPLE_DEST_COUNT ;
		names=sampleDestNames ;
	}
	for (int i=0;i<destCount_;i++) {
		names[i]=(char *)modDestNames[dests_[i]] ;
	}
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		type_[s]=new Variable(modVarNames[s][0],MOD_TYPE_ID(s),modTypeNames,MT_LAST,MT_OFF) ;
		owner.Insert(type_[s]) ;
		dest_[s]=new Variable(modVarNames[s][1],MOD_DEST_ID(s),names,destCount_,
		                      DestIndexOf(modDefaultDest[s])) ;
		owner.Insert(dest_[s]) ;
		amount_[s]=new Variable(modVarNames[s][2],MOD_AMOUNT_ID(s),modDefaultAmount[s]) ;
		owner.Insert(amount_[s]) ;
		for (int p=0;p<MOD_PARAM_COUNT;p++) {
			param_[s][p]=new Variable(modVarNames[s][3+p],MOD_PARAM_ID(s,p),0) ;
			owner.Insert(param_[s][p]) ;
		}
	}
}

void InstrumentMods::Reset() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		type_[s]->SetInt(MT_OFF) ;
		dest_[s]->SetInt(DestIndexOf(modDefaultDest[s])) ;
		amount_[s]->SetInt(modDefaultAmount[s]) ;
		for (int p=0;p<MOD_PARAM_COUNT;p++) {
			param_[s][p]->SetInt(0) ;
		}
	}
}

void InstrumentMods::ApplyTypeDefaults(int slot) {
	if (slot<0 || slot>=MOD_SLOT_COUNT || !type_[slot]) return ;
	int type=type_[slot]->GetInt() ;
	if (type<0 || type>=MT_LAST) return ;
	for (int p=0;p<MOD_PARAM_COUNT;p++) {
		const ModParamDef &d=modParams[type][p] ;
		param_[slot][p]->SetInt(d.kind_==MPK_NONE?0:d.default_) ;
	}
}

bool InstrumentMods::Active() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		int type=type_[s]->GetInt() ;
		if (type!=MT_OFF && (amount_[s]->GetInt()!=0 || type==MT_TRACK)) {
			return true ;
		}
	}
	return false ;
}

void InstrumentMods::GetSlot(int slot,ModSlotSettings &out) {
	out.type_=type_[slot]->GetInt() ;
	if (out.type_<0 || out.type_>=MT_LAST) out.type_=MT_OFF ;
	out.dest_=DestAt(dest_[slot]->GetInt()) ;
	out.amount_=amount_[slot]->GetInt() ;
	for (int p=0;p<MOD_PARAM_COUNT;p++) {
		out.param_[p]=param_[slot][p]->GetInt() ;
	}
}

bool InstrumentMods::HasVolumeRelease() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		if (type_[s]->GetInt()==MT_ADSR && DestAt(dest_[s]->GetInt())==MD_VOLUME &&
		    amount_[s]->GetInt()>0) {
			return true ;
		}
	}
	return false ;
}

void InstrumentMods::StartVoice(ModSource *sources,std::vector<I_SRPUpdater *> &active,
                                float krateHz,int note,int channel,unsigned int seed) {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		ModSource &m=sources[s] ;
		m.Start(this,s,krateHz,note,channel,seed+s*7919) ;
		std::vector<I_SRPUpdater *>::iterator it=active.begin() ;
		while (it!=active.end() && *it!=&m) it++ ;
		bool listed=(it!=active.end()) ;
		if (m.Enabled() && !listed) {
			active.push_back(&m) ;
		} else if (!m.Enabled() && listed) {
			active.erase(it) ;
		}
	}
}

int InstrumentMods::ParamCount(int type) {
	if (type<0 || type>=MT_LAST) return 0 ;
	int count=0 ;
	for (int p=0;p<MOD_PARAM_COUNT;p++) {
		if (modParams[type][p].kind_!=MPK_NONE) count=p+1 ;
	}
	return count ;
}

const ModParamDef *InstrumentMods::Param(int type,int p) {
	if (type<0 || type>=MT_LAST || p<0 || p>=MOD_PARAM_COUNT) return 0 ;
	if (modParams[type][p].kind_==MPK_NONE) return 0 ;
	return &modParams[type][p] ;
}

const char *InstrumentMods::TypeName(int type) {
	if (type<0 || type>=MT_LAST) return "?" ;
	return modTypeNames[type] ;
}

const char *InstrumentMods::DestName(int dest) {
	if (dest<0 || dest>=MD_LAST) return "?" ;
	return modDestNames[dest] ;
}

const char *InstrumentMods::ShapeName(int shape) {
	if (shape<0 || shape>=MLS_LAST) return "?" ;
	return modShapeNames[shape] ;
}

const char *InstrumentMods::TrigName(int trig) {
	if (trig<0 || trig>=MLT_LAST) return "?" ;
	return modTrigNames[trig] ;
}

// Songs from the first release: slot type "sine".."random" was an LFO shape
// (now LFO + shape, restarting with every note), "modN rate" is now P1
bool InstrumentMods::RestoreLegacy(const char *name,const char *value) {
	if (!name || !value) return false ;
	for (int s=0;s<2;s++) {
		char buffer[24] ;
		sprintf(buffer,"mod%d type",s+1) ;
		if (!strcmp(name,buffer)) {
			static const char *legacyLfo[]={"sine","triangle","square","saw","random"} ;
			static const int legacyShape[]={MLS_SINE,MLS_TRI,MLS_SQUARE_DOWN,MLS_RAMP_DOWN,MLS_RANDOM} ;
			for (int k=0;k<5;k++) {
				if (!strcmp(value,legacyLfo[k])) {
					type_[s]->SetInt(MT_LFO) ;
					legacyShape_[s]=legacyShape[k] ;
					return true ;
				}
			}
			return false ;
		}
		sprintf(buffer,"mod%d rate",s+1) ;
		if (!strcmp(name,buffer)) {
			legacyRate_[s]=atoi(value) ;
			return true ;
		}
	}
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!strcmp(name,modVarNames[s][3])) {
			sawParam_[s]=true ;   // saved by this release: nothing to convert
		}
	}
	return false ;
}

void InstrumentMods::FinishRestore() {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (!type_[s]) continue ;
		int type=type_[s]->GetInt() ;
		bool legacy=(legacyShape_[s]>=0 || legacyRate_[s]>=0 ||
		             ((type==MT_DECAY || type==MT_SWELL) && !sawParam_[s])) ;
		if (legacy) {
			// the first release's default rate was 80
			int rate=legacyRate_[s]>=0?legacyRate_[s]:0x80 ;
			param_[s][0]->SetInt(rate) ;
			if (legacyShape_[s]>=0) {
				param_[s][1]->SetInt(legacyShape_[s]) ;
				param_[s][2]->SetInt(MLT_RETRIG) ;
				param_[s][3]->SetInt(0) ;
			}
		}
		legacyShape_[s]=-1 ;
		legacyRate_[s]=-1 ;
		sawParam_[s]=false ;
	}
}
