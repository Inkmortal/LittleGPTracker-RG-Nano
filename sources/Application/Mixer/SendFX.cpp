#include "SendFX.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include "System/Console/Trace.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

// Freeverb tunings at 44.1kHz (Jezar's public domain design)
static const int combTuning[SENDFX_COMBS]={1116,1188,1277,1356,1422,1491,1557,1617} ;
static const int allpassTuning[SENDFX_ALLPASSES]={556,441,341,225} ;
static const int stereoSpread=23 ;
static const float reverbInputGain=0.015f ;
static const float allpassFeedback=0.5f ;

// Frames of silence to keep rendering tails for after the last send
#define SENDFX_TAIL_SECONDS 6

SendFX::SendFX() {
	project_=0 ;
	sampleRate_=0 ;
	allocated_=false ;
	hasInput_=false ;
	tail_=0 ;
	fade_=0 ;
	fadeLength_=1 ;
	feedback_=0.84f ;
	damp_=0.2f ;
	reverbLevel_=1.0f ;
	delayBuffer_=0 ;
	delaySize_=0 ;
	delayWrite_=0 ;
	delayFrames_=1 ;
	delayFeedback_=0.4f ;
	delayLevel_=1.0f ;
	delayLp_[0]=delayLp_[1]=0.0f ;
	chorusBuffer_=0 ;
	chorusSize_=0 ;
	chorusWrite_=0 ;
	chorusPhase_=0.0f ;
	chorusInc_=0.0f ;
	chorusDepth_=0.0f ;
	chorusBase_=0.0f ;
	memset(chorusIn_,0,sizeof(chorusIn_)) ;
	memset(muted_,0,sizeof(muted_)) ;
	memset(reverbIn_,0,sizeof(reverbIn_)) ;
	memset(delayIn_,0,sizeof(delayIn_)) ;
	for (int c=0;c<2;c++) {
		for (int i=0;i<SENDFX_COMBS;i++) {
			combs_[c][i].buffer_=0 ;
			combs_[c][i].size_=0 ;
			combs_[c][i].index_=0 ;
			combs_[c][i].store_=0.0f ;
		}
		for (int i=0;i<SENDFX_ALLPASSES;i++) {
			allpasses_[c][i].buffer_=0 ;
			allpasses_[c][i].size_=0 ;
			allpasses_[c][i].index_=0 ;
		}
	}
}

SendFX::~SendFX() {
	for (int c=0;c<2;c++) {
		for (int i=0;i<SENDFX_COMBS;i++) free(combs_[c][i].buffer_) ;
		for (int i=0;i<SENDFX_ALLPASSES;i++) free(allpasses_[c][i].buffer_) ;
	}
	free(delayBuffer_) ;
	free(chorusBuffer_) ;
}

void SendFX::SetProject(Project *project) {
	project_=project ;
	Clear() ;
}

float SendFX::ReverbSizeFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return value/255.0f ;
}

float SendFX::ChorusRateFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.1f*(float)pow(50.0,value/255.0) ;
}

float SendFX::ChorusDepthMsFromParam(int value) {
	if (value<0) value=0 ;
	if (value>255) value=255 ;
	return 0.5f+7.0f*value/255.0f ;
}

int SendFX::DelayStepsFromParam(int value) {
	if (value<1) value=1 ;
	if (value>16) value=16 ;
	return value ;
}

void SendFX::allocate(int sampleRate) {
	float scale=sampleRate/44100.0f ;
	for (int c=0;c<2;c++) {
		for (int i=0;i<SENDFX_COMBS;i++) {
			SendFXComb &comb=combs_[c][i] ;
			free(comb.buffer_) ;
			comb.size_=(int)((combTuning[i]+c*stereoSpread)*scale) ;
			comb.buffer_=(float *)calloc(comb.size_,sizeof(float)) ;
			comb.index_=0 ;
			comb.store_=0.0f ;
		}
		for (int i=0;i<SENDFX_ALLPASSES;i++) {
			SendFXAllpass &ap=allpasses_[c][i] ;
			free(ap.buffer_) ;
			ap.size_=(int)((allpassTuning[i]+c*stereoSpread)*scale) ;
			ap.buffer_=(float *)calloc(ap.size_,sizeof(float)) ;
			ap.index_=0 ;
		}
	}
	free(delayBuffer_) ;
	delaySize_=sampleRate*SENDFX_DELAY_SECONDS ;
	delayBuffer_=(float *)calloc(delaySize_*2,sizeof(float)) ;
	delayWrite_=0 ;
	free(chorusBuffer_) ;
	chorusSize_=sampleRate*SENDFX_CHORUS_MS/1000 ;
	chorusBuffer_=(float *)calloc(chorusSize_*2,sizeof(float)) ;
	chorusWrite_=0 ;
	sampleRate_=sampleRate ;
	allocated_=true ;
	Trace::Log("SENDFX","allocated at %d Hz",sampleRate) ;
}

void SendFX::Clear() {
	if (allocated_) {
		for (int c=0;c<2;c++) {
			for (int i=0;i<SENDFX_COMBS;i++) {
				memset(combs_[c][i].buffer_,0,combs_[c][i].size_*sizeof(float)) ;
				combs_[c][i].store_=0.0f ;
			}
			for (int i=0;i<SENDFX_ALLPASSES;i++) {
				memset(allpasses_[c][i].buffer_,0,allpasses_[c][i].size_*sizeof(float)) ;
			}
		}
		memset(delayBuffer_,0,delaySize_*2*sizeof(float)) ;
		memset(chorusBuffer_,0,chorusSize_*2*sizeof(float)) ;
	}
	memset(chorusIn_,0,sizeof(chorusIn_)) ;
	memset(reverbIn_,0,sizeof(reverbIn_)) ;
	memset(delayIn_,0,sizeof(delayIn_)) ;
	delayLp_[0]=delayLp_[1]=0.0f ;
	hasInput_=false ;
	tail_=0 ;
	fade_=0 ;
}

void SendFX::CancelFade() {
	if (fade_>0) {
		Clear() ;
	}
	fadeLength_=1 ;
}

void SendFX::FadeOut() {
	if (!allocated_ || (!hasInput_ && tail_<=0)) return ;
	fadeLength_=sampleRate_/4 ;
	if (fadeLength_<1) fadeLength_=1 ;
	fade_=fadeLength_ ;
}

void SendFX::SetChannelMuted(int channel,bool muted) {
	if (channel>=0 && channel<8) muted_[channel]=muted ;
}

void SendFX::AddSend(int channel,const float *stereo,int frames,float reverb,float delay,
                     float chorus) {
	if (channel>=0 && channel<8 && muted_[channel]) return ;
	if (frames>SENDFX_MAX_FRAMES) frames=SENDFX_MAX_FRAMES ;
	if (reverb>0.0f) {
		float *dst=reverbIn_ ;
		const float *src=stereo ;
		for (int i=0;i<frames*2;i++) {
			*dst++ += (*src++)*reverb ;
		}
		hasInput_=true ;
	}
	if (delay>0.0f) {
		float *dst=delayIn_ ;
		const float *src=stereo ;
		for (int i=0;i<frames*2;i++) {
			*dst++ += (*src++)*delay ;
		}
		hasInput_=true ;
	}
	if (chorus>0.0f) {
		float *dst=chorusIn_ ;
		const float *src=stereo ;
		for (int i=0;i<frames*2;i++) {
			*dst++ += (*src++)*chorus ;
		}
		hasInput_=true ;
	}
}

void SendFX::GetDebugState(bool &input,int &tail,int &fade) {
	input=hasInput_ ;
	tail=tail_ ;
	fade=fade_ ;
}

bool SendFX::IsActive() {
	return hasInput_ || tail_>0 ;
}

void SendFX::updateSettings() {
	int size=0x90 ;
	int damp=0x60 ;
	int delaySteps=3 ;
	int delayFeedback=0x70 ;
	int chorusRate=0x40 ;
	int chorusDepth=0x70 ;
	if (project_) {
		Variable *v=project_->FindVariable(VAR_REVERB_SIZE) ;
		if (v) size=v->GetInt() ;
		v=project_->FindVariable(VAR_REVERB_DAMP) ;
		if (v) damp=v->GetInt() ;
		v=project_->FindVariable(VAR_DELAY_STEPS) ;
		if (v) delaySteps=v->GetInt() ;
		v=project_->FindVariable(VAR_DELAY_FEEDBACK) ;
		if (v) delayFeedback=v->GetInt() ;
		v=project_->FindVariable(VAR_CHORUS_RATE) ;
		if (v) chorusRate=v->GetInt() ;
		v=project_->FindVariable(VAR_CHORUS_DEPTH) ;
		if (v) chorusDepth=v->GetInt() ;
	}
	feedback_=0.7f+ReverbSizeFromParam(size)*0.28f ;
	damp_=(damp/255.0f)*0.4f ;
	delayFeedback_=(delayFeedback/255.0f)*0.9f ;
	chorusInc_=ChorusRateFromParam(chorusRate)/sampleRate_ ;
	chorusDepth_=ChorusDepthMsFromParam(chorusDepth)*sampleRate_/1000.0f ;
	chorusBase_=12.0f*sampleRate_/1000.0f ;
	int tempo=SyncMaster::GetInstance()->GetTempo() ;
	if (tempo<20) tempo=120 ;
	// one step is a 16th note
	delayFrames_=(int)(DelayStepsFromParam(delaySteps)*15.0f*sampleRate_/tempo) ;
	if (delayFrames_<1) delayFrames_=1 ;
	if (delayFrames_>=delaySize_) delayFrames_=delaySize_-1 ;
}

bool SendFX::Render(fixed *buffer,int samplecount) {
	int sampleRate=Audio::GetInstance()->GetSampleRate() ;
	if (!allocated_ || sampleRate!=sampleRate_) {
		allocate(sampleRate) ;
	}
	if (!hasInput_ && tail_<=0) {
		return false ;
	}
	if (hasInput_ && fade_==0) {
		tail_=sampleRate_*SENDFX_TAIL_SECONDS ;
	}
	updateSettings() ;

	int frames=samplecount ;
	if (frames>SENDFX_MAX_FRAMES) frames=SENDFX_MAX_FRAMES ;

	float *rin=reverbIn_ ;
	float *din=delayIn_ ;
	float *cin=chorusIn_ ;
	float chorusReturn=Mixer::GetInstance()->GetGain(MIXER_LEVEL_CHORUS) ;
	fixed *out=buffer ;
	float oneMinusDamp=1.0f-damp_ ;
	// The return sits on the master like a channel bus, so it follows the
	// Project pregain ("Drive") exactly as the dry buses do.
	float outputGain=1.0f ;
	if (project_) {
		outputGain=project_->GetPregain()/100.0f ;
	}
	// Return levels from the Mixer screen
	float reverbReturn=Mixer::GetInstance()->GetGain(MIXER_LEVEL_REVERB) ;
	float delayReturn=Mixer::GetInstance()->GetGain(MIXER_LEVEL_DELAY) ;
	for (int i=0;i<frames;i++) {
		float wet[2] ;

		// Delay: stereo with cross feedback, darkening repeats
		int readPos=delayWrite_-delayFrames_ ;
		if (readPos<0) readPos+=delaySize_ ;
		float dl=delayBuffer_[readPos*2] ;
		float dr=delayBuffer_[readPos*2+1] ;
		delayLp_[0]+=0.45f*(dr-delayLp_[0]) ;
		delayLp_[1]+=0.45f*(dl-delayLp_[1]) ;
		delayBuffer_[delayWrite_*2]=din[0]+delayLp_[0]*delayFeedback_ ;
		delayBuffer_[delayWrite_*2+1]=din[1]+delayLp_[1]*delayFeedback_ ;
		if (++delayWrite_>=delaySize_) delayWrite_=0 ;

		// Reverb (the delay also feeds it a little for a softer tail)
		float input=(rin[0]+rin[1]+(dl+dr)*0.15f)*reverbInputGain ;
		for (int c=0;c<2;c++) {
			float acc=0.0f ;
			for (int k=0;k<SENDFX_COMBS;k++) {
				SendFXComb &comb=combs_[c][k] ;
				float y=comb.buffer_[comb.index_] ;
				comb.store_=y*oneMinusDamp+comb.store_*damp_ ;
				comb.buffer_[comb.index_]=input+comb.store_*feedback_ ;
				if (++comb.index_>=comb.size_) comb.index_=0 ;
				acc+=y ;
			}
			for (int k=0;k<SENDFX_ALLPASSES;k++) {
				SendFXAllpass &ap=allpasses_[c][k] ;
				float b=ap.buffer_[ap.index_] ;
				ap.buffer_[ap.index_]=acc+b*allpassFeedback ;
				if (++ap.index_>=ap.size_) ap.index_=0 ;
				acc=b-acc ;
			}
			wet[c]=acc ;  // Freeverb wet 1/3 of its 3x scale: unity-ish return
		}
		float gain=outputGain ;
		if (fade_>0) {
			gain*=(float)fade_/(float)fadeLength_ ;
			fade_-- ;
		}
		// Chorus: each side reads the recent input through its own swept
		// delay, the two LFOs a quarter cycle apart for width
		chorusBuffer_[chorusWrite_*2]=cin[0] ;
		chorusBuffer_[chorusWrite_*2+1]=cin[1] ;
		float ch[2] ;
		for (int c=0;c<2;c++) {
			float lfo=(float)sin(6.28318530718*(chorusPhase_+c*0.25f)) ;
			float lag=chorusBase_+chorusDepth_*(0.5f+0.5f*lfo) ;
			float pos=chorusWrite_-lag ;
			while (pos<0) pos+=chorusSize_ ;
			int i0=(int)pos ;
			float frac=pos-i0 ;
			int i1=(i0+1)%chorusSize_ ;
			ch[c]=chorusBuffer_[i0*2+c]*(1.0f-frac)+chorusBuffer_[i1*2+c]*frac ;
		}
		if (++chorusWrite_>=chorusSize_) chorusWrite_=0 ;
		chorusPhase_+=chorusInc_ ;
		if (chorusPhase_>=1.0f) chorusPhase_-=1.0f ;

		wet[0]=(wet[0]*reverbReturn+dl*delayReturn+ch[0]*chorusReturn)*gain ;
		wet[1]=(wet[1]*reverbReturn+dr*delayReturn+ch[1]*chorusReturn)*gain ;
		if (wet[0]>2.0f) wet[0]=2.0f ;
		if (wet[0]<-2.0f) wet[0]=-2.0f ;
		if (wet[1]>2.0f) wet[1]=2.0f ;
		if (wet[1]<-2.0f) wet[1]=-2.0f ;
		*out++=fl2fp(wet[0]*32767.0f) ;
		*out++=fl2fp(wet[1]*32767.0f) ;

		rin[0]=rin[1]=0.0f ;
		din[0]=din[1]=0.0f ;
		cin[0]=cin[1]=0.0f ;
		rin+=2 ;
		din+=2 ;
		cin+=2 ;
	}
	for (int i=frames;i<samplecount;i++) {
		*out++=0 ;
		*out++=0 ;
	}
	hasInput_=false ;
	tail_-=samplecount ;
	if (fadeLength_>0 && fade_==0 && tail_>0 && fadeLength_!=1) {
		// Fade finished: drop what is left in the rooms and echoes
		fadeLength_=1 ;
		Clear() ;
	}
	return true ;
}
