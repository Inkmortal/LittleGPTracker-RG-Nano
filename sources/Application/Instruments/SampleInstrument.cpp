#include "Application/Mixer/SendFX.h"
#include "SampleInstrument.h"
#include "SamplePool.h"
#include "System/Console/Trace.h"
#include "System/io/Status.h"
#include "CommandList.h"
#include <assert.h>
#include "Application/Player/PlayerMixer.h" // For MIX_BUFFER_SIZE.. kick out pls
#include "Application/Player/SyncMaster.h"
#include "Application/Instruments/Filters.h"
#include "Application/Model/Table.h"
#include "Services/Audio/Audio.h"
#include "SampleVariable.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "SampleInstrumentDatas.h"
#include "Application/Player/SyncMaster.h"

fixed SampleInstrument::feedback_[SONG_CHANNEL_COUNT][FB_BUFFER_LENGTH*2] ;

bool SampleInstrument::useDirtyDownsampling_ = false;

#define SHOULD_KILL_CLICKS false

int SampleInstrument::lastMidiNote_[SONG_CHANNEL_COUNT]= {
	-1,-1,-1,-1,-1,-1,-1,-1
} ;

#define KRATE_SAMPLE_COUNT 100

SampleInstrument::SampleInstrument() {

// Initialize instruments settings

     source_=0 ;
     dirty_=false ;
     running_=false ;
	 suggestedRootNote_=-1 ;
     fxPresets[0] = "room";
     fxPresets[1] = "hall";
     fxPresets[2] = "spring";
     fxPresets[3] = "church";

// Initialize exported variables

	 WatchedVariable *wv=new SampleVariable("sample",SIP_SAMPLE) ;
	 Insert(wv) ;
	 wv->AddObserver(*this) ;
	 
	 volume_=new Variable("volume",SIP_VOLUME,0x80) ;
	 Insert(volume_) ;

	 interpolation_=new Variable("interpol",SIP_INTERPOLATION,interpolationTypes,2,0) ;
	 Insert(interpolation_) ;
	 
	 crush_=new Variable("crush",SIP_CRUSH,16) ;
	 Insert(crush_) ;

	 drive_=new Variable("crushdrive",SIP_CRUSHVOL,0xFF) ;
	 Insert(drive_) ;

	 downsample_=new Variable("downsample",SIP_DOWNSMPL,0) ;
	 Insert(downsample_) ;

	 rootNote_=new Variable("root note",SIP_ROOTNOTE,60) ;
	 Insert(rootNote_) ;

	 fineTune_=new Variable("fine tune",SIP_FINETUNE,0x7F) ;
	 Insert(fineTune_) ;

	 pan_=new Variable("pan",SIP_PAN,0x7F) ;
	 Insert(pan_) ;

	 cutoff_=new Variable("filter cut",SIP_FILTCUTOFF,0xFF) ;
	 Insert(cutoff_) ;

	 reso_=new Variable("filter res",SIP_FILTRESO,0x00) ;
	 Insert(reso_) ;

	 filterMix_=new Variable("filter type",SIP_FILTMIX,0x00) ;
	 Insert(filterMix_) ;

	 filterMode_=new Variable("filter mode",SIP_FILTMODE,filterMode,3,0) ;
	 Insert(filterMode_) ;

	 attenuate_=new Variable("attenuate",SIP_ATTENUATE,0xFF) ;
	 Insert(attenuate_) ;

	 start_=new WatchedVariable("start",SIP_START,0) ;
	 Insert(start_) ;
	 start_->AddObserver(*this) ;

	 loopMode_=new Variable("loopmode",SIP_LOOPMODE,loopTypes,SILM_LAST,0) ;
	 Insert(loopMode_) ;
	 loopMode_->SetInt(0) ;

	 slices_=new Variable("slices",SIP_SLICES,1) ;
	 Insert(slices_) ;

	 loopStart_=new WatchedVariable("loopstart",SIP_LOOPSTART,0) ;
	 Insert(loopStart_) ;
	 loopStart_->AddObserver(*this) ;

	 loopEnd_=new WatchedVariable("end",SIP_END,0) ;
	 Insert(loopEnd_) ;
	 loopEnd_->AddObserver(*this) ;

	 table_=new Variable("table",SIP_TABLE,-1) ;
	 Insert(table_) ;

	 tableAuto_=new Variable("table automation",SIP_TABLEAUTO,false) ;
	 Insert(tableAuto_) ;

	 fbTune_=new Variable("feedback tune",SIP_FBTUNE,0xB0) ;
	 Insert(fbTune_) ;

	 fbMix_=new Variable("feedback mix",SIP_FBMIX,0x00) ;
	 Insert(fbMix_) ;

     printFx_ = new Variable("print fx", SIP_PRINTFX, fxPresets, 4, 3);
     Insert(printFx_);

     irPad_ = new Variable("pad with silence", SIP_IR_PAD, 0);
     Insert(irPad_);

     irWet_ = new Variable("effect amount", SIP_IR_WET, 45);
     Insert(irWet_);

     reverb_ = new Variable("reverb", SIP_REVERB, 0);
     Insert(reverb_);

     delay_ = new Variable("delay", SIP_DELAY, 0);
     Insert(delay_);

     chorus_ = new Variable("chorus", SIP_CHORUS, 0);
     Insert(chorus_);

     mods_.Create(*this, MIK_SAMPLE);
     eq_.Create(*this);

     customName_ = new Variable("name", INSTRUMENT_NAME_ID, "");
     Insert(customName_);

     // Initalize instrument's voices update list

     for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
         renderParams *rp = renderParams_ + i;
         rp->fresh_ = false;
         rp->loopMode_ = SILM_ONESHOT;
         rp->markStart_ = rp->markLoop_ = rp->markEnd_ = 0;
         rp->sampleSize_ = 0;
         rp->noteFactor_ = 1.0f;
         rp->updaters_.push_back(&rp->volumeRamp_);
         rp->updaters_.push_back(&rp->panner_);
         rp->updaters_.push_back(&rp->cutRamp_);
         rp->updaters_.push_back(&rp->resRamp_);
         rp->updaters_.push_back(&rp->fbMixRamp_);
         rp->updaters_.push_back(&rp->fbTunRamp_);
         rp->updaters_.push_back(&rp->arp_);
         rp->updaters_.push_back(&rp->speedRamp_);
         rp->updaters_.push_back(&rp->legato_);
         rp->updaters_.push_back(&rp->pfin_);
         for (int m = 0; m < MOD_SLOT_COUNT; m++) {
             rp->updaters_.push_back(&rp->mods_[m]);
         }
         rp->modVolScale_ = 1.0f;
         for (int x = 0; x < RUX_LAST; x++) {
             rp->modExtra_[x] = 0.0f;
         }
         rp->baseLoopStart_ = 0;
         rp->loopModded_ = false;
         rp->releasing_ = false;
         rp->finished_ = true;
	} ;

 // Reset table state

	tableState_.Reset() ;
}

SampleInstrument::~SampleInstrument() {
}

bool SampleInstrument::Init() {

	SamplePool *pool=SamplePool::GetInstance() ;
    Variable *vSample=FindVariable(SIP_SAMPLE) ;
	NAssert(vSample) ;
	int index=vSample->GetInt() ;
	source_=(index>=0)?pool->GetSource(index):0 ;
	tableState_.Reset() ;
	return false ;
}

void SampleInstrument::OnStart() {
	tableState_.Reset() ;
} ;

// Mod slots can push values past their range; commands never did. Keep
// volume positive, pan inside the pan law table and the filter in 0..1.
static void clampModulated(renderParams *rp) {
	if (rp->volume_<0) rp->volume_=0 ;
	if (rp->pan_<0) rp->pan_=0 ;
	if (rp->pan_>i2fp(254)) rp->pan_=i2fp(254) ;
	if (rp->cutoff_<0) rp->cutoff_=0 ;
	if (rp->cutoff_>i2fp(1)) rp->cutoff_=i2fp(1) ;
	if (rp->reso_<0) rp->reso_=0 ;
	if (rp->reso_>i2fp(1)) rp->reso_=i2fp(1) ;
}

// Sums the command ramps and MOD slots into the voice (tick or k-rate)
static void applyUpdaterSums(renderParams *rp,bool withFilter) {
	struct RUParams rup ;
	rup.Reset() ;
	std::vector<I_SRPUpdater *>::iterator it ;
	for (it=rp->activeUpdaters_.begin();it!=rp->activeUpdaters_.end();it++) {
		(*it)->UpdateSRP(rup) ;
	}
	rp->modVolScale_=rup.volumeScale_ ;
	for (int x=0;x<RUX_LAST;x++) {
		rp->modExtra_[x]=rup.extra_[x] ;
	}
	rp->volume_=fp_mul(rp->baseVolume_+rup.volumeOffset_,fl2fp(rup.volumeScale_)) ;
	rp->speed_=fp_mul(rp->baseSpeed_,rup.speedOffset_) ;
	rp->pan_=rp->basePan_+rup.panOffset_ ;
	if (withFilter) {
		rp->cutoff_=rp->baseFCut_+rup.cutOffset_ ;
		rp->reso_=rp->baseFRes_+rup.resOffset_ ;
		rp->fbMix_=rp->baseFbMix_+rup.fbMixOffset_ ;
		rp->fbTun_=rp->baseFbTun_+rup.fbTunOffset_ ;
	}
	clampModulated(rp) ;
}

// Bit depth after MOD crush (more amount = fewer bits) and the drive
// before it
static int modulatedCrush(renderParams *rp) {
	int bits=rp->crush_-(int)(rp->modExtra_[RUX_CRUSH]+(rp->modExtra_[RUX_CRUSH]>=0?0.5f:-0.5f)) ;
	if (bits<1) bits=1 ;
	if (bits>16) bits=16 ;
	return bits ;
}

static int modulatedDrive(renderParams *rp) {
	int drive=rp->drive_+(int)rp->modExtra_[RUX_DRIVE] ;
	if (drive<0) drive=0 ;
	if (drive>255) drive=255 ;
	return drive ;
}

// Loop start (L) moved by MOD, as a share of the trimmed sample, in the
// looping play modes (the oscillator and looper sync modes size their
// pitch from L..E, so they keep it). Rebuilds the voice's loop path.
static void applyModLoopStart(renderParams *rp) {
	int mode=rp->loopMode_ ;
	if (!SampleInstrument::IsLoopingMode(mode) || SampleInstrument::IsOscMode(mode) ||
	    mode==SILM_LOOPSYNC) {
		return ;
	}
	if (rp->modExtra_[RUX_LOOP]==0.0f && !rp->loopModded_) {
		return ;   // untouched: LPOF and the marks rule
	}
	int S=rp->markStart_ ;
	int E=rp->markEnd_ ;
	int lo=S<E?S:E ;
	int hi=(S<E?E:S)-2 ;
	int span=E-S ;
	if (span<0) span=-span ;
	int loop=rp->baseLoopStart_+(int)(rp->modExtra_[RUX_LOOP]*span) ;
	if (loop>hi) loop=hi ;
	if (loop<lo) loop=lo ;
	if (loop<0) loop=0 ;
	rp->markLoop_=loop ;
	int first,end,restart ;
	SampleInstrument::PlayPath(mode,S,loop,E,first,end,restart) ;
	rp->rendLoopStart_=restart ;
	rp->rendLoopEnd_=end ;
	rp->loopModded_=(rp->modExtra_[RUX_LOOP]!=0.0f) ;
}

// An ADSR on volume still fading this voice out
static bool volumeReleaseRunning(renderParams *rp) {
	for (int m=0;m<MOD_SLOT_COUNT;m++) {
		ModSource &mod=rp->mods_[m] ;
		if (mod.Enabled() && mod.GetType()==MT_ADSR && mod.GetDest()==MD_VOLUME &&
		    mod.GetAmount()>0.0f && !mod.IsDone()) {
			return true ;
		}
	}
	return false ;
}

bool SampleInstrument::Start(int channel,unsigned char midinote,bool cleanstart)
{
	// Look if we're dirty & need to update this instrument's data

  if (dirty_) 
  {
  updateInstrumentData(false) ;
  }

  running_=true ;

  if (source_==0) return false ;

  // Get Rendering params for current voice & fill init data

  renderParams *rp=renderParams_+channel ;

  rp->midiNote_=midinote ;
  
  if (lastMidiNote_[channel] == -1) // To prevent First LEGA to go bonkers
  {
    lastMidiNote_[channel]=midinote ;
  }

 // Duplicate variable value to local rendering
 // to alter with commands

	 rp->sampleBuffer_=source_->GetSampleBuffer(rp->midiNote_) ;
	 if (rp->sampleBuffer_==0) {
		 return false ;
	 } ;
	 rp->channelCount_=source_->GetChannelCount(rp->midiNote_) ;

	 int rootNote=(rootNote_->GetInt()-60)+source_->GetRootNote(rp->midiNote_) ;

	 rp->volume_=rp->baseVolume_=i2fp(volume_->GetInt()) ;
	 rp->attenuate_=i2fp(attenuate_->GetInt()) ;

	 rp->pan_=rp->basePan_=i2fp(pan_->GetInt()) ;

	 int markStart=start_->GetInt() ;
	 int markLoop=loopStart_->GetInt() ;
	 int markEnd=loopEnd_->GetInt() ;
	 int voiceMode=loopMode_->GetInt() ;
	 if (source_->IsMulti()) {
		 // SoundFont presets bring their own loop
		 long start=source_->GetLoopStart(rp->midiNote_)  ;
		 markStart=0 ;
		 if (start>0) {
			 markLoop=source_->GetLoopStart(rp->midiNote_); // hack at the moment
			 markEnd=source_->GetLoopEnd(rp->midiNote_) ;
			 loopMode_->SetInt(SILM_LOOP) ;
		 } else {
			 markLoop=0; // hack at the moment
			 markEnd=source_->GetSize(rp->midiNote_) ; // hack at the moment
			 loopMode_->SetInt(SILM_ONESHOT) ;
		 } ;
		 voiceMode=loopMode_->GetInt() ;
	 }
     rp->releasing_=false ;

     int isSliced = slices_->GetInt() > 1;
     if (isSliced) {
		 if (rp->midiNote_ > slices_->GetInt() - 1) return false; // No sound outside of slice range
		 // Each slice is its own little sample: S and L at its start, E at
		 // its end, so every play mode (reverse too) works per slice
		 int slice = markEnd / slices_->GetInt();
		 markStart = markLoop = rp->midiNote_ * slice;
		 markEnd = (rp->midiNote_ + 1) * slice;
	 }
	 if (voiceMode<0 || voiceMode>=SILM_LAST) voiceMode=SILM_ONESHOT ;

	 rp->markStart_=markStart ;
	 rp->markLoop_=markLoop ;
	 rp->markEnd_=markEnd ;
	 rp->loopMode_=voiceMode ;
	 rp->sampleSize_=source_->GetSize(rp->midiNote_) ;

        // Compute octave & note difference from root
        float fineTune = float(fineTune_->GetInt() - 0x7F);
        fineTune /= float(0x80);
        int offset = midinote - rootNote;
        if (isSliced) {
            offset = rootNote - source_->GetRootNote(rp->midiNote_);
    }
    while (offset > 127) {
        offset -= 12;
    }
	rp->noteFactor_=float(pow(2.0,(offset+fineTune)/12.0)) ;

	setupVoicePlayback(rp,cleanstart,true) ;
    rp->fresh_=true ;
    
  // Init k rate counter
 
    rp->krateCount_=0 ;

	// We allow processing

	rp->finished_=false ;

  // Initialize feedback data

	memset(feedback_[channel],0,FB_BUFFER_LENGTH*2*sizeof(fixed)) ;

	rp->feedbackIn_=0 ;
	rp->feedbackMode_=FB_NONE ;

	rp->baseFbTun_=rp->fbTun_=fl2fp(fbTune_->GetInt()/255.0f) ; 
	rp->baseFbMix_=rp->fbMix_=fl2fp(fbMix_->GetInt()/255.0f) ; 

  // If we do a clean start (there was a instr number on the line)

	if (cleanstart) 
  {

	// Clear retrigger data
	  
		rp->retrig_=false ;
		rp->retrigLoop_=0 ;
		rp->retrigCount_=0 ;
		rp->retrigOffset_=0 ;

	// Could click

		rp->couldClick_=SHOULD_KILL_CLICKS ;

	// Init filter params

		rp->cutoff_=rp->baseFCut_=fl2fp(cutoff_->GetInt()/255.0f); 
		rp->reso_=rp->baseFRes_=fl2fp(reso_->GetInt()/255.0f) ;

	// Init crush params

		rp->crush_=crush_->GetInt() ;
		rp->drive_=drive_->GetInt() ;

	// Init downsampling

		rp->downsample_=downsample_->GetInt() ;

		// Disable all updaters for new voice
		
		std::vector<I_SRPUpdater *>::iterator it ;

		for (it=rp->updaters_.begin();it!=rp->updaters_.end();it++) {
			I_SRPUpdater *current=*it ;
			current->Disable() ;
		}

		rp->activeUpdaters_.clear() ;
	}

	// The EQ page's filters start from silence, like the voice
	if (cleanstart) {
		eq_.ResetVoice(channel) ;
	}

	// Envelopes and LFOs from the MOD page restart with every note, with or
	// without an instrument number on the step, and the note starts with
	// their first values
	float krateHz=Audio::GetInstance()->GetSampleRate()/(float)KRATE_SAMPLE_COUNT ;
	mods_.StartVoice(rp->mods_,rp->activeUpdaters_,krateHz,midinote,channel,channel*131+midinote) ;
	applyUpdaterSums(rp,true) ;
	rp->baseLoopStart_=rp->markLoop_ ;
	rp->loopModded_=false ;
	applyModLoopStart(rp) ;

	// Sample start moved by MOD (key tracking, a free LFO...): a share of
	// the way from where the note starts to where its first pass ends, in
	// the play direction (not in the oscillator / looper sync modes)
	int mode=rp->loopMode_ ;
	if (rp->modExtra_[RUX_START]!=0.0f && !IsOscMode(mode) && mode!=SILM_LOOPSYNC) {
		int first=rp->rendFirst_ ;
		int end=rp->rendLoopEnd_ ;
		float pos=first+rp->modExtra_[RUX_START]*(end-first) ;
		float lo=(float)(first<end?first:end) ;
		float hi=(float)(first<end?end:first) ;
		if (first<end) hi-=2.0f ;   // stays inside the forward pass
		else lo+=1.0f ;
		if (hi>rp->sampleSize_-2) hi=(float)(rp->sampleSize_-2) ;
		if (lo<0.0f) lo=0.0f ;
		if (pos<lo) pos=lo ;
		if (pos>hi) pos=hi ;
		rp->position_=pos ;
	}
	return true ;
}

// The voice's path through the sample for its play mode. Three numbers
// drive the renderer: rendFirst_ (where the note starts), rendLoopEnd_
// (where every pass ends, in the direction of travel) and rendLoopStart_
// (where a loop jumps back to). Reverse modes are the forward ones mirrored:
// they start at E and run down. Old songs whose E sits before S (the way
// LGPT used to reverse) keep playing backwards in the forward modes.
void SampleInstrument::setupVoicePlayback(renderParams *rp,bool cleanstart,bool reposition) {
	int mode=rp->loopMode_ ;
	int S=rp->markStart_ ;
	int L=rp->markLoop_ ;
	int E=rp->markEnd_ ;
	bool osc=IsOscMode(mode) || mode==SILM_LOOPSYNC ;

	int first,end,restart ;
	PlayPath(mode,S,L,E,first,end,restart) ;
	// E is exclusive (a forward pass stops before it): going down, the
	// first frame heard is the one before E. A backward loop jumps to just
	// under its restart point the same way (see Render).
	if (IsReverseMode(mode)) first-=1 ;
	// The interpolation also reads the frame after the playhead: never
	// start on the very last frame of the sample
	int lastStart=rp->sampleSize_-2 ;
	if (lastStart<0) lastStart=0 ;
	if (end<first && first>lastStart) first=lastStart ;
	if (first<0) first=0 ;
	rp->rendFirst_=first ;
	rp->rendLoopStart_=restart ;
	rp->rendLoopEnd_=end ;

	float driverRate=float(Audio::GetInstance()->GetSampleRate()) ;
	float base ;
	if (IsOscMode(mode)) {
		// L..E is one cycle at C3 (there and back for ping-pong)
		float freq=261.6255653006f ;
		int length=E-L ;
		if (length<0) length=-length ;
		if (length==0) length=1 ;
		if (mode==SILM_OSC_PINGPONG) length*=2 ;
		base=(freq*length)/driverRate ;
	} else if (mode==SILM_LOOPSYNC) {
		int length=E-L ;
		if (length<0) length=-length ;
		SyncMaster *sm=SyncMaster::GetInstance() ;
		int sampleCount=int(sm->GetTickSampleCount()) ;
		sampleCount*=(6*16) ;
		base=length/float(sampleCount) ;
	} else {
		// A sample recorded below the output rate travels slower
		base=source_->GetSampleRate(rp->midiNote_)/driverRate ;
	}
	rp->baseSpeed_=fl2fp(base*rp->noteFactor_) ;
	rp->speed_=rp->baseSpeed_ ;

	// Oscillator modes keep their phase on a retrigger without instrument;
	// otherwise the note starts at its first frame. Without reposition (a
	// PLAY command on a sounding note) it carries on from where it is, in
	// the new mode's direction.
	if (reposition && (cleanstart || !osc)) {
		rp->position_=float(first) ;
	}
	rp->reverse_=(end<first) ;
}

void SampleInstrument::PlayPath(int mode,int S,int L,int E,int &first,int &end,int &restart) {
	if (!IsReverseMode(mode)) {
		bool osc=IsOscMode(mode) || mode==SILM_LOOPSYNC ;
		first=osc?L:S ;
		end=E ;
		restart=L ;
	} else {
		first=E ;
		end=(mode==SILM_REVERSE)?S:L ;
		restart=E ;
	}
}

bool SampleInstrument::IsReverseMode(int mode) {
	return mode==SILM_REVERSE || mode==SILM_REVLOOP ||
	       mode==SILM_REV_PINGPONG || mode==SILM_OSC_REV ;
}

bool SampleInstrument::IsLoopingMode(int mode) {
	return mode!=SILM_ONESHOT && mode!=SILM_REVERSE ;
}

bool SampleInstrument::IsPingPongMode(int mode) {
	return mode==SILM_LOOP_PINGPONG || mode==SILM_REV_PINGPONG ||
	       mode==SILM_OSC_PINGPONG ;
}

bool SampleInstrument::IsOscMode(int mode) {
	return mode==SILM_OSC || mode==SILM_OSC_REV || mode==SILM_OSC_PINGPONG ;
}

int SampleInstrument::WithLooping(int mode,bool loop) {
	if (loop) {
		if (mode==SILM_ONESHOT) return SILM_LOOP ;
		if (mode==SILM_REVERSE) return SILM_REVLOOP ;
		return mode ;
	}
	return IsReverseMode(mode)?SILM_REVERSE:SILM_ONESHOT ;
}

const char *SampleInstrument::CanonicalLoopModeName(const char *saved) {
	if (!saved) return saved ;
	static const char *legacy[][2]={
		{"none","forward"},
		{"ping pong","pingpong"},
		{"oscillator","osc"},
		{"looper sync","loop-sync"},
		{0,0}
	} ;
	for (int i=0;legacy[i][0];i++) {
		if (!strcmp(saved,legacy[i][0])) return legacy[i][1] ;
	}
	return saved ;
}

const char *SampleInstrument::GetLoopModeName(int mode) {
	if (mode<0 || mode>=SILM_LAST) return "?" ;
	return loopTypes[mode] ;
}

void SampleInstrument::ReplaceSample(int index,int start,int loopStart,int end) {
	FindVariable(SIP_SAMPLE)->SetInt(index) ;
	// Take it now: while a voice plays, the variable's observer only marks
	// the instrument dirty, and the next note would reset the markers
	updateInstrumentData(false) ;
	start_->SetInt(start) ;
	loopStart_->SetInt(loopStart) ;
	loopEnd_->SetInt(end) ;
	suggestedRootNote_=-1 ;
	SetChanged() ;
	NotifyObservers() ;
}

void SampleInstrument::Stop(int channel) {

	 renderParams *rp=renderParams_+channel ;
	 running_=false ;
	 // Note-off / KILL: ADSR slots release. One on volume keeps the sample
	 // playing while it fades (the channel holds it as a release tail)
	 for (int m=0;m<MOD_SLOT_COUNT;m++) {
		 rp->mods_[m].NoteOff() ;
	 }
	 rp->releasing_=!rp->finished_ && volumeReleaseRunning(rp) ;
}

bool SampleInstrument::IsReleasing(int channel) {
	renderParams *rp=renderParams_+channel ;
	return rp->releasing_ && !rp->finished_ ;
}

// Transport stop: no release tail
void SampleInstrument::StopQuickly(int channel) {
	Stop(channel) ;
	renderParams_[channel].releasing_=false ;
}

void SampleInstrument::AllNotesOff() {
	// Mark every voice finished: Stop() only flips the shared running_ flag
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		renderParams_[i].finished_=true ;
		renderParams_[i].releasing_=false ;
	}
	running_=false ;
}

void SampleInstrument::doTickUpdate(int channel) {

  // Process updaters

	renderParams *rp=renderParams_+channel ;
	std::vector<I_SRPUpdater *>::iterator it ;

	for (it=rp->activeUpdaters_.begin();it!=rp->activeUpdaters_.end();it++) {
		I_SRPUpdater *current=*it ;
		current->Trigger(true) ;
	}
} ;

void SampleInstrument::doKRateUpdate(int channel) {

	renderParams *rp=renderParams_+channel ;
	std::vector<I_SRPUpdater *>::iterator it ;

	for (it=rp->activeUpdaters_.begin();it!=rp->activeUpdaters_.end();it++) {
		I_SRPUpdater *current=*it;
		current->Trigger(false) ;
	}
	
} ;

void SampleInstrument::updateFeedback(renderParams *rp) {

	 if (rp->fbMix_!=0) {
		int offset=fp2i(fp_mul(rp->fbTun_,fl2fp(255.0f))) ;

		if (IsOscMode(rp->loopMode_)) {
				if (offset<0x80) {
					offset=FB_BUFFER_LENGTH-offset-1 ;
					rp->feedbackMode_=FB_ADD ;
				} else {
					offset=FB_BUFFER_LENGTH-(0x100-offset) ;
					rp->feedbackMode_=FB_SUB ;
				}
		} else {
                rp->feedbackMode_ = FB_ADD;
                if (offset < 0x80) {
                    offset = FB_BUFFER_LENGTH - offset - 1;
                } else {
                    offset = FB_BUFFER_LENGTH - offset * 10 - 1;
                }
		}
		rp->feedbackOut_=rp->feedbackIn_+offset ;
		if (rp->feedbackOut_>=FB_BUFFER_LENGTH) {
			rp->feedbackOut_-=FB_BUFFER_LENGTH ;
		}
	} ;
}

// Size in samples

bool SampleInstrument::Render(int channel,fixed *buffer,int size,bool updateTick) {

  bool somethingToMix=false ;

  // Get Current render parameters
	  
  renderParams *rp=renderParams_+channel ;
  lastMidiNote_[channel]=rp->midiNote_ ;
	 bool *rpFinished=&(rp->finished_) ;

     if (source_) {

		 if (*rpFinished) return false ;	
		 
		 // clear the fixed point buffer

		 SYS_MEMSET(buffer,0,size*2*sizeof(fixed)) ;


		 bool hasUpdaters=!(rp->activeUpdaters_.empty()) ;

		 int filterMix=filterMix_->GetInt() ;
		 FilterMode filterMode=(FilterMode)filterMode_->GetInt() ;
		 bool filterBoost=(filterMode==FM_SCREAM) ;
		 bool bassyFilter=(filterMode==FM_BASSY) ;

		 // Be sure filters are properly initialized

		 set_filter(channel,FLT_LOWPASS,rp->cutoff_,rp->reso_,filterMix,bassyFilter);

		 filter_t* flt =get_filter(channel) ;
		 bool filtering=(rp->cutoff_<i2fp(1))||(rp->reso_>i2fp(0)) ;

	// Process tick-level updates
	  
		 if (updateTick) {

			 if (hasUpdaters) {

				 doTickUpdate(channel) ;
				 applyUpdaterSums(rp,false) ;
			}

		// Process retrig
		  
			if (rp->retrig_) {
				if (rp->retrigCount_==0) {
					int ticks=rp->retrigOffset_-rp->retrigLoop_ ;
					long offset=long(ticks*SyncMaster::GetInstance()->GetTickSampleCount()) ;
					rp->position_+=offset*fp2fl(rp->speed_) ;
					if (rp->position_<0) {
						rp->position_=0 ;
					} ;
					rp->retrigCount_=rp->retrigLoop_ ;
					// each re-strike restarts the MOD envelopes too
					for (int m=0;m<MOD_SLOT_COUNT;m++) {
						if (rp->mods_[m].Enabled()) rp->mods_[m].Retrigger() ;
					}
				}
				rp->retrigCount_-- ;
			} ;


		}
		
	  // Get additional parameters from variables


		 // Crush 

		 int shift=16-modulatedCrush(rp);
	     fixed mask=0xFFFFFFFF ;
		 if (shift !=0) {
			 mask<<=FIXED_SHIFT+shift  ;
		 }

		 // Crush vol

		 int crushvol=modulatedDrive(rp) ;
		 fixed fpcrushvol=fl2fp(crushvol/255.0F) ;

		 // downsample

		 int downsmpl=rp->downsample_ ;
		 unsigned int dsMask=0xFFFFFFFF<<downsmpl ;

		 // Play mode of this voice

		 int loopMode=rp->loopMode_ ;
		 bool looping=IsLoopingMode(loopMode) ;
		 bool pingpong=IsPingPongMode(loopMode) ;

		 // Interpolation

		 int interpol=interpolation_->GetInt() ;

		 // Get sound characteristics
 

		char *wavbuf=(char *)rp->sampleBuffer_ ;

		int channelCount=rp->channelCount_ ;

		int count=size ; // number of samples to treat

		fixed *result=buffer ;

		// Get volume factor and pan

		fixed volscale=fl2fp(0.003921568627450980392156862745098f) ;
		fixed volfactor=fp_mul(rp->volume_,volscale) ;

		// Filter attenuate
		fixed fpattenuate=fp_mul(rp->attenuate_,volscale) ;

	  int pan=fp2i(rp->pan_) ;
		fixed fixedpanl=panlaw[pan] ;
		fixed fixedpanr=panlaw[254-pan] ;

		// filter constants

		fixed f_k=fl2fp( 1.0F / 3.0F ) ;
		fixed f_s=FP_ONE - f_k ;

		// Get pan multiplicators, and take volume into account

    int n=int(rp->position_) ;
    short *input=(short *)(wavbuf+2*channelCount*n) ; // input is the current
                                                     // sample to the left of position

		fixed fpPos=fl2fp(rp->position_-n) ;  // fpPos is current pos from input
		fixed fpSpeed=rp->speed_ ;     // speed in fixed
		if (rp->reverse_)
    {
			fpSpeed=-rp->speed_ ; 
		} 

    fixed s1,s2,t2,eta,inveta;
		s2=0 ; t2=0 ;

    // MOD on the loop start: once per buffer, before the path is laid out
    applyModLoopStart(rp) ;

    // Boundaries of this voice's path, in frames (see setupVoicePlayback).
    // Every pointer stays where input and input+1 can both be read.
    short *base=(short *)wavbuf ;
    int lastFrame=rp->sampleSize_-2 ;
    if (lastFrame<0) lastFrame=0 ;
    int endFrame=rp->rendLoopEnd_ ;
    if (endFrame>rp->sampleSize_) endFrame=rp->sampleSize_ ;
    if (endFrame<0) endFrame=0 ;
    // A loop restarts in the direction from its restart point to its end.
    // Going up it restarts on L; going down just under its restart point
    // (E is exclusive, like a forward pass's end)
    bool restartReverse=(rp->rendLoopEnd_<rp->rendLoopStart_) ;
    int restartFrame=rp->rendLoopStart_ ;
    if (!restartReverse && restartFrame>endFrame-2) restartFrame=endFrame-2 ;
    if (restartFrame>lastFrame+1) restartFrame=lastFrame+1 ;
    if (restartFrame<1 && restartReverse) restartFrame=1 ;
    if (restartFrame<0) restartFrame=0 ;
    // Ping-pong bounces between L and the frame before E
    int loFrame=rp->rendLoopStart_<rp->rendLoopEnd_?rp->rendLoopStart_:rp->rendLoopEnd_ ;
    int hiFrame=rp->rendLoopStart_<rp->rendLoopEnd_?rp->rendLoopEnd_:rp->rendLoopStart_ ;
    if (hiFrame>rp->sampleSize_) hiFrame=rp->sampleSize_ ;
    if (loFrame<0) loFrame=0 ;
    if (loFrame>lastFrame) loFrame=lastFrame ;
    short *endFwd=base+(endFrame-1)*channelCount ;   // a forward pass stops here
    short *endBack=base+endFrame*channelCount ;      // a backward pass stops below
    short *restartPtr=base+restartFrame*channelCount ;
    short *loPtr=base+loFrame*channelCount ;         // ping-pong turns
    short *hiPtr=base+(hiFrame-1)*channelCount ;
    if (hiPtr<loPtr+channelCount) hiPtr=loPtr+channelCount ;

    fixed zerofive=fl2fp(0.5f) ;

		// Get feedback pointer position & boundary

		fixed *feedbackIn=feedback_[channel]+rp->feedbackIn_*2 ;
		fixed *feedbackStart=feedback_[channel] ;
		fixed *feedbackEnd=feedback_[channel]+FB_BUFFER_LENGTH*2 ;

		// Update feedback data if necessary

		updateFeedback(rp) ;

		fixed *feedbackPick=feedback_[channel]+rp->feedbackOut_*2 ;
		fixed feedbackEta=fp_mul(rp->fbMix_,fl2fp(4.0f));

		fixed f_32767=i2fp(32767/4) ;
		fixed f_m32768=i2fp(-32768/4) ;

		// try to speed up access using pointers rather than structure access

		bool rpReverse=rp->reverse_ ;
		int rpKrateCount=rp->krateCount_ ;
		FeedbackMode rpFeedbackMode=rp->feedbackMode_ ;

		fixed *fltSpeed=flt->speed ;
		fixed *fltHeight=flt->height ;
		fixed fltMix=flt->mix ;
		fixed fltMixInv=FP_ONE-fltMix ;
		fixed *fltDelay=flt->hipdelay ;
		fixed fltParm1=flt->freq ;
		fixed fltParm2=flt->reso ;
		fixed fltDirt=flt->dirt ;

		fixed *fltSpeedPtr=0 ;
		fixed *fltDelayPtr=0 ;
		fixed *fltHeightPtr=0 ;

    short *dsBasePtr = ((short *)wavbuf) + rp->rendFirst_* channelCount;
       
		while (count>0) {

			// look where we are, if we need to

			if (!rpReverse) { // going forward
				if (pingpong) {
					if (input>=hiPtr) {
						// turn around: the position mirrored at E's last frame
						if (fpPos==0) {
							input=hiPtr-(input-hiPtr) ;
						} else {
							input=hiPtr-(input-hiPtr)-channelCount ;
							fpPos=FP_ONE-fpPos ;
						}
						if (input>=hiPtr) {
							// that frame itself: read it as the far end of
							// its left neighbour (input+1 must stay inside)
							input=hiPtr-channelCount ;
							fpPos=FP_ONE-1 ;
						}
						if (input<loPtr) input=loPtr ;
						rpReverse=true ;
						fpSpeed=-rp->speed_ ;
					}
				} else if (input>=endFwd) {
					if (!looping) {
						*rpFinished = true;
					} else {
						// back to the loop start, keeping what we overshot
						rpReverse=restartReverse ;
						if (!rpReverse) {
							input=restartPtr+(input-endFwd) ;
							if (input>=endFwd) input=restartPtr ;
							fpSpeed=rp->speed_ ;
						} else {
							input=restartPtr-channelCount ;
							fpSpeed=-rp->speed_ ;
						}
					}
				}
			} else { // going backward
				if (pingpong) {
					if (input<loPtr) {
						// turn around, mirrored at L
						if (fpPos==0) {
							input=loPtr+(loPtr-input) ;
						} else {
							input=loPtr+(loPtr-input)-channelCount ;
							fpPos=FP_ONE-fpPos ;
						}
						if (input>=hiPtr) input=hiPtr-channelCount ;
						if (input<loPtr) input=loPtr ;
						rpReverse=false ;
						fpSpeed=rp->speed_ ;
					}
				} else if (input<endBack) {
					if (!looping) {
						*rpFinished = true;
					} else {
						rpReverse=restartReverse ;
						if (rpReverse) {
							// as far under the restart point as we went past
							// the end
							input=restartPtr+(input-endBack) ;
							if (input<endBack) input=restartPtr-channelCount ;
							fpSpeed=-rp->speed_ ;
						} else {
							input=restartPtr ;
							fpSpeed=rp->speed_ ;
						}
					}
				}
			};

      if (*rpFinished) {
				count=-1 ;
			} else {

	  	    // See if time to process k-rate change

			  if (rpKrateCount--==0) {
					rpKrateCount=KRATE_SAMPLE_COUNT ;

					if (hasUpdaters) {
						doKRateUpdate(channel) ;
						applyUpdaterSums(rp,true) ;

						// MOD on crush / drive / loop start
						int crushShift=16-modulatedCrush(rp) ;
						mask=0xFFFFFFFF ;
						if (crushShift!=0) {
							mask<<=FIXED_SHIFT+crushShift ;
						}
						fpcrushvol=fl2fp(modulatedDrive(rp)/255.0F) ;

						// Note-off faded out by an ADSR on volume: done
						if (rp->releasing_ && !volumeReleaseRunning(rp)) {
							*rpFinished=true ;
						}

						set_filter(channel,FLT_LOWPASS,rp->cutoff_,rp->reso_,filterMix,bassyFilter);
						filtering=(rp->cutoff_<i2fp(1))||(rp->reso_>i2fp(0)) ;

						rp->feedbackIn_ = (feedbackIn-feedback_[channel])/2 ;

						updateFeedback(rp) ;
						feedbackPick=feedback_[channel]+rp->feedbackOut_*2 ;
						feedbackEta=fp_mul(rp->fbMix_,fl2fp(4.0f));

						volfactor=fp_mul(rp->volume_,volscale) ;
						fpattenuate=fp_mul(rp->attenuate_,volscale) ;
						pan=fp2i(rp->pan_) ;
						fixed fixedpanl=panlaw[pan] ;
						fixed fixedpanr=panlaw[254-pan] ;

						if (rpReverse) {
							fpSpeed=-rp->speed_ ;
						} else {
							fpSpeed=rp->speed_ ;
						}
					}
			  }

		    // get input sample to interpolate from
			  // s= left channel
		    // t= right channel

			  short *i1=input;
        if (dsMask!=0xFFFFFFFF)
        {
	        if (useDirtyDownsampling_)
	        {
#ifdef _64BIT
	          i1 =(short *)(((long)input)&dsMask);
#else
	          i1 =(short *)(((unsigned int)input)&dsMask);
#endif
	        }
          else
	        {
            unsigned int distance = (unsigned int)(input - dsBasePtr) /channelCount;
            i1 = dsBasePtr+(distance&dsMask)*channelCount ;
          }
        }

        short *i2=i1+channelCount ;

				if (filtering) 
        {
					fltSpeedPtr=fltSpeed ;
					fltHeightPtr=fltHeight ;
					fltDelayPtr=fltDelay ;
				}

				for (int i=0;i<channelCount;i++) 
        {
					t2=s2 ; // move L to R if necessary
					s1=i2fp(*i1++) ;
          s2=i2fp(*i2++) ;

					switch(interpol) {

						case 0: // Linear interpolation
						
							eta=fpPos ;
							inveta=fp_sub(FP_ONE,eta) ;
		           
							// interpolate

			  				s1=fp_mul(s1,inveta) ;
							s2=fp_mul(s2,eta) ;

							// Compute interpolated sample

    					s1+=s2 ;
							break ;
							
						case 1: // Nearest neighbor
						
							if (fpPos>zerofive) {
								s1=s2 ;
							} ;
							break ;
					}

				// apply feedback mix

					switch(rpFeedbackMode) {
						case FB_NONE:

							feedbackPick++ ;
							break ;

						case FB_ADD:

							s2=*feedbackPick++ ;
							if (s2>f_32767) s2=f_32767 ;
							if (s2<f_m32768) s2=f_m32768 ;
							s2=fp_mul(s2,feedbackEta) ;
							s1=fp_add(s1,s2) ;
							break ;

						case FB_SUB:

							s2=*feedbackPick++ ;
							if (s2>f_32767) s2=f_32767 ;
							if (s2<f_m32768) s2=f_m32768 ;
							s2=fp_mul(s2,feedbackEta) ;
							s1=fp_sub(s1,s2) ;
							break ;
					}

				// crush predrive

					s2=fp_mul(s1,fpcrushvol) ;

				// store result, applying crush

					s2=(s2&mask);

				// apply volume

          s2=fp_mul(s2,volfactor) ;

				// apply filtering if needed

					if (filtering) {

						fixed lpin =fp_mul(s2,fltMixInv) ;
      			fixed hpin = -fp_mul(s2,fltMix) ;
							
    				fixed difr = fp_sub(lpin,*fltHeightPtr);

						// Introduce non-linearity if screamin'

						if (filterBoost) {
							if (*fltSpeedPtr<-FP_ONE) {
								*fltSpeedPtr=-f_s ;
							} else if (fltSpeed[i]>FP_ONE) {
								*fltSpeedPtr=f_s ;
							};
							*fltSpeedPtr=fp_mul(*fltSpeedPtr,fltDirt) ;
						}

						*fltSpeedPtr = fp_mul(*fltSpeedPtr,fltParm2);		//mul by res, it's some kind of inertia. caution to feedback
/*HOG:5*/				*fltSpeedPtr = fp_add(*fltSpeedPtr,fp_mul(difr,fltParm1)); //mul by cutoff, less cutoff = no sound, so it's better not be 0.

						*fltHeightPtr += *fltSpeedPtr ;
						*fltHeightPtr += *fltDelayPtr-hpin ;
						s2=*fltHeightPtr ;

						*fltDelayPtr=hpin ;
						fltDelayPtr++ ;
						fltHeightPtr++ ;
						fltSpeedPtr++ ;
					}
					// apply attenuation
					s2=fp_mul(s2,fpattenuate) ;
				}

				if (channelCount==1) {
					t2=s2 ;
					feedbackPick++ ;
				}

				if (feedbackPick>=feedbackEnd) {
					feedbackPick=feedbackStart ;
				} ;

				// introduce panning & vol - store result

				s2=fp_mul(s2,fixedpanl) ;
				t2=fp_mul(t2,fixedpanr) ;

				*result++=s2 ;
				*result++=t2 ;

				*feedbackIn++=s2 ;
				*feedbackIn++=t2 ;

				if (feedbackIn>=feedbackEnd) {
					feedbackIn=feedbackStart ;
				} ;

			// Computes new pos for next input sample
			// fpPos is always relative to 'input' pointer
		
			   fpPos=fp_add(fpPos,fpSpeed) ;
         int delta=fp2i(fpPos) ;
         input+=channelCount*delta;
         fpPos = fp_sub(fpPos,i2fp(delta)) ;
			   count-- ;
			}
    }
    // Update 'reverse' mode if changed

    rp->reverse_ = rpReverse;
    rp->fresh_ = false; // a PLAY from now on changes a sounding note

		// Update final sample position
    rp->position_=(((char *)input)-wavbuf)/(2*channelCount)+fp2fl(fpPos) ;

		// Update feedback position

		rp->feedbackIn_=(feedbackIn-feedbackStart)/2 ;
		rp->feedbackOut_=(feedbackPick-feedbackStart)/2 ;
		somethingToMix=true ;
    }

    if (somethingToMix) {
      // The instrument's own EQ (EQ page), before the sends
      if (eq_.Prepare()) {
        eq_.ProcessStereo(channel,buffer,size) ;
      }
      sendToEffects(channel,buffer,size) ;
    }
    return somethingToMix ; 
} ;

// Copy the rendered voice to the shared reverb / echo (SendFX), like synths
void SampleInstrument::sendToEffects(int channel,fixed *buffer,int size) {
  // MOD slots aimed at the sends move them per voice
  renderParams *rp=renderParams_+channel ;
  float reverb=reverb_->GetInt()/255.0f+rp->modExtra_[RUX_REVERB] ;
  float delay=delay_->GetInt()/255.0f+rp->modExtra_[RUX_DELAY] ;
  float chorus=chorus_->GetInt()/255.0f+rp->modExtra_[RUX_CHORUS] ;
  reverb=reverb<0.0f?0.0f:(reverb>1.0f?1.0f:reverb) ;
  delay=delay<0.0f?0.0f:(delay>1.0f?1.0f:delay) ;
  chorus=chorus<0.0f?0.0f:(chorus>1.0f?1.0f:chorus) ;
  if (reverb<=0.0f && delay<=0.0f && chorus<=0.0f) return ;
  static float send[SENDFX_MAX_FRAMES*2] ;
  int frames=size<SENDFX_MAX_FRAMES ? size : SENDFX_MAX_FRAMES ;
  for (int i=0;i<frames*2;i++) {
    send[i]=fp2fl(buffer[i])/32767.0f ;
  }
  SendFX::GetInstance()->AddSend(channel,send,frames,reverb,delay,chorus) ;
}


void SampleInstrument::AssignSample(int i) {

	 Variable *v=FindVariable(SIP_SAMPLE) ;
	 v->SetInt(i) ;
	 suggestedRootNote_=-1;
} ;

static bool isSampleNameTokenBoundary(char c) {
	if (c==0) return true;
	if (isalnum((unsigned char)c)) return false;
	return true;
}

static bool containsTokenNoCase(const char *name,const char *token) {
	if (!name || !token) return false;
	int tokenLen=strlen(token);
	if (tokenLen<=0) return false;
	for (const char *p=name;*p;p++) {
		if (p!=name && !isSampleNameTokenBoundary(*(p-1))) continue;
		int j=0;
		while (j<tokenLen && p[j] &&
		       tolower((unsigned char)p[j])==tolower((unsigned char)token[j])) {
			j++;
		}
		if (j==tokenLen && isSampleNameTokenBoundary(p[j])) return true;
	}
	return false;
}

static double sampleRootCorrelation(short *buffer,int channels,int start,int window,int lag) {
	double sum=0.0;
	double e1=0.0;
	double e2=0.0;
	for (int n=0;n<window-lag;n++) {
		double a=(double)buffer[(start+n)*channels];
		double b=(double)buffer[(start+n+lag)*channels];
		sum+=a*b;
		e1+=a*a;
		e2+=b*b;
	}
	if (e1<=0.0 || e2<=0.0) return 0.0;
	return sum/sqrt(e1*e2);
}

static bool looksPercussiveByName(const char *name) {
	const char *tokens[]={
		"kick","snare","hat","hihat","clap","rim","tom","cym","crash",
		"ride","drum","perc","shaker","noise","vinyl","fx",0
	};
	for (int i=0;tokens[i];i++) {
		if (containsTokenNoCase(name,tokens[i])) return true;
	}
	return false;
}

static int frequencyToMidi(float freq) {
	if (freq<=0.0f) return -1;
	int note=(int)(69.0f+12.0f*(log(freq/440.0f)/log(2.0f))+0.5f);
	if (note<0) note=0;
	if (note>127) note=127;
	return note;
}

int SampleInstrument::DetectRootNoteSuggestion() {
	return DetectRootNoteSuggestionInRange(0,-1);
}

int SampleInstrument::DetectRootNoteSuggestionFromTrim() {
	int start=start_?start_->GetInt():0;
	int end=loopEnd_?loopEnd_->GetInt():-1;
	return DetectRootNoteSuggestionInRange(start,end);
}

int SampleInstrument::DetectRootNoteSuggestionInRange(int rangeStart, int rangeEnd) {
	int sampleIndex=GetSampleIndex();
	if (sampleIndex<0 || sampleIndex>=MAX_SAMPLEINSTRUMENT_COUNT) return -1;
	SamplePool *pool=SamplePool::GetInstance();
	SoundSource *source=pool->GetSource(sampleIndex);
	const char *name=pool->GetName(sampleIndex);
	if (!source || looksPercussiveByName(name)) {
#ifdef PLATFORM_RGNANO_SIM
		Trace::Log("SAMPLE_AUTOROOT","skip name=%s",name?name:"(null)");
#endif
		suggestedRootNote_=-1;
		return -1;
	}
	int size=source->GetSize(-1);
	int channels=source->GetChannelCount(-1);
	int sampleRate=source->GetSampleRate(-1);
	short *buffer=(short *)source->GetSampleBuffer(-1);
	if (!buffer || size<256 || channels<1 || sampleRate<=0) return -1;

	if (rangeStart<0) rangeStart=0;
	if (rangeStart>=size) rangeStart=0;
	if (rangeEnd<=rangeStart || rangeEnd>size) rangeEnd=size;
	int rangeSize=rangeEnd-rangeStart;
	if (rangeSize<256) {
		suggestedRootNote_=-1;
		return -1;
	}

	int scan=rangeSize<sampleRate?rangeSize:sampleRate;
	int peak=0;
	double rmsSum=0.0;
	for (int i=0;i<scan;i++) {
		int v=buffer[(rangeStart+i)*channels];
		if (v<0) v=-v;
		if (v>peak) peak=v;
		rmsSum+=(double)v*(double)v;
	}
	if (peak<512) {
		suggestedRootNote_=-1;
		return -1;
	}
	double rms=sqrt(rmsSum/(double)scan);
	if (rms<256.0) {
		suggestedRootNote_=-1;
		return -1;
	}

	int start=rangeStart;
	int threshold=peak/8;
	if (threshold<512) threshold=512;
	for (int i=0;i<scan;i++) {
		int v=buffer[(rangeStart+i)*channels];
		if (v<0) v=-v;
		if (v>=threshold) {
			start=rangeStart+i+(sampleRate/100);
			break;
		}
	}
	if (start<rangeStart || start>=rangeEnd) start=rangeStart;

	int window=sampleRate/3;
	if (window>8192) window=8192;
	if (window<1024) window=1024;
	if (start+window>=rangeEnd) {
		start=rangeStart;
		if (window>=rangeSize) window=rangeSize-1;
	}
	if (window<256) {
		suggestedRootNote_=-1;
		return -1;
	}

	int minLag=sampleRate/1200;
	int maxLag=sampleRate/45;
	if (minLag<8) minLag=8;
	if (maxLag>=window/2) maxLag=window/2;
	if (maxLag<=minLag) return -1;

	int bestLag=0;
	double bestCorr=0.0;
	for (int lag=minLag;lag<=maxLag;lag++) {
		double corr=sampleRootCorrelation(buffer,channels,start,window,lag);
		if (corr>bestCorr) {
			bestCorr=corr;
			bestLag=lag;
		}
	}
	if (bestLag<=0 || bestCorr<0.42) {
#ifdef PLATFORM_RGNANO_SIM
		Trace::Log("SAMPLE_AUTOROOT","low confidence name=%s corr=%.3f",name?name:"(null)",bestCorr);
#endif
		suggestedRootNote_=-1;
		return -1;
	}
	double strongCorr=bestCorr-0.08;
	if (strongCorr<0.42) strongCorr=0.42;
	double prevCorr=sampleRootCorrelation(buffer,channels,start,window,minLag);
	for (int lag=minLag+1;lag<bestLag;lag++) {
		double corr=sampleRootCorrelation(buffer,channels,start,window,lag);
		double nextCorr=sampleRootCorrelation(buffer,channels,start,window,lag+1);
		if (corr>=strongCorr && corr>=prevCorr && corr>=nextCorr) {
			bestCorr=corr;
			bestLag=lag;
			break;
		}
		prevCorr=corr;
	}
	float freq=(float)sampleRate/(float)bestLag;
	int note=frequencyToMidi(freq);
	suggestedRootNote_=note;
#ifdef PLATFORM_RGNANO_SIM
	Trace::Log("SAMPLE_AUTOROOT","suggest name=%s start=%d end=%d freq=%.2f midi=%d corr=%.3f",name?name:"(null)",rangeStart,rangeEnd,freq,note,bestCorr);
#endif
	return note;
}

int SampleInstrument::GetSuggestedRootNote() {
	return suggestedRootNote_;
}

void SampleInstrument::ClearRootNoteSuggestion() {
	suggestedRootNote_=-1;
}

bool SampleInstrument::AcceptSuggestedRootNote() {
	if (suggestedRootNote_<0) return false;
	Variable *root=FindVariable(SIP_ROOTNOTE);
	if (!root) return false;
	root->SetInt(suggestedRootNote_);
	suggestedRootNote_=-1;
	dirty_=true;
	return true;
}

int SampleInstrument::GetSampleIndex() {
	 Variable *v=FindVariable(SIP_SAMPLE) ;
	 return v->GetInt() ;
} ;

void SampleInstrument::SetVolume(int volume) {
	Variable *v=FindVariable(SIP_VOLUME) ;
	v->SetInt(volume) ;
} ;

int SampleInstrument::GetVolume() {
	Variable *v=FindVariable(SIP_VOLUME) ;
	return v->GetInt() ;
} ;

int SampleInstrument::GetSampleSize(int channel) {
	if (source_) {
		 renderParams *rp=renderParams_+channel ;
		return source_->GetSize(rp->midiNote_) ;
	} ;
	return 0 ;
} ;

int SampleInstrument::GetLoopEnd() { return loopEnd_->GetInt(); }

bool SampleInstrument::IsInitialized() {
    return (source_!=0) ;
} ;

void SampleInstrument::updateInstrumentData(bool search) {

	SamplePool *pool=SamplePool::GetInstance() ;

	// Get the source index

  Variable *vSample=FindVariable(SIP_SAMPLE) ;
	int index=vSample->GetInt() ;
	int instrSize=0 ;

	if (index!=NO_SAMPLE)
  {
		source_=pool->GetSource(index) ;
		if (source_&&(!source_->IsMulti()))
    {
    		instrSize=source_->GetSize(-1) ;
		}
	}

	Variable *v=FindVariable(SIP_END) ;
	v->SetInt(instrSize) ;
	v=FindVariable(SIP_LOOPSTART) ;
	v->SetInt(0) ;
	v=FindVariable(SIP_START) ;
	v->SetInt(0) ;
	dirty_=false ;
} ;

void SampleInstrument::Update(Observable &o,I_ObservableData *d)
{
	WatchedVariable &v=(WatchedVariable &)o ;
	FourCC id=v.GetID() ;

	switch(id) {
		case SIP_SAMPLE:	
		{
			if (running_) {
				dirty_=true ; // we'll update later, when instrument gets re-triggered
			} else {
				updateInstrumentData(true) ;
				SetChanged() ;
				NotifyObservers();
			} ;
		}
			break ;

		case SIP_START:
		case SIP_LOOPSTART:
		case SIP_END:
			suggestedRootNote_=-1;
			break;



		default:
   //         Trace::Dump("Got notification from %c%c%c%c",fourcc[0],fourcc[1],fourcc[2],fourcc[3]) ;
            break ;
	};
} ;

void SampleInstrument::ProcessCommand(int channel,FourCC cc,ushort value) {
	
 	renderParams *rp=renderParams_+channel ;
	if (!source_) return ;

	switch(cc) {
    case I_CMD_LPOF: {
        // LPOF (Loop Offset) shifts the loop window (rendLoopStart_,
        // rendLoopEnd_) within the sample. Two coexisting use cases:
        //
        // 1) Wavetable / single-cycle synthesis (SILM_OSC):
        //    The loop length defines the oscillator's pitch (freq * length /
        //    driverRate). LPOF is used to scan across stored waveforms or
        //    modulate timbre (e.g. PWM) WITHOUT changing pitch. We must NOT
        //    move the playhead here — doing so makes pitch glide as the bounds
        //    shift mid-cycle, producing unwanted microtonal artifacts.
        //
        // 2) Granular stretching (every other mode — loop, ping-pong, loopsync,
        //    oneshot, sliced or not):
        //    Advancing the playhead alongside the loop window drags playback
        //    through the source faster than the note's natural rate, decoupling
        //    stretch speed from pitch. With short loops + LPOF + HOP in a
        //    table, this produces timestretch / breakbeat-style scrubbing.
        //
        //    In oneshot or slice modes the playhead may cross into territory
        //    the user didn't anticipate (notes ending early, slices bleeding
        //    into neighbors). That's intentional — these are useful artifacts,
        //    not bugs.

        bool dragPlayhead = !IsOscMode(rp->loopMode_);
        // Reverse modes keep the window's ends the other way round
        int windowLow = rp->rendLoopStart_ < rp->rendLoopEnd_ ? rp->rendLoopStart_ : rp->rendLoopEnd_;
        int windowHigh = rp->rendLoopStart_ < rp->rendLoopEnd_ ? rp->rendLoopEnd_ : rp->rendLoopStart_;

        if (value > 0x8000) {
            // Backward shift (two's complement): 0xFFFF = -1, 0x8001 = -32767
            int shift = (int)(0x10000 - value);
            if (shift > windowLow) { // Don't push start below sample 0
                shift = windowLow;
            }
            rp->rendLoopEnd_ -= shift;
            rp->rendLoopStart_ -= shift;
            if (dragPlayhead) {
                rp->position_ -= shift;
            }
        } else if (value > 0) { // LPOF 0000 is a no-op
            int sampleSize = source_->GetSize(rp->midiNote_);
            int shift = (int)value;
            // Clamp so rendLoopEnd_ doesn't escape the sample. When the window
            // hits the end, further forward LPOFs become no-ops — the loop is
            // parked at the boundary until something resets it.
            if (windowHigh + shift >= sampleSize) {
                shift = sampleSize - windowHigh;
            }
            if (shift > 0) {
                rp->rendLoopEnd_ += shift;
                rp->rendLoopStart_ += shift;
                if (dragPlayhead) {
                    rp->position_ += shift;
                }
            }
        }
        break;
    }
    case I_CMD_PLAY: {
        // PLAY 00bb: this note's play mode (bb as in the instrument's play
        // list: 00 forward, 01 reverse, 02 loop ...). On the note's own
        // step the note starts over in the new mode (reverse from E); on a
        // sounding note it carries on from where it is, the new way round.
        if (rp->sampleSize_ <= 0)
            break; // this voice never played a note
        int mode = value & 0xFF;
        if (mode >= SILM_LAST)
            mode = SILM_LAST - 1;
        rp->loopMode_ = mode;
        setupVoicePlayback(rp, rp->fresh_, rp->fresh_);
#ifdef PLATFORM_RGNANO_SIM
        Trace::Log("SAMPLE_PLAY", "channel %d mode=%s %s first=%d end=%d at=%d",
                   channel, loopTypes[mode], rp->fresh_ ? "restart" : "carry on",
                   rp->rendFirst_, rp->rendLoopEnd_, int(rp->position_));
#endif
        break;
    }
    case I_CMD_PLOF: {
        if (!source_)
            return;
        int wavSize = source_->GetSize(rp->midiNote_);
        float chkSize = wavSize / 256.0f;
        int absShft = value >> 8;
        if (absShft != 0) {
            rp->position_ = chkSize * absShft;
        };
        int relSfht = value & 0xFF;
        if (relSfht > 127)
            relSfht = relSfht - 256;
        rp->position_ += relSfht * chkSize;
        while (rp->position_ < 0) {
            rp->position_ = wavSize + rp->position_;
        };
        while (rp->position_ >= wavSize) {
            rp->position_ -= wavSize;
        };
        rp->couldClick_ = SHOULD_KILL_CLICKS;
			}
			break ;

		case I_CMD_ARPG:
			{
				rp->arp_.SetData(value) ;
				if (!rp->arp_.Enabled()) {
					rp->arp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->arp_) ;
				}
			}
			break ;

		case I_CMD_VOLM:
			{
				float targetVolume=float(value&0xFF) ;
				float speed=float(value>>8) ;
                float startVolume=fp2fl(rp->volume_) ;
				float baseVolume=fp2fl(rp->baseVolume_) ;
                
				int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
				speed=(speed==0)?0:fabs(targetVolume-startVolume)*KRATE_SAMPLE_COUNT/float(speed)/sampleCount ;
				rp->volumeRamp_.SetData(targetVolume-baseVolume,speed,startVolume-baseVolume) ;
				if (!rp->volumeRamp_.Enabled()) {
					rp->volumeRamp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->volumeRamp_) ;
				}
			}
			break ;

		case I_CMD_PAN_:
			{
				float targetPan=float(value&0xFF) ;
				if (targetPan==0xFF) {
					targetPan=0xFE ;
				}
				float basePan=fp2fl(rp->basePan_) ;
				float speed=float(value>>8) ;
				float startPan=fp2fl(rp->pan_) ;
				int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
				speed=(speed==0)?0:fabs(targetPan-startPan)*KRATE_SAMPLE_COUNT/float(speed)/sampleCount ;
				rp->panner_.SetData(targetPan-basePan,speed,startPan-basePan) ;
				if (!rp->panner_.Enabled()) {
					rp->panner_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->panner_) ;
				}
			}
			break ;

		case I_CMD_FCUT:
			{
				float target=float(value&0xFF)/255.0f ;
				float speed=float(value>>8) ;
                float start=fp2fl(rp->cutoff_) ;
                float baseCut=fp2fl(rp->baseFCut_) ;
				int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
				speed=(speed==0)?0:fabs(target-start)*KRATE_SAMPLE_COUNT/float(speed)/sampleCount ;
 				rp->cutRamp_.SetData(target-baseCut,speed,start-baseCut) ;
				if (!rp->cutRamp_.Enabled()) {
					rp->cutRamp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->cutRamp_) ;
				}
			}
			break ;

		case I_CMD_FRES:
			{
				float target=float(value&0xFF)/255.0f ;
				float speed=float(value>>8) ;
                float start=fp2fl(rp->reso_) ;
				float baseRes=fp2fl(rp->baseFRes_) ;                
				int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
				speed=(speed==0)?0:fabs(target-start)*KRATE_SAMPLE_COUNT/float(speed)/sampleCount ;
 				rp->resRamp_.SetData(target-baseRes,speed,start-baseRes) ;
				if (!rp->resRamp_.Enabled()) {
					rp->resRamp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->resRamp_) ;
				}
			}
			break ;

			// Feedback mix

		case I_CMD_FBMX:
			{
				float target=float(value&0xFF)/255.0f ;
				float speed=float(value>>8) ;
                float start=fp2fl(rp->fbMix_) ;
				float baseMix=fp2fl(rp->baseFbMix_) ;                
				int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
				speed=(speed==0)?0:fabs(target-start)*KRATE_SAMPLE_COUNT/float(speed)/sampleCount ;
 				rp->fbMixRamp_.SetData(target-baseMix,speed,start-baseMix) ;
				if (!rp->fbMixRamp_.Enabled()) {
					rp->fbMixRamp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->fbMixRamp_) ;
				}
			}
			break ;

		// Feedback tune

		case I_CMD_FBTN:
			{
				float target=float(value&0xFF)/255.0f ;
				float speed=float(value>>8) ;
                float start=fp2fl(rp->fbTun_) ;
				float baseTune=fp2fl(rp->baseFbTun_) ;                
				int sampleCount=int(4*SyncMaster::GetInstance()->GetTickSampleCount()) ;
				speed=(speed==0)?0:fabs(target-start)*KRATE_SAMPLE_COUNT/float(speed)/sampleCount ;
 				rp->fbTunRamp_.SetData(target-baseTune,speed,start-baseTune) ;
				if (!rp->fbTunRamp_.Enabled()) {
					rp->fbTunRamp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->fbTunRamp_) ;
				}
			}
			break ;

		case I_CMD_PTCH:
			{
				int pitch=(char)(value&0xFF) ; // number of semi tones
				float speed=float(value>>8) ;   // get speed parameter
				if (pitch>127) pitch=pitch-256 ;

			 // Target speed for the ramp
			 
				float targetSpeed=float(pow(2.0,(pitch)/12.0)) ;
				float srcSpeed=fp2fl(rp->speed_)/fp2fl(rp->baseSpeed_) ;

			// speed of ramp
			
				speed=(speed==0)?0.0f:fp2fl(rp->speed_)*255.0f/speed/KRATE_SAMPLE_COUNT/32.0f ;

			 // Fill ramp data & enable
			 
        rp->speedRamp_.SetData(targetSpeed,speed,srcSpeed) ;
				if (!rp->speedRamp_.Enabled()) {
					rp->speedRamp_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->speedRamp_) ;
				}
			} ;
			break ;

		case I_CMD_LEGA:
			{
				int pitch=(char)(value&0xFF) ; // number of semi tones
				float speed=float(value>>8) ;   // get speed parameter

				if (pitch>127) pitch=pitch-256 ;
				
			 // Target speed for the ramp, taken from channel' last note
			 // if no pitch is given
			 
        float targetSpeed,initSpeed ;
        if (pitch==0) 
        {
          pitch=lastMidiNote_[channel]-rp->midiNote_ ;
          targetSpeed=1.0 ;
          initSpeed=float(pow(2.0f,pitch/12.0f)) ;
        } 
        else
        {
          initSpeed=fp2fl(rp->speed_)/fp2fl(rp->baseSpeed_) ;
          targetSpeed=float(pow(2.0f,pitch/12.0f)) ;                    
        }
                
			// speed of ramp
			
				speed=(speed==0)?0.0f:float(1+50.0/KRATE_SAMPLE_COUNT/speed) ;

			 // Fill ramp data & enable
			 
        rp->legato_.SetData(targetSpeed,speed,initSpeed) ;
				if (!rp->legato_.Enabled()) {
					rp->legato_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->legato_) ;
				}
			} ;
			break ;
			
		case I_CMD_PFIN:
			{

				float semi=(value&0xFF)/float(0x80) ; // number of semi tones
				if (semi>1) semi=semi-2 ;

				float speed=float(value>>8) ;   // get speed parameter
				
				float initSpeed=rp->pfin_.Enabled()?rp->pfin_.GetCurrent():1 ;
        float targetSpeed=float(pow(2.0f,semi/12.0f)) ;                    
                
			// speed of ramp
			
				speed=(speed==0)?0.0f:float(1+50.0/KRATE_SAMPLE_COUNT/speed) ;

			 // Fill ramp data & enable
			 
        rp->pfin_.SetData(targetSpeed,speed,initSpeed) ;

				if (!rp->pfin_.Enabled()) {
					rp->pfin_.Enable() ;
					rp->activeUpdaters_.push_back(&rp->pfin_) ;
				}
			} ;
			break ;
			
		case I_CMD_RTRG:
            {
				unsigned char loop=(value&0xFF) ; // number of ticks before repeat
				unsigned char offset=(value>>8) ; // number of ticks to offset at each repeat
                if (loop!=0) {
                    rp->retrig_=true ;
                    rp->retrigLoop_=loop ;
                    rp->retrigCount_=loop ;
                    rp->retrigOffset_=offset ;
					rp->couldClick_=SHOULD_KILL_CLICKS ;
                } else {
                    rp->retrig_=false ;
                }
            }
            break ;
		case I_CMD_FLTR:
    		{
    			float cut=(value>>8)/255.0f;	// cutoff frequency (FF=all pass, 0=none pass)
    			float res=(value&0xFF)/255.0f;	// resonance, aka Q (0=none) so default is FF00
				rp->cutoff_=rp->baseFCut_=fl2fp(cut) ;
				rp->reso_=rp->baseFRes_=fl2fp(res) ;
				if (rp->cutRamp_.Enabled()) {
					rp->cutRamp_.Disable() ;
					std::vector<I_SRPUpdater *>::iterator it=rp->activeUpdaters_.begin() ;
					while (it!=rp->activeUpdaters_.end()) {
						if (*it==&rp->cutRamp_) {
							it=rp->activeUpdaters_.erase(it) ;
							break ;
						}
						it++ ;
					}
				}
				if (rp->resRamp_.Enabled()) {
					rp->resRamp_.Disable() ;
					std::vector<I_SRPUpdater *>::iterator it=rp->activeUpdaters_.begin() ;
					while (it!=rp->activeUpdaters_.end()) {
						if (*it==&rp->resRamp_) {
							it=rp->activeUpdaters_.erase(it) ;
							break ;
						}
						it++ ;
					}
				}

    		}
			break ;
		case I_CMD_CRSH:
			{
    			unsigned char drive=(value>>8);
    			unsigned char crush=(value&0x0F);
				if (drive >0 ) rp->drive_=drive ;
				if (crush >0 ) rp->crush_=crush ;
			}
		default:
			break;
	} ;
} ;

/*
 * Return whole file name
 * Intended for file operations where a complete name is necessary
 */
const char *SampleInstrument::GetFileName() {
    Variable *v = FindVariable(SIP_SAMPLE);
    const char *src = v->GetString();
    return src;
}

/*
* Return cropped name
* Intended for printing where the whole name doesn't fit on screen
*/
const char *SampleInstrument::GetName() {
    if (IsEmpty()) {
        return "EMPTY SAMPLE";
    }
    const char *custom = customName_->GetString();
    if (custom && custom[0]) {
        return custom;
    }
    Variable *v = FindVariable(SIP_SAMPLE);
    const char *src = v->GetString();

    // Shorten names if they're too long to display
    if (strlen(src) > Variable::MAX_NAME_LENGTH) {
	    static char shortName[Variable::MAX_NAME_LENGTH + 1];
        strncpy(shortName, src, Variable::MAX_NAME_LENGTH);
        shortName[Variable::MAX_NAME_LENGTH] = '\0';
        return shortName;
    }

    return src;
}

void SampleInstrument::Purge() {
	IteratorPtr<Variable> it(GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		Variable &v=it->CurrentItem() ;
		v.Reset() ;
	}
  source_ = NULL;
/*    Variable *v=FindVariable(SIP_SAMPLE) ;
	if (v->GetInt()!=-1) {
	    v->SetInt(-1) ;
	}*/
} ;

bool SampleInstrument::IsEmpty() {
    Variable *v=FindVariable(SIP_SAMPLE) ;
	return (v->GetInt()==-1) ;
} ;

int SampleInstrument::GetTable() {
	int result=table_->GetInt() ;
	if (result>TABLE_COUNT) {
		return VAR_OFF ;
	}
	return result ;
} ;

bool SampleInstrument::GetTableAutomation() {
	return tableAuto_->GetBool() ;
} ;

void SampleInstrument::GetTableState(TableSaveState &state) {
	memcpy(state.hopCount_,tableState_.hopCount_,sizeof(uchar)*TABLE_STEPS*3) ;
	memcpy(state.position_,tableState_.position_,sizeof(int)*3) ;
} ;

void SampleInstrument::SetTableState(TableSaveState &state) {
	memcpy(tableState_.hopCount_,state.hopCount_,sizeof(uchar)*TABLE_STEPS*3) ;
	memcpy(tableState_.position_,state.position_,sizeof(int)*3) ;
} ;

bool SampleInstrument::IsMulti() {
	return source_->IsMulti() ;
}

void SampleInstrument::EnableDownsamplingLegacy()
{
  useDirtyDownsampling_ = true;
  Trace::Log("CONFIG","Enabling downsampling legacy");
}
