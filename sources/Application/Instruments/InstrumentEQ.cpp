#include "InstrumentEQ.h"
#include "Services/Audio/Audio.h"
#include <string.h>

const FourCC InstrumentEQ::Ids[6]={IEQ_LOW_GAIN,IEQ_LOW_FREQ,IEQ_MID_GAIN,
                                   IEQ_MID_FREQ,IEQ_HIGH_GAIN,IEQ_HIGH_FREQ} ;

static const char *eqVarNames[6]={"eq low gain","eq low freq","eq mid gain",
                                  "eq mid freq","eq high gain","eq high freq"} ;

InstrumentEQ::InstrumentEQ() {
	for (int i=0;i<6;i++) {
		vars_[i]=0 ;
		params_[i]=-1 ;
	}
	rate_=0.0f ;
	activeCount_=0 ;
	memset(coeffs_,0,sizeof(coeffs_)) ;
	memset(state_,0,sizeof(state_)) ;
}

void InstrumentEQ::Create(VariableContainer &owner) {
	for (int i=0;i<6;i++) {
		vars_[i]=new Variable(eqVarNames[i],Ids[i],ThreeBandEQ::DefaultParams[i],0xFF) ;
		owner.Insert(vars_[i]) ;
	}
}

void InstrumentEQ::Reset() {
	for (int i=0;i<6;i++) {
		if (vars_[i]) vars_[i]->SetInt(ThreeBandEQ::DefaultParams[i]) ;
	}
}

bool InstrumentEQ::Prepare() {
	if (!vars_[0]) return false ;
	int now[6] ;
	bool changed=false ;
	for (int i=0;i<6;i++) {
		now[i]=vars_[i]->GetInt() ;
		if (now[i]!=params_[i]) changed=true ;
	}
	float rate=(float)Audio::GetInstance()->GetSampleRate() ;
	if (rate<8000.0f) rate=44100.0f ;
	if (rate!=rate_) changed=true ;
	if (changed) {
		bool wasOn[3]={false,false,false} ;
		for (int k=0;k<activeCount_;k++) wasOn[active_[k]]=true ;
		memcpy(params_,now,sizeof(params_)) ;
		rate_=rate ;
		ThreeBandEQ::Design(params_,rate_,coeffs_) ;
		activeCount_=0 ;
		for (int b=0;b<3;b++) {
			if (ThreeBandEQ::BandFlat(params_,b)) continue ;
			active_[activeCount_++]=b ;
			if (!wasOn[b]) {
				// A band switching on starts without leftovers of an
				// older setting
				for (int c=0;c<SONG_CHANNEL_COUNT;c++) {
					memset(state_[c][b],0,sizeof(state_[c][b])) ;
				}
			}
		}
	}
	return activeCount_>0 ;
}

void InstrumentEQ::ResetVoice(int channel) {
	if (channel<0 || channel>=SONG_CHANNEL_COUNT) return ;
	memset(state_[channel],0,sizeof(state_[channel])) ;
}

void InstrumentEQ::ProcessStereo(int channel,fixed *buffer,int frames) {
	double (*z)[2][2]=state_[channel] ;
	for (int i=0;i<frames*2;i+=2) {
		double l=fp2fl(buffer[i]) ;
		double r=fp2fl(buffer[i+1]) ;
		for (int k=0;k<activeCount_;k++) {
			int b=active_[k] ;
			l=ThreeBandEQ::Step(coeffs_[b],z[b][0],l) ;
			r=ThreeBandEQ::Step(coeffs_[b],z[b][1],r) ;
		}
		// A boost must not wrap the fixed-point sample
		if (l>65000.0) l=65000.0 ;
		if (l<-65000.0) l=-65000.0 ;
		if (r>65000.0) r=65000.0 ;
		if (r<-65000.0) r=-65000.0 ;
		buffer[i]=fl2fp((float)l) ;
		buffer[i+1]=fl2fp((float)r) ;
	}
}
