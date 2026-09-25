#include "System/Console/CrashLog.h"
#include "Application/Model/Scale.h"

// RAND / CHNC randomness (defined with playCursorPosition)
static int randomUpTo(int range) ;
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Mixer/SendFX.h"
#include "Player.h"
#include "Application/Views/BaseClasses/ViewEvent.h"
#include "System/io/Status.h"
#include "System/System/System.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/I_Instrument.h"
#include "Application/Utils/char.h"
#include "System/Console/Trace.h"
#include "System/Console/n_assert.h"
#include "Application/Player/TablePlayback.h"
#include "Application/Model/Groove.h"
#include <math.h>
#include <string.h>
#include "Services/Midi/MidiService.h"
#include <sstream>

// Private constructor - Singleton

Player::Player() {

	isRunning_=false ;
	viewData_=0 ;
	mixer_=new PlayerMixer() ;

	lastSongPos_=0 ;
	mode_=PM_SONG ;
	sequencerMode_=SM_SONG ;
	lastPercentage_=0 ;
	retrigAllImmediate_=false ;
#ifdef PLATFORM_RGNANO_SIM
	simStreaming_=false ;
	simStreamingPath_="" ;
#endif

	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		instrumentOnChannel_[i][0] = ' ';
		instrumentOnChannel_[i][1] = ' ';
		instrumentOnChannel_[i][2] = '\0';
		liveQueueingMode_[i]=QM_NONE ;
		liveQueuePosition_[i]=0 ;
		liveQueueChainPosition_[i]=0 ;
		timeToLive_[i]=0 ;
		timeToStart_[i]=0 ;
	}

} ;

Player *Player::GetInstance() {
	if (instance_==0) {
		instance_=new Player() ;
	}
	return instance_ ;
}

bool Player::Init(Project *project,ViewData *viewData) {

	viewData_=viewData ;
	project_=project ;

	if (!mixer_->Init(project)) {
		return false ;
	}

	mixer_->AddObserver((*this)) ;
	SyncMaster *sync=SyncMaster::GetInstance() ;
	sync->SetTempo(project_->GetTempo()) ;
	return mixer_->Start() ;
}

void Player::Reset() {
#ifdef PLATFORM_RGNANO_SIM
	simStreaming_=false ;
	simStreamingPath_="" ;
#endif
	viewData_=0 ;
	project_=0 ;
	mixer_->RemoveObserver(*this) ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		liveQueueingMode_[i]=QM_NONE ;
		liveQueuePosition_[i]=0 ;
		liveQueueChainPosition_[i]=0 ;
		timeToLive_[i]=0 ;
		timeToStart_[i]=0 ;
	}
	Close() ;
} ;

void Player::Close() {
	mixer_->Stop() ;
	mixer_->Close() ;
} ;

void Player::SetChannelMute(int channel,bool mute) {
	mixer_->SetChannelMute(channel,mute) ;
} ;

bool Player::IsChannelMuted(int channel) {
	return mixer_->IsChannelMuted(channel) ;
} ;

void Player::Start(PlayMode mode,bool forceSongMode) {
	CrashLog::Note("player start mode %d%s",mode,sequencerMode_==SM_LIVE?" live":"") ;
    
	mixer_->Lock() ;

	lastBeatCount_=0 ;
    
	// Get start time for clock
	
	System *system=System::GetInstance() ;
	now_=startClock_=system->GetClock() ;
  
  // Sets play mode.
  // DO I need playMode_ in view data ?
  // Seems like duplicate with mode_ 
 
	viewData_->playMode_=(forceSongMode?PM_SONG:mode) ;

  // Always set position, allows for playing song with chain offset
  unsigned playPos=viewData_->songY_+viewData_->songOffset_ ;
  lastSongPos_ = playPos;

  // Clear all channel based data

	for (int i=0;i<SONG_CHANNEL_COUNT;i++)
  {
		mixer_->StopChannel(i) ;
		timeToLive_[i]=0 ;
		timeToStart_[i]=0 ;
		TablePlayback &tpb=TablePlayback::GetTablePlayback(i) ;
		tpb.Stop();
  }
    
	// Tell the instruments we're starting

	project_->GetInstrumentBank()->OnStart() ;

	Groove::GetInstance()->Reset() ;

    // Let's get started !

	SyncMaster::GetInstance()->Start() ;

	firstPlayCycle_=true ;
	mode_=viewData_->playMode_ ;
	
	mixer_->OnPlayerStart() ;

	MidiService *ms=MidiService::GetInstance() ;
	ms->OnPlayerStart() ;

  switch(viewData_->playMode_)
  {
		case PM_SONG:
    {
      for (int i=0;i<8;i++)
      {
        mixer_->StartChannel(i) ;
        updateSongPos(playPos, i, viewData_->chainRow_);
      }
    }
    break ;
      
		case PM_LIVE:
    {
      for (int i=0;i<8;i++)
      {
        if ((liveQueueingMode_[i]==QM_CHAINSTART)||
            (liveQueueingMode_[i]==QM_PHRASESTART)||
            (liveQueueingMode_[i]==QM_TICKSTART))
        {
           mixer_->StartChannel(i) ;
           updateSongPos(liveQueuePosition_[i],i,liveQueueChainPosition_[i]) ;
                   liveQueueingMode_[i]=QM_NONE ;
        }
      }
    }
    break ;
      
		case PM_CHAIN:
		case PM_PHRASE:
    {
      int currentChannel=viewData_->songX_ ;
      mixer_->StartChannel(currentChannel); ;
      int currentChainPos=viewData_->chainRow_ ;
      updateSongPos(playPos,currentChannel,currentChainPos) ;
    }
    break ;
    case PM_AUDITION: {
	    int currentChannel = viewData_->songX_;
	    mixer_->StartChannel(currentChannel);
	    int currentChainPos = viewData_->chainRow_;
	    int currentPhrasePos = viewData_->phraseCurPos_;
	    // uses hop for PhrasePos
	    updateSongPos(playPos, currentChannel, currentChainPos, currentPhrasePos);
	} break;
		default:
			NInvalid ;
			break ;
	}
 
  ProcessCommands() ;

	startTime_ = mixer_->GetAudioOut()->GetStreamTime() ;

	SetChanged() ;
	PlayerEvent pe(PET_START) ;
	NotifyObservers(&pe) ;

	// Restarting mid fade: start from clean rooms instead of a half-faded tail
	SendFX::GetInstance()->CancelFade() ;

	isRunning_=true ; // keep last !!!!

	mixer_->Unlock() ;

 }

// Longest a preview note sounds before it releases itself
#define AUDITION_MAX_SECONDS 4.0

void Player::AuditionInstrument(int instrument,int note) {
	if (!project_ || !viewData_) {
		return;
	}
	if (instrument<0 || instrument>=MAX_INSTRUMENT_COUNT) {
		return;
	}
	if (note<0) note=0;
	if (note>127) note=127;
	mixer_->Lock();
	if (isRunning_ && viewData_->playMode_==PM_AUDITION) {
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			mixer_->StopChannel(i);
		}
	}
	viewData_->playMode_=PM_AUDITION;
	mode_=PM_AUDITION;
	int channel=viewData_->songX_;
	if (channel<0 || channel>=SONG_CHANNEL_COUNT) channel=0;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		if (i!=channel) {
			mixer_->StopChannel(i);
		}
	}
	I_Instrument *instr=project_->GetInstrumentBank()->GetInstrument(instrument);
	if (instr) {
		// A preview plays no phrase: its channel must not run the commands
		// of whatever phrase that channel played last
		viewData_->currentPlayPhrase_[channel]=0xFF;
		viewData_->phrasePlayPos_[channel]=0;
		mixer_->StartChannel(channel);
		mixer_->StopInstrument(channel);
		mixer_->StartInstrument(channel,instr,(unsigned char)note,true);
		// The preview's own clock: it releases itself after a few seconds
		startTime_=mixer_->GetAudioOut()->GetStreamTime();
		isRunning_=true;
		SetChanged();
		PlayerEvent pe(PET_START);
		NotifyObservers(&pe);
	}
	mixer_->Unlock();
}

void Player::ForgetInstrument(I_Instrument *instrument) {
	if (!instrument) return ;
	mixer_->Lock() ;
	mixer_->ForgetInstrument(instrument) ;
	TablePlayback::ForgetInstrument(instrument) ;
	mixer_->Unlock() ;
}

void Player::Stop() {
	CrashLog::Note("player stop") ;

	mixer_->Lock() ;

	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		mixer_->StopChannelQuickly(i) ;
	}
	// Stop means stop: anything still holding a voice that no channel is
	// fading out gets dropped, so nothing can outlive the transport
	if (project_) {
		InstrumentBank *bank=project_->GetInstrumentBank() ;
		for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {
			I_Instrument *instr=bank->GetInstrument(i) ;
			if (!instr) continue ;
			bool fading=false ;
			for (int c=0;c<SONG_CHANNEL_COUNT;c++) {
				if (mixer_->GetTailInstrument(c)==instr) {
					fading=true ;
				}
			}
			if (!fading) {
				instr->AllNotesOff() ;
			}
		}
	}

	// reverb and echo fade out instead of ringing on
	SendFX::GetInstance()->FadeOut() ;
	MidiService::GetInstance()->OnPlayerStop() ;
	mixer_->OnPlayerStop() ;

	SyncMaster::GetInstance()->Stop() ;
	isRunning_=false ;
	SetChanged() ;
	PlayerEvent pe(PET_STOP) ;
	NotifyObservers(&pe) ;

	mixer_->Unlock() ;
}

char *Player::GetPlayedNote(int channel) {
	return mixer_->GetPlayedNote(channel) ;
}

char *Player::GetPlayedOctive(int channel) {
	return mixer_->GetPlayedOctive(channel) ;
}

char *Player::GetPlayedInstrument(int channel) {
	if( (mixer_->GetPlayedOctive(channel))[1] == ' ' ){
		return mixer_->GetPlayedOctive(channel);
	} else {
		if (!IsChannelMuted(channel)) {
			return (char *)(&(instrumentOnChannel_[channel][0]));
		} else {
			return "--" ;
		}
	}
}

char *Player::GetLiveIndicator(int channel) {

	bool blink=true ;


	switch (liveQueueingMode_[channel]) {
		case QM_CHAINSTART:
		case QM_CHAINSTOP:
			blink=(now_-startClock_)%500<250 ;
			break ;
		case QM_PHRASESTART:
		case QM_PHRASESTOP:
			blink=(now_-startClock_)%125<72 ;
			break ;
		case QM_TICKSTART:
			blink=(now_-startClock_)%75<37 ;
			break ;
		case QM_NONE:
			break ;

	} ;
	if (blink) {
		switch (liveQueueingMode_[channel]) {
			case QM_CHAINSTART:
			case QM_PHRASESTART:
			case QM_TICKSTART:
				if (!IsChannelMuted(channel)) {
					return(">") ;
				} else {
					return("-") ;
				}
				break ;
			case QM_CHAINSTOP:
			case QM_PHRASESTOP:
				return "_" ;
				break ;
			case QM_NONE:
				break ;
		} 
	}
	return " " ;
} ;

void Player::SetSequencerMode(SequencerMode mode) {
	if (isRunning_ && (mode_==PM_SONG || mode_==PM_LIVE)) {
		mode_=(mode==SM_LIVE)?PM_LIVE:PM_SONG ;
		viewData_->playMode_=mode_ ;  // the PLAY: label follows
	} ;
	sequencerMode_=mode ;
} ;

SequencerMode Player::GetSequencerMode() {
	return sequencerMode_ ;
} ;

static int instrumentIndexOf(Project *project,I_Instrument *instr) {
	if (!project || !instr) return -1 ;
	InstrumentBank *bank=project->GetInstrumentBank() ;
	for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {
		if (bank->GetInstrument(i)==instr) return i ;
	}
	return -2 ;
}

int Player::GetChannelInstrumentIndex(int channel) {
	return instrumentIndexOf(project_,mixer_->GetInstrument(channel)) ;
}

bool Player::GetChannelTailVoice(int channel,int &stage,float &level) {
	SynthInstrument *synth=dynamic_cast<SynthInstrument *>(mixer_->GetTailInstrument(channel)) ;
	if (!synth) return false ;
	synth->GetVoiceDebug(channel,stage,level) ;
	return true ;
}

int Player::GetChannelTailIndex(int channel) {
	return instrumentIndexOf(project_,mixer_->GetTailInstrument(channel)) ;
}

bool Player::IsChannelPlaying(int channel) {
	return mixer_->IsChannelPlaying(channel) ;
} ;

// Handles start button on any screen BUT the song screen

void Player::OnStartButton(PlayMode origin,unsigned int from,bool startFromPrevious,unsigned char chainPos) {

	// Live mode only changes what Start does on the Song screen; everywhere
	// else Start still plays (and stops) as usual
	switch(GetSequencerMode()) {

        case SM_SONG:
        case SM_LIVE:

			// If sequencer not running, start otherwise stop

			if (isRunning_ && viewData_->playMode_ != PM_AUDITION) {
				Stop() ;
			} else {
				// Silence a preview before playing, so it cannot hang on
				if (isRunning_) {
					Stop() ;
				}
				for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
        			liveQueueingMode_[i]=QM_NONE ;
				} ;
				Start(origin,startFromPrevious) ;
			}
    		break ;
	}
}

// Handles start on song screen
void Player::OnSongStartButton(unsigned int from,unsigned int to,bool requestStop,bool forceImmediate) {

    switch(GetSequencerMode()) {

        case SM_SONG:

			// If sequencer not running, start otherwise stop

			if (isRunning_ && viewData_->playMode_ != PM_AUDITION) {
				if (!forceImmediate) {
					Stop() ;
				} else {
					// Get current song row and queue for immediate retrigger
					retrigPos_=viewData_->songY_+viewData_->songOffset_ ;
					retrigAllImmediate_=true ;
				}
			} else {
				if (isRunning_) {
					Stop() ;  // a preview was still sounding
				}
				for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
        			liveQueueingMode_[i]=QM_NONE ;
				} ;
				Start(PM_SONG,false) ;
			}
    		break ;

		case SM_LIVE:

			// Get current song row
			unsigned char songPos=viewData_->songY_+viewData_->songOffset_ ;

			if (!IsRunning()) {

				// not playing; we queue the chains in the selection
				// that contain something then start the player

				for (unsigned int i=0;i<SONG_CHANNEL_COUNT;i++) {
					if ((i<from)||(i>to)) {
						QueueChannel(i,QM_NONE,0) ;
					} else {
						if (isPlayable(songPos,i,0)) {
							QueueChannel(i,QM_CHAINSTART,songPos,0) ;
						}
  					}
				} ;
				
				Start(PM_LIVE,false) ;
				
			} else { // Player already running

				// Queue all chain in the given selection

				for (unsigned int i=from;i<to+1;i++) {

					QueueingMode mode=QM_NONE ;
				
					uchar row=songPos ;

					if (!requestStop) {
						if (findPlayable(&row,i,0)) {
							if (!forceImmediate) {
								if ((liveQueueingMode_[i]!=QM_CHAINSTART)
									||(liveQueuePosition_[i]!=row)) {
									mode=QM_CHAINSTART ;
								} else {
									mode=QM_PHRASESTART ;
 								}
							} else {
								mode=QM_TICKSTART ;
							}
						}
					} else { // modifier = onStop from song screen
						if (GetQueueingMode(i)!=QM_CHAINSTOP) {
							mode=QM_CHAINSTOP ;
						} else {
							mode=QM_PHRASESTOP ;
 						}
					}
                    if (mode!=QM_NONE) {
    					QueueChannel(i,mode,row,0) ;
                    }
 				}
			} ;
			break ;
    }

}

bool Player::IsRunning() {
	return isRunning_ ;
} ;

bool Player::Clipped() {
     return mixer_->Clipped() ;
}

bool Player::isPlayable(int row,int col,int chainPos) {

	uchar *chain=viewData_->song_->data_+8*row+col ;
	if (*chain!=0xFF) {
       uchar data=viewData_->song_->chain_->data_[16*(*chain)+chainPos] ;
    	return (data!=0xFF);
    }
    return false ;

}

bool Player::findPlayable(uchar *row,int col,uchar  chainPos) {

	// first look if current is fine

	uchar *chain=viewData_->song_->data_+8*(*row)+col ;
	if (*chain!=0xFF) {
		uchar data=viewData_->song_->chain_->data_[16*(*chain)+chainPos] ;
		return (data!=0xFF);
	}

	// Find upwards the first non blank

	while (*row!=255) {
		chain=viewData_->song_->data_+8*(*row)+col ;
		if (*chain!=0xFF) {
			break ;
		}
		*row-=1 ;
	}

	if (*row==255) return false ;

	// Find upwards the first blank

	while (*row!=255) {
		chain=viewData_->song_->data_+8*(*row)+col ;
		if (*chain==0xFF) {
			break ;
		}
		*row-=1 ;
	}

	if (*row==255) {
		*row=0 ;
	} else {
		*row+=1 ;
	}
	chain=viewData_->song_->data_+8*(*row)+col ;
	uchar data=0xFF ;
	if (*chain!=0xFF) {
		data=viewData_->song_->chain_->data_[16*(*chain)+chainPos] ;
	}
    return (data!=0xFF) ;
}

QueueingMode Player::GetQueueingMode(int i) {
	return liveQueueingMode_[i] ;
};

unsigned char Player::GetQueuePosition(int i) {
	return liveQueuePosition_[i] ;
} ;

unsigned char Player::GetQueueChainPosition(int i) {
	return liveQueueChainPosition_[i] ;
} ;

void Player::QueueChannel(int i,QueueingMode mode,unsigned char position,unsigned char chainpos) {
    liveQueueingMode_[i]=mode ;
	liveQueuePosition_[i]=position ;
	liveQueueChainPosition_[i]=chainpos ;
} ;

/************************************************************
 Update:
	this one gets called when the audio driver has just sent
	a block of audio and we get room to prepare the next one
 ************************************************************/

void Player::Update(Observable &o,I_ObservableData *d) {

	// Make sure sync's ok

	MidiService::GetInstance()->Trigger() ;
	project_->Trigger() ;

	if (isRunning_) {

		SyncMaster *sync=SyncMaster::GetInstance() ;
		sync->SetTempo(project_->GetTempo()) ;

		if (!firstPlayCycle_) {
			Groove::GetInstance()->Trigger() ;
			sync->NextSlice() ;
			triggerLiveChains_=false ;
			if (retrigAllImmediate_) {
				for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
					QueueChannel(i,QM_TICKSTART,retrigPos_,0) ;
				} ;
				retrigAllImmediate_=false ;
			}
			// Don't advance in audition mode
			if (viewData_->playMode_ != PM_AUDITION) {
				moveToNextStep() ;
			} else if (GetPlayTime() > AUDITION_MAX_SECONDS) {
				// A preview note has no note-off of its own: end it here,
				// or a pad or held sample would ring until the app quits
				Stop() ;
			}
			if (triggerLiveChains_) {
				triggerLiveChains() ;
			} ;
		} 

		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			if (timeToStart_[i]>0){
				if (--timeToStart_[i]==0) {
					playCursorPosition(i) ;
				}
			}
		}

        // Process commands in current phrase
        ProcessCommands();

        // Initialise retrigger table
        int instrRetrigger[SONG_CHANNEL_COUNT];
        memset(instrRetrigger, -1, SONG_CHANNEL_COUNT * sizeof(int));

		// Process any table commands now

		for (int channel=0;channel<SONG_CHANNEL_COUNT;channel++) {
			TablePlayback &tpb=TablePlayback::GetTablePlayback(channel) ;
			if (!tpb.GetAutomation()) {
				TablePlayerChange tpc ;
				tpc.timeToLive_=timeToLive_[channel] ;
				tpc.instrRetrigger_=-1 ;
				tpb.ProcessStep(tpc) ;
				timeToLive_[channel]=tpc.timeToLive_ ;
				instrRetrigger[channel]=tpc.instrRetrigger_ ;
			}
		}

	  // Do we need to kill a voice ?
	  
		if (sync->TableSlice()) {
			for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
				bool stopped=false ;
				if (timeToLive_[i]>0) {
					if (--timeToLive_[i]==0) {
                		mixer_->StopInstrument(i) ;
						stopped=true ;
					} 
				}
				if (!stopped) {
					if (instrRetrigger[i]>=0) {
						int note=mixer_->GetChannelNote(i) ;
						I_Instrument *instr=mixer_->GetInstrument(i) ;
						if ((note!=0xFF)&&(instr!=0)) {
							note+=(instrRetrigger[i]>80)?instrRetrigger[i]-256:instrRetrigger[i] ;
							while (note>127) {
								note-=12 ;
							} ;
							mixer_->StopInstrument(i) ;
							mixer_->StartInstrument(i,instr,note,false) ;
						} ;
					} ;
				} ;		
			}
		}

		firstPlayCycle_=false ;
		System *system=System::GetInstance() ;
		now_=system->GetClock() ;

		// Notify refresh

		PlayerEvent pe(PET_UPDATE,0) ;
		SetChanged() ;
		NotifyObservers(&pe) ;

	}
} ;

/************************************************************
 ProcessCommands:
	Check if there's any command to trigger at current playing
	position for all channels
 ************************************************************/
 
void Player::ProcessCommands() {

    // loop on all channels

	Groove *gs=Groove::GetInstance() ;

	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {

        if (mixer_->IsChannelPlaying(i)) {

            // check if there's any phrase playing

			uchar phrase=viewData_->currentPlayPhrase_[i] ;
			if (phrase!=0xFF) {
				if (gs->TriggerChannel(i)) { // If groove says it is time to play
					int pos=viewData_->phrasePlayPos_[i] ;
					FourCC cc=viewData_->song_->phrase_->cmd1_[phrase*16+pos] ;
					ushort param=viewData_->song_->phrase_->param1_[phrase*16+pos] ;
					FourCC other=viewData_->song_->phrase_->cmd2_[phrase*16+pos] ;
					// RAND in the other column: this command's value moves by
					// a random amount up to RAND's value
					if (other==I_CMD_RAND && cc!=I_CMD_RAND && cc!=I_CMD_CHNC) {
						int range=viewData_->song_->phrase_->param2_[phrase*16+pos]&0xFF ;
						int v=(param&0xFF)+randomUpTo(range) ;
						param=(param&0xFF00)|(v>0xFF?0xFF:v) ;
					}
					
					// if there's any command to trigger, first pass it on the player
					// then pass it on to the instrument
					
					if (cc!=I_CMD_NONE) {
						if (!ProcessChannelCommand(i,cc,param)) {
							I_Instrument *instrument=mixer_->GetInstrument(i) ;
							if (instrument) {
								instrument->ProcessCommand(i,cc,param) ;
							}
						} ;
					} ;

					// Now process second command row

					cc=viewData_->song_->phrase_->cmd2_[phrase*16+pos] ;
					param=viewData_->song_->phrase_->param2_[phrase*16+pos] ;
					other=viewData_->song_->phrase_->cmd1_[phrase*16+pos] ;
					if (other==I_CMD_RAND && cc!=I_CMD_RAND && cc!=I_CMD_CHNC) {
						int range=viewData_->song_->phrase_->param1_[phrase*16+pos]&0xFF ;
						int v=(param&0xFF)+randomUpTo(range) ;
						param=(param&0xFF00)|(v>0xFF?0xFF:v) ;
					}

					// if there's any command to trigger, first pass it on the player
					// then pass it on to the instrument

					if (cc!=I_CMD_NONE) {
						if (!ProcessChannelCommand(i,cc,param)) {
							I_Instrument *instrument=mixer_->GetInstrument(i) ;
							if (instrument) {
								instrument->ProcessCommand(i,cc,param) ;
							}
						} ;
					} ;
				}
			}
		}
	} ;
} ;

bool Player::ProcessChannelCommand(int channel,FourCC cmd,ushort param) {

	I_Instrument *instr=mixer_->GetInstrument(channel) ;

	switch(cmd) {
		case I_CMD_RAND:
		case I_CMD_CHNC:
			return true ;  // applied when the step plays (see above)
		case I_CMD_KILL:
			if (instr) {
                int timeToLive=(param&0xFF) ;
                timeToLive_[channel]=timeToLive+1 ;
			}
			return true ;
		case I_CMD_TMPO:
            if ((param<400)&&(param>40))
            {
                Variable*v=project_->FindVariable(VAR_TEMPO) ;
                v->SetInt(param) ;
                SyncMaster *sync=SyncMaster::GetInstance() ;
 	            sync->SetTempo(project_->GetTempo()) ;  
            }   
            return true ;
            break ;
		case I_CMD_TABL:
			{
				TableHolder *th=TableHolder::GetInstance() ;
				TablePlayback &tpb=TablePlayback::GetTablePlayback(channel) ;
				param=param&0x7F ;
				Table &table=th->GetTable(param) ;
				tpb.Start(instr,table,false) ;
				return true ;
				break ;
			}
		case I_CMD_GROV:
			{
				Groove *gr=Groove::GetInstance() ;
				bool all=(param&0xFF00)!=0 ;
				param=param&0xFF ;
				if (all) {
					for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
						gr->SetGroove(i,param) ;
					}
				} else {
					gr->SetGroove(channel,param) ;
				}
			}
      break;
    case I_CMD_STOP:
      {
        switch(GetSequencerMode())
        {
          case SM_SONG:
            Stop();
            break;
          case SM_LIVE:
//            QueueChannel(channel,QM_CHAINSTOP,0) ;
 
            mixer_->StopChannel(channel) ;
            liveQueueingMode_[channel]=QM_NONE ;
            break;
        }
      }
			break ;
		default:
			break ;
	} ;
	return false ;
} ;

/********************************************************
 triggerLiveChains:
        look if there's any chain needed to be started
		i.e. they're queued but on a channel that is not
		yet active
 ********************************************************/

void Player::triggerLiveChains() {
 
   if (mode_==PM_LIVE) {
      for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
         if (!(mixer_->IsChannelPlaying(i))&&
              ((liveQueueingMode_[i]==QM_CHAINSTART)||
              (liveQueueingMode_[i]==QM_TICKSTART)||
                (liveQueueingMode_[i]==QM_PHRASESTART))) {
                    if (findPlayable(&(liveQueuePosition_[i]),i,liveQueueChainPosition_[i])) {
                        mixer_->StartChannel(i) ;
                        updateSongPos(liveQueuePosition_[i],i,liveQueueChainPosition_[i]) ;
					}
                    liveQueueingMode_[i]=QM_NONE ;
         }
      }
    } 
} ;

/********************************************************
 updateSongPos:
	sets current song position for a give channel. the
	last parameter allow to start the song position at a
	give chain position directly (for PM_CHAIN/PM_PHRASE)
 ********************************************************/

void Player::updateSongPos(int pos,int channel,int chainPos,int hop) {
	unsigned char *data=viewData_->song_->data_+channel+8*pos ;
	viewData_->songPlayPos_[channel]=pos ;
	viewData_->currentPlayChain_[channel]=*data ;
	updateChainPos(chainPos,channel,hop) ;
} ;

/********************************************************
 updateChainPos:
	sets current chain position for a give channel
 ********************************************************/

void Player::updateChainPos(int pos,int channel,int hop) {
	unsigned char chain=viewData_->currentPlayChain_[channel] ;
	if (chain!=0xFF) {
		viewData_->chainPlayPos_[channel]=pos ;
		unsigned char *data=viewData_->song_->chain_->data_+(16*chain+pos) ;
		viewData_->currentPlayPhrase_[channel]=*data ;
		if (*data==0xFF) { // This could happen if starting in song mode on a row
			               // where a chain contains no phrase
			mixer_->StopChannel(channel) ;
		} 
	} else {
		viewData_->currentPlayPhrase_[channel]=0xFF;
		mixer_->StopChannel(channel) ;
	};
	updatePhrasePos((hop>=0)?hop:0,channel) ;
}

/********************************************************
 updatePhrasePos:
	sets current phrase position for a give channel.
 ********************************************************/

void Player::updatePhrasePos(int pos,int channel) {

	viewData_->phrasePlayPos_[channel]=pos ;

	// See if we need to delay the trigger
	timeToStart_[channel]=1 ;

	uchar phrase=viewData_->currentPlayPhrase_[channel] ;

	// Check both param colum 1 & 2

	FourCC cc=viewData_->song_->phrase_->cmd1_[phrase*16+pos] ;
	if (cc==I_CMD_DLAY) {
		ushort param=viewData_->song_->phrase_->param1_[phrase*16+pos] ;
		timeToStart_[channel]=(param&0x0F)+1 ;
	}

	cc=viewData_->song_->phrase_->cmd2_[phrase*16+pos] ;
	if (cc==I_CMD_DLAY) {
		ushort param=viewData_->song_->phrase_->param1_[phrase*16+pos] ;
		timeToStart_[channel]=(param&0x0F)+1 ;
	}
}

// Randomness for RAND / CHNC: a small fast generator (xorshift), plenty for
// music and safe to call from the audio thread
static unsigned int randomState_ = 0x9E3779B9 ;
static unsigned int nextRandom() {
	unsigned int x=randomState_ ;
	x^=x<<13 ;
	x^=x>>17 ;
	x^=x<<5 ;
	randomState_=x ;
	return x ;
}

// 0..range inclusive
static int randomUpTo(int range) {
	if (range<=0) return 0 ;
	return (int)(nextRandom()%(unsigned int)(range+1)) ;
}

// A random note up to range semitones above, moved onto the song's scale
static int randomNoteOffset(Project *project,int note,int range) {
	int offset=randomUpTo(range) ;
	int key=project->GetScaleKey() ;
	if (key<0) return offset ;
	int scale=project->GetScale() ;
	for (int tries=0;tries<12;tries++) {
		int inKey=(note+offset-key)%12 ;
		if (inKey<0) inKey+=12 ;
		if (scaleSteps[scale][inKey]) break ;
		offset=(offset>0)?offset-1:offset+1 ;
	}
	return offset ;
}

void Player::playCursorPosition(int channel) {

	int pos=viewData_->phrasePlayPos_[channel] ;

	// Get chain content and see if instr needs to be started/Stopped
    unsigned char currentPhrase=viewData_->currentPlayPhrase_[channel] ;

	if (currentPhrase!=0xFF) {

		Song *song=viewData_->song_ ;
		Phrase *phrase=song->phrase_ ;
		unsigned char note=phrase->note_[16*currentPhrase+pos]  ;
		unsigned char instr=phrase->instr_[16*currentPhrase+pos]  ;

		// CHNC: sometimes the note doesn't play (as if the step were empty).
		// RAND with no other command: a random note, in the song's scale.
		FourCC c1=phrase->cmd1_[16*currentPhrase+pos] ;
		FourCC c2=phrase->cmd2_[16*currentPhrase+pos] ;
		ushort p1=phrase->param1_[16*currentPhrase+pos] ;
		ushort p2=phrase->param2_[16*currentPhrase+pos] ;
		if (note!=0xFF) {
			int chance=(c1==I_CMD_CHNC)?(p1&0xFF):((c2==I_CMD_CHNC)?(p2&0xFF):-1) ;
			if (chance>=0 && randomUpTo(254)>=chance) {
				note=0xFF ;
				instr=0xFF ;
			}
		}
		if (note!=0xFF) {
			bool alone1=(c1==I_CMD_RAND)&&(c2==I_CMD_NONE||c2==I_CMD_RAND||c2==I_CMD_CHNC) ;
			bool alone2=(c2==I_CMD_RAND)&&(c1==I_CMD_NONE||c1==I_CMD_CHNC) ;
			if (alone1 || alone2) {
				int range=(alone1?p1:p2)&0xFF ;
				int n=note+randomNoteOffset(project_,note,range) ;
				note=(unsigned char)(n>127?127:n) ;
			}
		}

		TableHolder *th=TableHolder::GetInstance() ;
		TablePlayback &tpb=TablePlayback::GetTablePlayback(channel) ;

		if (note!=0xFF) {

			// Stop instrument if playing
			
            mixer_->StopInstrument(channel) ;
			InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;

			// get instrument for next note

			bool newInstrument=false ;

			I_Instrument *instrument ;
			if (instr!=0xFF) {
			   instrument=bank->GetInstrument(instr) ;
			   newInstrument=true ;
			}  else {
	            instrument=mixer_->GetLastInstrument(channel) ;
			}
			
			if (instrument==0) {
					instrument=bank->GetInstrument(0) ;
			}

			if (instrument!=0) {

				int chain=viewData_->currentPlayChain_[channel] ;
				int chainPos=viewData_->chainPlayPos_[channel] ;
				unsigned char *trsp=viewData_->song_->chain_->transpose_+(16*chain+chainPos) ;
				note+=*trsp ;
				note+=project_->GetTranspose() ;
				instrumentOnChannel_[channel][0] = (instr/16)>9?'A'-10+(instr/16):'0'+(instr/16);
				instrumentOnChannel_[channel][1] = (instr%16)>9?'A'-10+(instr%16):'0'+(instr%16);
				instrumentOnChannel_[channel][2] = '\0';

				// Check if note is in acceptable midi range

				if (note<128) {
					mixer_->StartInstrument(channel,instrument,note,newInstrument) ;
					int instrTable=instrument->GetTable() ;
	
					// If an instrument number has been specified && instrument has table,
					// we trigger the table.
	
					if ((instrTable!=VAR_OFF)&&(newInstrument)) {
						Table &table=th->GetTable(instrTable) ;
						bool automated=instrument->GetTableAutomation() ;
						tpb.Start(instrument,table,automated) ;
					} else {
						// if there was an instrument number, we stop the table
						if (newInstrument) {
							tpb.Stop() ;
						}
					}
				} 
				else {
					Trace::Error("Note outside range: %02x", (unsigned int) note) ;
				}
			}
		}
		if ((note!=0xFF)||(instr!=0xFF)) {
			I_Instrument *instrument=mixer_->GetInstrument(channel) ;
			if (instrument) {
				if (instrument->GetTableAutomation()) {
					TablePlayerChange tpc ;
					tpc.timeToLive_=timeToLive_[channel] ;
					tpb.ProcessStep(tpc) ;
					timeToLive_[channel]=tpc.timeToLive_ ;
				}
			}
		}
    } 
}


int Player::getChannelHop(int channel,int pos) {

  int phrase=viewData_->currentPlayPhrase_[channel] ;
	FourCC cc=viewData_->song_->phrase_->cmd1_[phrase*16+pos] ;
  if (cc==I_CMD_HOP) {
      return (viewData_->song_->phrase_->param1_[phrase*16+pos])&0xF ;
  }
	cc=viewData_->song_->phrase_->cmd2_[phrase*16+pos] ;
  if (cc==I_CMD_HOP) {
      return (viewData_->song_->phrase_->param2_[phrase*16+pos])&0xF ;
  }
  return -1 ;
}

/********************************************************
 moveToNextStep:
	Atomic routine that triggers the next step for all
	playing channels.
 ********************************************************/

void Player::moveToNextStep() 
{
  // we'll need to know if any channel is playing
  
  bool playingChannel=false ;

	for (int i=0;i<SONG_CHANNEL_COUNT;i++)
  {
		bool liveTriggered = false ; 

    switch (liveQueueingMode_[i])
    {
      case QM_TICKSTART:
        liveQueueingMode_[i]=QM_NONE ;
        if (findPlayable(&(liveQueuePosition_[i]),i,liveQueueChainPosition_[i]))
        {
          liveTriggered = true ;
          updateSongPos(liveQueuePosition_[i],i,liveQueueChainPosition_[i]) ;
        }
        return ;
        break ;
      case QM_PHRASESTART:
      case QM_PHRASESTOP:
      case QM_CHAINSTART:
      case QM_CHAINSTOP:
      case QM_NONE:
         break ;
    }

    Groove *gs=Groove::GetInstance() ;
    
	  if (mixer_->IsChannelPlaying(i)&&!liveTriggered)
    {
      playingChannel=true ;
      
      if (gs->TriggerChannel(i))
      { // If groove says it is time to play
        if (viewData_->currentPlayPhrase_[i]!=0xFF)
        {
          int pos=(viewData_->phrasePlayPos_[i])+1 ;
          if (pos!=16)
          {
            int hop=getChannelHop(i,pos) ;
            if (hop>=0)
            {
              if (mode_!=PM_PHRASE)
              {
                moveToNextPhrase(i,hop) ;
              }
              else
              {
                updatePhrasePos(hop,i) ;
              }
            }
            else
            {
                updatePhrasePos(pos,i) ;
            }
          }
          else
          { // HOP.. something should be done so that if
              // next chain has a hop on pos zero, it is effective
            if (mode_!=PM_PHRASE)
            {
              moveToNextPhrase(i) ;
            } else
            {
              updatePhrasePos(0,i) ;
            }
          }
        }
		  }
		}
	}
	// if no channel is playing we allow straight
	// queueing of chains
	if (!playingChannel) {
		triggerLiveChains_=true ;
	} ;
}

/********************************************************
 moveToNextPhrase:
	Compute what is the next phrase to be triggered on
	the selected channel. Called when the current phrase
	has reached the end
 ********************************************************/

void Player::moveToNextPhrase(int channel,int hop) {

    // First check if we're in live mode and the channel
    // has been trigged in immediate mode. In which case we
    // do the action straight away (START/STOP)

    if (mode_==PM_LIVE) {

       switch (liveQueueingMode_[channel]) {
			  case QM_TICKSTART:
              case QM_PHRASESTART:
                  if (findPlayable(&(liveQueuePosition_[channel]),channel,liveQueueChainPosition_[channel])) {
    				  updateSongPos(liveQueuePosition_[channel],channel,liveQueueChainPosition_[channel],hop) ;
                  }
				  liveQueueingMode_[channel]=QM_NONE ;
				  return ;
                  break ;
              case QM_PHRASESTOP:
					mixer_->StopChannel(channel) ;
                   liveQueueingMode_[channel]=QM_NONE ;
                   return ;
              case QM_CHAINSTART:
              case QM_CHAINSTOP:
              case QM_NONE:
                   break ;
       }
    }

    // If nothing has been triggered, we need to find what is
    // The next phrase to play

	int chain=viewData_->currentPlayChain_[channel] ;
	int pos=(viewData_->chainPlayPos_[channel])+1 ;
	
	// Look if there' any data at current position
	// which means we continue in the current chain
	
	bool canContinue=(pos<16) ;
	if (canContinue) {
		unsigned char *data=viewData_->song_->chain_->data_+(16*chain+pos) ;
		canContinue=(*data!=0xFF) ;
	}
	
	// If so, we trigger it. Otherwise, we go to the next phrase
	
	if (canContinue) {
		updateChainPos(pos,channel,hop) ;
	} else { // Should move to next chain
		if ((mode_==PM_SONG)||(mode_==PM_LIVE)) {
			moveToNextChain(channel,hop) ; // HOP. here
		} else {
			updateChainPos(0,channel,hop) ;
		} ;
	}
}

/********************************************************
 moveToNextChain:
	Compute what is the next chain to be triggered on
	the selected channel. Called when the current chain
	has reached the end.
 ********************************************************/

void Player::moveToNextChain(int channel,int hop) {

// if there's unplaying channels queue they should be started 
// once all position have been updated

    triggerLiveChains_=true ;

    bool searchNext=true ;
    int nextPos=0 ;
    int chainPosition=0 ;

   // Hop here ?

    // See if current channel has been queued to play something
    // in normal mode

    if (mode_==PM_LIVE) {
       switch (liveQueueingMode_[channel]) {

              case QM_CHAINSTART:
              case QM_PHRASESTART:

                  if (findPlayable(&(liveQueuePosition_[channel]),channel,liveQueueChainPosition_[channel])) {
                     nextPos=liveQueuePosition_[channel] ; 
                     searchNext=false ;
                     liveQueueingMode_[channel]=QM_NONE ;
				     chainPosition=liveQueueChainPosition_[channel] ;
                  } else {
                      liveQueueingMode_[channel]=QM_NONE ;
                  }
                  break ;

              case QM_CHAINSTOP:
              case QM_PHRASESTOP:
				   mixer_->StopChannel(channel) ;
                   liveQueueingMode_[channel]=QM_NONE ;
                   return ;

              case QM_NONE:
                   break ;
       }
    }
    
// if live mode didn't queue anything, we find the next to play

    if (searchNext) {
    	int pos=(viewData_->songPlayPos_[channel])+1 ;
     	unsigned char *data=viewData_->song_->data_+channel+8*pos ;
      	bool loopBack=(*data==0xFF) ;
       	// Check if first step of chain contains somethin, if not we loop back
       	if (!loopBack) {
		   unsigned char step=viewData_->song_->chain_->data_[*data*16] ;
		   loopBack=(step==0xFF) ;
	    } ;
	    if (loopBack) {
		   data-=8 ;
		   pos-- ;
		   while (pos>=0) {
                if (*data==0xFF) { // we stop searching if there's a blank
                    break ;
                } else  { // Or if first phrase of chain is empty
                    if (viewData_->song_->chain_->data_[(*data)*16]==0xFF) {
                        break ;
                    }
                } 
			    if (pos!=0) data-=8 ;
			    pos-- ;
           } ;
		   pos++ ;
     	}
     	nextPos=pos ;
    } 
    // Do a last check in case we had only one chain and it go destroyed

    if (isPlayable(nextPos,channel,chainPosition)) {
    	updateSongPos(nextPos,channel,chainPosition,hop) ;
    } else  {
        mixer_->StopChannel(channel) ;
    }
} ;

double Player::GetPlayTime() {
	AudioOut *out=mixer_->GetAudioOut() ;
	double currentTime = out->GetStreamTime() ;
	return currentTime - startTime_ ;
} ;

int Player::GetPlayedBufferPercentage() {
	unsigned int beatCount=SyncMaster::GetInstance()->GetBeatCount();
	if (beatCount!=lastBeatCount_) {
		lastBeatCount_=beatCount ;
		lastPercentage_=mixer_->GetPlayedBufferPercentage() ;
	}
	return lastPercentage_ ;
} ;

PlayerEvent::PlayerEvent(PlayerEventType type,unsigned int tickCount):ViewEvent(VET_PLAYER_POSITION_UPDATE) {
	type_=type ;
	tickCount_=tickCount ;
}

PlayerEventType PlayerEvent::GetType() {
	return type_ ;
} ;

unsigned int PlayerEvent::GetTickCount() {
	return tickCount_ ;
} ;

void Player::StartStreaming(const Path &path) {
#ifdef PLATFORM_RGNANO_SIM
	simStreaming_=true ;
	simStreamingPath_=path.GetPath() ;
	Trace::Log("RGNANO_SIM_PLAYER","stream_start path=%s",simStreamingPath_.c_str());
#endif
	mixer_->StartStreaming(path) ;
}
void Player::StopStreaming() {
	mixer_->StopStreaming() ;
#ifdef PLATFORM_RGNANO_SIM
	if (simStreaming_) {
		Trace::Log("RGNANO_SIM_PLAYER","stream_stop path=%s",simStreamingPath_.c_str());
	}
	simStreaming_=false ;
	simStreamingPath_="" ;
#endif
}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
static const char *SimPlayModeName(PlayMode mode) {
	switch (mode) {
		case PM_SONG: return "song";
		case PM_CHAIN: return "chain";
		case PM_PHRASE: return "phrase";
		case PM_LIVE: return "live";
		case PM_AUDITION: return "audition";
		default: return "unknown";
	}
}

static const char *SimSequencerModeName(SequencerMode mode) {
	switch (mode) {
		case SM_SONG: return "song";
		case SM_LIVE: return "live";
		default: return "unknown";
	}
}

static const char *SimQueueModeName(QueueingMode mode) {
	switch (mode) {
		case QM_NONE: return "none";
		case QM_CHAINSTART: return "chain-start";
		case QM_PHRASESTART: return "phrase-start";
		case QM_CHAINSTOP: return "chain-stop";
		case QM_PHRASESTOP: return "phrase-stop";
		case QM_TICKSTART: return "tick-start";
		default: return "unknown";
	}
}

std::string Player::GetSimDebugSummary() {
	std::ostringstream out;
	out << "player(running=" << (isRunning_ ? "yes" : "no")
		<< " mode=" << SimPlayModeName(mode_)
		<< " viewMode=" << (viewData_ ? SimPlayModeName(viewData_->playMode_) : "none")
		<< " sequencer=" << SimSequencerModeName(sequencerMode_);
	if (isRunning_) {
		out << " time=" << GetPlayTime();
	}
#ifdef PLATFORM_RGNANO_SIM
	out << " streaming=" << (simStreaming_ ? "yes" : "no");
	if (simStreaming_) {
		out << " streamPath=\"" << simStreamingPath_ << "\"";
	}
#endif
	out << ")";
	if (viewData_) {
		out << " channels=[";
		bool any=false;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			bool playing=IsChannelPlaying(i);
			QueueingMode queue=liveQueueingMode_[i];
			if (playing || queue!=QM_NONE) {
				if (any) {
					out << "; ";
				}
				out << i << ":";
				out << (playing ? "play" : "idle");
				out << " song=" << viewData_->songPlayPos_[i]
					<< " chain=";
				if (viewData_->currentPlayChain_[i]==0xFF) out << "--"; else out << (int)viewData_->currentPlayChain_[i];
				out << "/" << viewData_->chainPlayPos_[i]
					<< " phrase=";
				if (viewData_->currentPlayPhrase_[i]==0xFF) out << "--"; else out << (int)viewData_->currentPlayPhrase_[i];
				out << "/" << viewData_->phrasePlayPos_[i]
					<< " note=" << GetPlayedNote(i) << GetPlayedOctive(i)
					<< " inst=" << GetPlayedInstrument(i);
				if (queue!=QM_NONE) {
					out << " queue=" << SimQueueModeName(queue)
						<< "@" << (int)liveQueuePosition_[i]
						<< "/" << (int)liveQueueChainPosition_[i];
				}
				any=true;
			}
		}
		if (!any) {
			out << "none";
		}
		out << "]";
	}
	return out.str();
}
#endif

#ifdef PLATFORM_RGNANO_SIM
std::string Player::GetSimStreamingPath() const {
	return simStreamingPath_;
}

bool Player::IsSimStreaming() const {
	return simStreaming_;
}
#endif

std::string Player::GetAudioAPI() {
	AudioOut *out=mixer_->GetAudioOut() ;
	return (out)?out->GetAudioAPI():"" ;
} ;

std::string Player::GetAudioDevice() {
	AudioOut *out=mixer_->GetAudioOut() ;
	return (out)?out->GetAudioDevice():"" ;
} ;

int Player::GetAudioBufferSize() {
	AudioOut *out=mixer_->GetAudioOut() ;
	return (out)?out->GetAudioBufferSize():0 ;
} ;

int Player::GetAudioRequestedBufferSize() {
	AudioOut *out=mixer_->GetAudioOut() ;
	return (out)?out->GetAudioRequestedBufferSize():0 ;
}

int Player::GetAudioPreBufferCount() {
	AudioOut *out=mixer_->GetAudioOut() ;
	return (out)?out->GetAudioPreBufferCount():0 ;
} ;

