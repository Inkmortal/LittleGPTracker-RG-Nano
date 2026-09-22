
#include "PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Services/Audio/AudioOut.h"

static fixed tailBuffer_[MIX_BUFFER_SIZE*2] ;

PlayerChannel::PlayerChannel(int index) {             
    index_=index ;
    instr_=0 ;
    tail_=0 ;
    muted_=false ;
	mixBus_=0 ;
	busIndex_=-1 ;
}

PlayerChannel::~PlayerChannel() {
}

void PlayerChannel::StartInstrument(I_Instrument *instr,unsigned char note,bool trigger) {
   if (instr_) {
      StopInstrument() ;
   }
   // The instrument handles retriggering its own voice (with a short fade)
   if (tail_==instr) {
      tail_=0 ;
   }
   if (instr->Start(index_,note,trigger)) { // note could be refused coz it's out of the keymap
	   instr_=instr ;
   } else {
	   instr_=0 ;
   };
} ;

void PlayerChannel::StopInstrument() {
     if (instr_) {
       instr_->Stop(index_) ;
       if (instr_->IsReleasing(index_)) {
         tail_=instr_ ;
       }
     }
     instr_=0 ;
} ;

void PlayerChannel::StopQuickly() {
     if (instr_) {
       instr_->StopQuickly(index_) ;
       if (instr_->IsReleasing(index_)) {
         tail_=instr_ ;
       }
     } else if (tail_) {
       tail_->StopQuickly(index_) ;
     }
     instr_=0 ;
} ;

void PlayerChannel::ForgetInstrument(I_Instrument *instr) {
     if (instr_==instr) {
       instr_=0 ;
     }
     if (tail_==instr) {
       tail_=0 ;
     }
} ;

bool PlayerChannel::Render(fixed *buffer,int samplecount) {
   bool status=false ;
   if (instr_) {
     bool tableSlice=SyncMaster::GetInstance()->TableSlice() ;
     status=instr_->Render(index_,buffer,samplecount,tableSlice) ;
   }
   if (tail_) {
     if (samplecount>MIX_BUFFER_SIZE) samplecount=MIX_BUFFER_SIZE ;
     fixed *dst=status?tailBuffer_:buffer ;
     bool tailStatus=tail_->Render(index_,dst,samplecount,false) ;
     if (tailStatus && status) {
       fixed *src=tailBuffer_ ;
       fixed *out=buffer ;
       int count=samplecount*2 ;
       while (count--) {
         *out++ += *src++ ;
       }
     }
     status=status||tailStatus ;
     if (!tail_->IsReleasing(index_)) {
       tail_=0 ;
     }
   }
   return ((status)&&(!muted_)) ;
} ;

I_Instrument *PlayerChannel::GetInstrument() {
   return instr_ ;
} ;

void PlayerChannel::SetMute(bool muted) {
     muted_=muted ;
}

bool PlayerChannel::IsMuted() {
     return muted_ ;
}

void PlayerChannel::SetMixBus(int i) {

	if (i==busIndex_) return ;

	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	mixBus_=MixerService::GetInstance()->GetMixBus(i) ;
	if (mixBus_) {
		mixBus_->Insert(*this) ;
	}
} ;

void PlayerChannel::Reset() {
	tail_=0 ;
	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	muted_=false ;
	busIndex_=-1 ;
} ;