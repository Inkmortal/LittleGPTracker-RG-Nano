#include "SendFX.h"
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
	}
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

void SendFX::AddSend(const float *stereo,int frames,float reverb,float delay) {
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
}

bool SendFX::IsActive() {
	return hasInput_ || tail_>0 ;
}

void SendFX::updateSettings() {
	int size=0x90 ;
	int damp=0x60 ;
	int delaySteps=3 ;
	int delayFeedback=0x70 ;
	if (project_) {
		Variable *v=project_->FindVariable(VAR_REVERB_SIZE) ;
		if (v) size=v->GetInt() ;
		v=project_->FindVariable(VAR_REVERB_DAMP) ;
		if (v) damp=v->GetInt() ;
		v=project_->FindVariable(VAR_DELAY_STEPS) ;
		if (v) delaySteps=v->GetInt() ;
		v=project_->FindVariable(VAR_DELAY_FEEDBACK) ;
		if (v) delayFeedback=v->GetInt() ;
	}
	feedback_=0.7f+ReverbSizeFromParam(size)*0.28f ;
	damp_=(damp/255.0f)*0.4f ;
	delayFeedback_=(delayFeedback/255.0f)*0.9f ;
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
	fixed *out=buffer ;
	float oneMinusDamp=1.0f-damp_ ;
	// The return sits on the master like a channel bus, so it follows the
	// Project pregain ("Drive") exactly as the dry buses do.
	float outputGain=1.0f ;
	if (project_) {
		outputGain=project_->GetPregain()/100.0f ;
	}
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
		wet[0]=(wet[0]+dl)*gain ;
		wet[1]=(wet[1]+dr)*gain ;
		if (wet[0]>2.0f) wet[0]=2.0f ;
		if (wet[0]<-2.0f) wet[0]=-2.0f ;
		if (wet[1]>2.0f) wet[1]=2.0f ;
		if (wet[1]<-2.0f) wet[1]=-2.0f ;
		*out++=fl2fp(wet[0]*32767.0f) ;
		*out++=fl2fp(wet[1]*32767.0f) ;

		rin[0]=rin[1]=0.0f ;
		din[0]=din[1]=0.0f ;
		rin+=2 ;
		din+=2 ;
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
