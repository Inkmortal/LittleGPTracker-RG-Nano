#include "SendFX.h"
#include "Application/Instruments/SynthEngines.h"
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
	SynthEnginesInit() ;   // the sine table the chorus LFOs read
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
	// locals: stores into the delay lines could alias the members, which
	// would make the compiler reload them for every sample
	const float damp=damp_ ;
	const float feedback=feedback_ ;
	// The return sits on the master like a channel bus, so it follows the
	// Project pregain ("Drive") exactly as the dry buses do.
	float outputGain=1.0f ;
	if (project_) {
		outputGain=project_->GetPregain()/100.0f ;
	}
	// Return levels from the Mixer screen
	float reverbReturn=Mixer::GetInstance()->GetGain(MIXER_LEVEL_REVERB) ;
	float delayReturn=Mixer::GetInstance()->GetGain(MIXER_LEVEL_DELAY) ;
	// Worked through in chunks, one stage at a time over the whole chunk
	// (each comb and allpass then runs as a tight loop with no per-sample
	// wrap test); every sample goes through the same arithmetic in the
	// same order as a sample-by-sample loop would.
	const int CHUNK=64 ;
	float revIn[CHUNK] ;
	float dly[2][CHUNK] ;
	float cho[2][CHUNK] ;
	float wet[2][CHUNK] ;
	for (int start=0;start<frames;start+=CHUNK) {
		int n=frames-start ;
		if (n>CHUNK) n=CHUNK ;

		{
			// the state in locals for the loop (see above), written back after
			float *const dbuf=delayBuffer_ ;
			float *const cbuf=chorusBuffer_ ;
			const int dsize=delaySize_ ;
			const int dframes=delayFrames_ ;
			const float dfb=delayFeedback_ ;
			const int csize=chorusSize_ ;
			const float cbase=chorusBase_ ;
			const float cdepth=chorusDepth_ ;
			const float cinc=chorusInc_ ;
			int dwrite=delayWrite_ ;
			int cwrite=chorusWrite_ ;
			float cphase=chorusPhase_ ;
			float lp0=delayLp_[0] ;
			float lp1=delayLp_[1] ;
			for (int j=0;j<n;j++) {
				// Delay: stereo with cross feedback, darkening repeats
				int readPos=dwrite-dframes ;
				if (readPos<0) readPos+=dsize ;
				float dl=dbuf[readPos*2] ;
				float dr=dbuf[readPos*2+1] ;
				lp0+=0.45f*(dr-lp0) ;
				lp1+=0.45f*(dl-lp1) ;
				dbuf[dwrite*2]=din[0]+lp0*dfb ;
				dbuf[dwrite*2+1]=din[1]+lp1*dfb ;
				if (++dwrite>=dsize) dwrite=0 ;
				dly[0][j]=dl ;
				dly[1][j]=dr ;

				// Reverb input (the delay also feeds it a little for a softer
				// tail)
				revIn[j]=(rin[0]+rin[1]+(dl+dr)*0.15f)*reverbInputGain ;

				// Chorus: each side reads the recent input through its own
				// swept delay, the two LFOs a quarter cycle apart for width
				cbuf[cwrite*2]=cin[0] ;
				cbuf[cwrite*2+1]=cin[1] ;
				// The LFOs from the shared sine table: a libm sin() per side
				// per sample cost more than the whole reverb, and the device's
				// sin() returns 0 past 2 pi, which froze the right side's LFO
				// for a quarter of every cycle (phase + 0.25 went over 1)
				unsigned int lfoPhase=(unsigned int)(cphase*4294967296.0f) ;
				for (int c=0;c<2;c++) {
					float lfo=synthSinU32(lfoPhase+(c?0x40000000u:0u)) ;
					float lag=cbase+cdepth*(0.5f+0.5f*lfo) ;
					float pos=cwrite-lag ;
					while (pos<0) pos+=csize ;
					int i0=(int)pos ;
					float frac=pos-i0 ;
					int i1=i0+1 ;   // pos < csize: at most one wrap
					if (i1>=csize) i1=0 ;
					cho[c][j]=cbuf[i0*2+c]*(1.0f-frac)+cbuf[i1*2+c]*frac ;
				}
				if (++cwrite>=csize) cwrite=0 ;
				cphase+=cinc ;
				if (cphase>=1.0f) cphase-=1.0f ;

				rin[0]=rin[1]=0.0f ;
				din[0]=din[1]=0.0f ;
				cin[0]=cin[1]=0.0f ;
				rin+=2 ;
				din+=2 ;
				cin+=2 ;
			}
			delayWrite_=dwrite ;
			chorusWrite_=cwrite ;
			chorusPhase_=cphase ;
			delayLp_[0]=lp0 ;
			delayLp_[1]=lp1 ;
		}

		// Reverb: eight parallel combs, then four allpasses in series
		for (int c=0;c<2;c++) {
			float *acc=wet[c] ;
			for (int k=0;k<SENDFX_COMBS;k++) {
				SendFXComb &comb=combs_[c][k] ;
				float *buf=comb.buffer_ ;
				const int size=comb.size_ ;
				int index=comb.index_ ;
				float store=comb.store_ ;
				int j=0 ;
				while (j<n) {
					int run=size-index ;
					if (run>n-j) run=n-j ;
					float *p=buf+index ;
					float *a=acc+j ;
					const float *in=revIn+j ;
					if (k==0) {
						// the first comb starts the sum (0 + y is y)
						for (int r=0;r<run;r++) {
							float y=p[r] ;
							store=y*oneMinusDamp+store*damp ;
							p[r]=in[r]+store*feedback ;
							a[r]=y ;
						}
					} else {
						for (int r=0;r<run;r++) {
							float y=p[r] ;
							store=y*oneMinusDamp+store*damp ;
							p[r]=in[r]+store*feedback ;
							a[r]+=y ;
						}
					}
					j+=run ;
					index+=run ;
					if (index>=size) index=0 ;
				}
				comb.index_=index ;
				comb.store_=store ;
			}
			for (int k=0;k<SENDFX_ALLPASSES;k++) {
				SendFXAllpass &ap=allpasses_[c][k] ;
				float *buf=ap.buffer_ ;
				int index=ap.index_ ;
				int j=0 ;
				while (j<n) {
					int run=ap.size_-index ;
					if (run>n-j) run=n-j ;
					float *p=buf+index ;
					for (int r=0;r<run;r++) {
						float b=p[r] ;
						float a=acc[j+r] ;
						p[r]=a+b*allpassFeedback ;
						acc[j+r]=b-a ;
					}
					j+=run ;
					index+=run ;
					if (index>=ap.size_) index=0 ;
				}
				ap.index_=index ;
			}
			// (Freeverb wet 1/3 of its 3x scale: unity-ish return)
		}

		for (int j=0;j<n;j++) {
			float gain=outputGain ;
			if (fade_>0) {
				gain*=(float)fade_/(float)fadeLength_ ;
				fade_-- ;
			}
			float l=(wet[0][j]*reverbReturn+dly[0][j]*delayReturn+cho[0][j]*chorusReturn)*gain ;
			float r=(wet[1][j]*reverbReturn+dly[1][j]*delayReturn+cho[1][j]*chorusReturn)*gain ;
			if (l>2.0f) l=2.0f ;
			if (l<-2.0f) l=-2.0f ;
			if (r>2.0f) r=2.0f ;
			if (r<-2.0f) r=-2.0f ;
			*out++=fl2fp(l*32767.0f) ;
			*out++=fl2fp(r*32767.0f) ;
		}
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
