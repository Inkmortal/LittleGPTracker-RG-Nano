#ifndef _PLAYER_H_
#define _PLAYER_H_
#include "Foundation/T_Singleton.h"
#include "Foundation/Observable.h"
#include "System/Timer/Timer.h"
#include "Application/Views/ViewData.h"
#include "Application/Views/BaseClasses/ViewEvent.h"
#include "PlayerMixer.h"
#include "SyncMaster.h"

enum PlayerEventType {
	PET_START,
	PET_UPDATE,
	PET_STOP
} ;

enum SequencerMode {
     SM_SONG,
     SM_LIVE
} ;

enum QueueingMode {
     QM_NONE,
     QM_CHAINSTART,
     QM_PHRASESTART,
     QM_CHAINSTOP,
	 QM_PHRASESTOP,
	 QM_TICKSTART
} ;

class PlayerEvent: public ViewEvent {
public:
	PlayerEvent(PlayerEventType type,unsigned int tickCount=0) ;
	PlayerEventType GetType() ;
	unsigned int GetTickCount() ;
private:
	PlayerEventType type_ ;
	unsigned int tickCount_ ;
} ;

class Player: public I_Observer,public Observable,public T_Singleton<Player> {
private: // Singleton
	Player() ;
public:


	static Player *GetInstance() ;
	bool Init(Project*,ViewData *) ;
	void Reset() ;
	void Close() ;
	
	
	virtual void Update(Observable &o,I_ObservableData *d) ;

	// basic interface

	void Start(PlayMode mode,bool forceSongMode) ;
	void Stop() ;
	void AuditionInstrument(int instrument,int note) ;
	void ForgetInstrument(I_Instrument *instrument) ;
	
//	void Toggle(PlayMode mode,bool forceSongMode=false) ;
//	void ChangePlayMode(PlayMode mode) ;
//	PlayMode GetPlayMode() ;
	
	void SetSequencerMode(SequencerMode mode) ;
	SequencerMode GetSequencerMode() ;
	
	void OnStartButton(PlayMode origin,unsigned int from,bool startFromLastPos,unsigned char chainPos) ;
	void OnSongStartButton(unsigned int from,unsigned int to,bool requestStop,bool forceImmediate) ;
	
	bool IsRunning() ;
	bool Clipped() ;
	
	void ProcessCommands() ;
	bool ProcessChannelCommand(int channel,FourCC cmd,ushort param) ;

	void StartStreaming(const Path &path) ;
	void StopStreaming() ;

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	std::string GetSimDebugSummary() ;
#endif
#ifdef PLATFORM_RGNANO_SIM
	std::string GetSimStreamingPath() const ;
	bool IsSimStreaming() const ;
	// Sequencer state for sim checks: notes (started on the track),
	// last_note, pass (of the playing phrase), roll_volume, vibrato,
	// table_tick, table_row; -1 unknown name
	int GetSimValue(const std::string &name,int channel) ;
	// The last 2*period notes started on the track repeat with that period
	bool SimNotesRepeat(int channel,int period) ;
#endif

	// Channel data
	
	bool IsChannelPlaying(int channel) ;
	// Debug: instrument number still attached to a channel (-1 none), and
	// the one whose release tail is still rendering
	int GetChannelInstrumentIndex(int channel) ;
	int GetChannelTailIndex(int channel) ;
	// Debug: envelope stage/level of the synth voice rendering as a tail
	bool GetChannelTailVoice(int channel,int &stage,float &level) ;
	void SetChannelMute(int channel,bool mute) ;
	bool IsChannelMuted(int channel) ;
	
	// Live queuing

	QueueingMode GetQueueingMode(int i) ;
	unsigned char GetQueuePosition(int i) ;
	unsigned char GetQueueChainPosition(int i) ;
	void QueueChannel(int i,QueueingMode mode,unsigned char position,unsigned char chainpos=0) ;

	char *GetLiveIndicator(int channel) ;
	double GetPlayTime() ;

	char *GetPlayedNote(int channel) ;
	char *GetPlayedOctive(int channel) ;
	char *GetPlayedInstrument(int channel) ;

	// info
	int GetPlayedBufferPercentage() ;

	std::string GetAudioAPI() ;
	std::string GetAudioDevice() ;
	int GetAudioBufferSize() ;
	int GetAudioRequestedBufferSize() ;
	int GetAudioPreBufferCount() ;

protected:
	void updateSongPos(int position,int channel,int chainPos=0,int hop=-1) ;
	void updateChainPos(int position,int channel,int hop=0) ;
	void updatePhrasePos(int pos,int channel) ;
	void playCursorPosition(int channel) ;
    int  getChannelHop(int channel,int pos) ;
	void moveToNextStep() ;
	void moveToNextPhrase(int channel,int hop=-1) ;
	void moveToNextChain(int channel,int hop) ;
	// Per-tick sequencer effects (ROLL, VIBR)
	void updateTrackEffects() ;
	void startRoll(int channel,ushort param,int volume) ;
	void stopVibrato(int channel) ;
	// A note just started on the track (count it, cancel ROLL)
	void onNoteStarted(int channel,unsigned char note,bool newInstrument) ;
	// Time a phrase starts on a track (NTH counts passes)
	void countPhrasePass(int channel) ;
	int nextRandom(int channel) ;
	int randomUpTo(int channel,int range) ;
	int randomNoteOffset(int channel,int note,int range) ;
	void seedRandom(int channel,int seed) ;
	void resetTrackEffects() ;

    void triggerLiveChains() ;
    
    bool isPlayable(int row,int col,int chainPos=0) ;
    bool findPlayable(uchar *row,int col,uchar chainPos=0) ;

private:

	PlayerMixer *mixer_ ;
	ViewData *viewData_ ;
	Project *project_ ;

	SequencerMode sequencerMode_ ;
	PlayMode mode_ ;
	bool isRunning_ ;

	unsigned long startClock_ ;    // .Used to time display live queued chains
								   //  for blinking effect
	unsigned long now_ ;
	int lastPercentage_ ;
	int lastBeatCount_ ;
	unsigned char lastSongPos_ ;
	bool firstPlayCycle_ ;
	bool triggerLiveChains_ ;

	double startTime_ ;

	char instrumentOnChannel_[SONG_CHANNEL_COUNT][3];

	// Live queuing system

    unsigned char liveQueuePosition_[SONG_CHANNEL_COUNT] ;
    QueueingMode liveQueueingMode_[SONG_CHANNEL_COUNT] ;
	unsigned char liveQueueChainPosition_[SONG_CHANNEL_COUNT] ;
	unsigned int timeToLive_[SONG_CHANNEL_COUNT] ;
	unsigned int timeToStart_[SONG_CHANNEL_COUNT] ;

	bool retrigAllImmediate_ ;
	unsigned char retrigPos_ ;

	// Per-track sequencer state
	unsigned int randomState_[SONG_CHANNEL_COUNT] ;   // RAND, CHNC, SEED
	int rollTicks_[SONG_CHANNEL_COUNT] ;    // ROLL: ticks between hits, 0 off
	int rollCount_[SONG_CHANNEL_COUNT] ;    //       ticks to the next hit
	int rollVolume_[SONG_CHANNEL_COUNT] ;   //       volume of the last hit
	int rollStep_[SONG_CHANNEL_COUNT] ;     //       volume change per hit
	bool rollOnce_[SONG_CHANNEL_COUNT] ;    //       a single re-strike
	int vibratoSpeed_[SONG_CHANNEL_COUNT] ; // VIBR, 0 off
	int vibratoDepth_[SONG_CHANNEL_COUNT] ;
	unsigned int vibratoPhase_[SONG_CHANNEL_COUNT] ;
	unsigned short phrasePass_[SONG_CHANNEL_COUNT][PHRASE_COUNT] ; // NTH
	unsigned int notesStarted_[SONG_CHANNEL_COUNT] ;
	unsigned char noteHistory_[SONG_CHANNEL_COUNT][32] ;

#ifdef PLATFORM_RGNANO_SIM
	bool simStreaming_ ;
	std::string simStreamingPath_ ;
#endif
	
} ;

#endif
