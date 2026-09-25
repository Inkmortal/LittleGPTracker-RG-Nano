#ifndef _TABLE_PLAYBACK_H_
#define _TABLE_PLAYBACK_H_

#include "Application/Model/Table.h"
#include "Application/Model/Song.h"
#include "Application/Model/Groove.h"

class I_Instrument ;

class TablePlayerChange{
public:
	int timeToLive_ ;
	int instrRetrigger_ ;
} ;

struct TablePlayback {
public:
	void Init(int i) ;
	void ProcessStep(TablePlayerChange &tpc) ;
	bool ProcessLocalCommand(int row,FourCC *commandList,ushort *paramList,TablePlayerChange &tpc) ;
	void Start(I_Instrument *,Table&,bool automated) ;
	void Stop() ;
	int GetPlaybackPosition(int channel) ;
	Table *GetTable() ;
	bool GetAutomation() ;
	// TICK: one row every 'ticks' ticks (0: follow the groove, the default)
	void SetTickRate(int ticks) ;
	int GetTickRate() ;
	// THOP: every column jumps to this row
	void JumpTo(int row) ;

	static void Reset() ;
	static void ForgetInstrument(I_Instrument *instrument) ;
	static TablePlayback &GetTablePlayback(int channel) ;
private:
	Table *table_ ;
	int position_[3] ;
	int previous_[3] ;
	bool hopped_[3] ;
	I_Instrument *instrument_ ;
	int channel_ ;
	bool automated_ ;
	uchar hopCount_[TABLE_STEPS][3] ;
	ChannelGroove groove_ ;
	int tickRate_ ;   // TICK, 0 = groove
	int tickCount_ ;  // ticks spent on the current row with a tick rate
	int jumpTo_ ;     // THOP row waiting for the next row, -1 none

	static TablePlayback playback_[SONG_CHANNEL_COUNT] ;
} ;

class TableSaveState {
public:
	void Reset() ;
	uchar hopCount_[TABLE_STEPS][3] ;
	int position_[3] ;
} ;

#endif
