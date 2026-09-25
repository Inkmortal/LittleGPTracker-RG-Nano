#ifndef _SAMPLE_INSTRUMENT_H_
#define _SAMPLE_INSTRUMENT_H_

#include "I_Instrument.h"
#include "SampleRenderingParams.h"
#include "SRPUpdaters.h"
#include "ModSources.h"
#include "InstrumentEQ.h"

#include "SoundSource.h"
#include "Application/Model/Song.h" 
#include "Foundation/Observable.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/WatchedVariable.h"

// Play modes, as on the M8 sampler (FWD, REV, FWDLOOP, REVLOOP, FWD PP,
// REV PP, OSC, OSC REV, OSC PP) plus LGPT's tempo-locked loop. Songs save
// the name (see loopModeNames), so this order is free to change; old names
// are translated by SampleInstrument::CanonicalLoopModeName.
enum SampleInstrumentLoopMode {
    SILM_ONESHOT = 0,    // forward: S to E, once
    SILM_REVERSE,        // E back to S, once
    SILM_LOOP,           // S to E, then L to E again and again
    SILM_REVLOOP,        // E back to L, again and again
    SILM_LOOP_PINGPONG,  // S to E, then back and forth between L and E
    SILM_REV_PINGPONG,   // E back to L, then back and forth
    SILM_OSC,            // L..E is one wave cycle, pitched by the note
    SILM_OSC_REV,        // the cycle played backwards
    SILM_OSC_PINGPONG,   // the cycle there and back
    SILM_LOOPSYNC,       // L..E stretched to one bar at the song tempo
    SILM_LAST
};

#define NO_SAMPLE (-1)
#define SIP_VOLUME    		MAKE_FOURCC('V','O','L','M')
#define SIP_CRUSH 	  		MAKE_FOURCC('C','R','S','H')
#define SIP_CRUSHVOL 	  	MAKE_FOURCC('C','R','S','V')
#define SIP_DOWNSMPL 	  	MAKE_FOURCC('D','S','P','L')
#define SIP_ROOTNOTE  		MAKE_FOURCC('R','O','O','T')
#define SIP_FINETUNE  		MAKE_FOURCC('F','N','T','N')
#define SIP_PAN 	  		MAKE_FOURCC('P','A','N','_')
#define SIP_START       	MAKE_FOURCC('S','T','R','T')
#define SIP_END       		MAKE_FOURCC('E','N','D','_')
#define SIP_LOOPMODE  		MAKE_FOURCC('L','M','O','D')
#define SIP_LOOPSTART 		MAKE_FOURCC('L','S','T','A')
#define SIP_LOOPLEN 		MAKE_FOURCC('L','L','E','N')
#define SIP_INTERPOLATION   MAKE_FOURCC('I','N','T','P')
#define SIP_SAMPLE 		    MAKE_FOURCC('S','M','P','L')
#define SIP_SLICES 		    MAKE_FOURCC('S','L','C','S')
#define SIP_FILTMODE		MAKE_FOURCC('F','I','M','O') 
#define SIP_ATTENUATE		MAKE_FOURCC('F','I','A','T')
#define SIP_FILTMIX			MAKE_FOURCC('F','M','I','X')
#define SIP_FILTCUTOFF		MAKE_FOURCC('F','C','U','T')
#define SIP_FILTRESO		MAKE_FOURCC('F','R','E','S')
#define SIP_TABLE			MAKE_FOURCC('T','A','B','L')
#define SIP_TABLEAUTO		MAKE_FOURCC('T','B','L','A')
#define SIP_FBTUNE			MAKE_FOURCC('F','B','T','U')
#define SIP_FBMIX			MAKE_FOURCC('F','B','M','X')
#define SIP_PRINTFX MAKE_FOURCC('P', 'R', 'F', 'X')
#define SIP_IR_PAD MAKE_FOURCC('I', 'R', 'P', 'D')
#define SIP_IR_WET MAKE_FOURCC('I', 'R', 'W', 'T')
// Shared reverb / echo sends, same as the synth's MIX page
#define SIP_REVERB MAKE_FOURCC('S', 'R', 'V', 'B')
#define SIP_DELAY MAKE_FOURCC('S', 'D', 'L', 'Y')
#define SIP_CHORUS MAKE_FOURCC('S', 'C', 'H', 'O')

#define FB_BUFFER_LENGTH 3500 // (in samples)

class SampleInstrument: public I_Instrument,I_Observer {

public:
       SampleInstrument() ;
       virtual ~SampleInstrument() ;
       // I_Instrument implementation
	   virtual bool Init() ;
       virtual bool Start(int channel,unsigned char note,bool trigger=true) ;
       virtual void Stop(int channel) ;
       virtual bool Render(int channel,fixed *buffer,int size,bool updateTick) ;
       void sendToEffects(int channel,fixed *buffer,int size) ;
       virtual void AllNotesOff() ;
       virtual bool IsReleasing(int channel) ;
       virtual InstrumentMods *GetMods() { return &mods_ ; } ;
       virtual void StopQuickly(int channel) ;
       virtual bool IsInitialized() ;
	   virtual bool IsEmpty() ;

	   virtual InstrumentType GetType() { return IT_SAMPLE ; } ;
  	   virtual void ProcessCommand(int channel,FourCC cc,ushort value) ;
	   virtual void Purge() ;
	   virtual int GetTable() ;
	   virtual bool GetTableAutomation();
	   virtual void GetTableState(TableSaveState &state) ;	 
	   virtual void SetTableState(TableSaveState &state) ;	 

	   bool IsMulti() ;

	   // Play mode helpers (see SampleInstrumentLoopMode)
	   static bool IsReverseMode(int mode) ;
	   static bool IsLoopingMode(int mode) ;
	   static bool IsPingPongMode(int mode) ;
	   static bool IsOscMode(int mode) ;
	   // The same direction, looping or not (the preview's once/loop)
	   static int WithLooping(int mode,bool loop) ;
	   // A note's path through the markers: it starts at first, each pass
	   // ends at end (either way round), a loop jumps back to restart
	   static void PlayPath(int mode,int S,int L,int E,int &first,int &end,int &restart) ;
	   // Saved name of an old song -> today's name ("none" -> "forward")
	   static const char *CanonicalLoopModeName(const char *saved) ;
	   static const char *GetLoopModeName(int mode) ;
	   // Put a new sample on this instrument now (even while it plays) and
	   // set its markers: sample editing keeps S/L/E where they belong
	   void ReplaceSample(int index,int start,int loopStart,int end) ;
	   int DetectRootNoteSuggestion() ;
	   int DetectRootNoteSuggestionFromTrim() ;
	   int GetSuggestedRootNote() ;
	   void ClearRootNoteSuggestion() ;
	   bool AcceptSuggestedRootNote() ;

	  // Engine playback  start callback

	  virtual void OnStart() ;

	   // I_Observer
       virtual void Update(Observable &o,I_ObservableData *d);
       // Additional
       void AssignSample(int i) ;
	   int GetSampleIndex() ;
	   int GetVolume() ;
	   void SetVolume(int) ;
	   int GetSampleSize(int channel=-1) ;
       int GetLoopEnd();
       virtual const char *GetName();
       virtual const char *GetFileName();
 
  static void EnableDownsamplingLegacy();

protected:
		void updateInstrumentData(bool search) ;
		void doTickUpdate(int channel) ;
		void doKRateUpdate(int channel) ;
		void updateFeedback(renderParams *rp) ;
		// Where a voice starts, where each pass ends and where a loop
		// restarts, for its play mode; reposition: move the playhead there
		void setupVoicePlayback(renderParams *rp,bool cleanstart,bool reposition) ;

private:
       int DetectRootNoteSuggestionInRange(int rangeStart, int rangeEnd) ;
       SoundSource *source_ ;
       struct renderParams renderParams_[SONG_CHANNEL_COUNT] ;
       bool running_ ;
       bool dirty_ ;
	   TableSaveState tableState_ ;
	   
	   static int lastMidiNote_[SONG_CHANNEL_COUNT] ;
	   static fixed lastSample_[SONG_CHANNEL_COUNT][2] ;
	   static fixed feedback_[SONG_CHANNEL_COUNT][FB_BUFFER_LENGTH*2] ;

	   Variable *volume_ ;
	   Variable *crush_ ;
	   Variable *cutoff_ ;
	   Variable *reso_ ;
	   Variable *table_ ;
	   Variable *tableAuto_ ;
	   Variable *downsample_ ;
	   Variable *rootNote_ ;
	   Variable *fineTune_ ;
	   Variable *drive_ ;
	   Variable *fbMix_ ;
	   Variable *fbTune_ ;
	   WatchedVariable *start_ ;
	   WatchedVariable *loopStart_ ;
	   WatchedVariable *loopEnd_ ;
	   Variable *filterMix_ ;
	   Variable *filterMode_ ;
	   Variable *attenuate_ ;
	   Variable *pan_ ;
	   Variable *loopMode_ ;
	   Variable *slices_ ;
	   Variable *interpolation_ ;
       Variable *printFx_;
       Variable *irPad_;
	   Variable *irWet_;
	   Variable *reverb_;
	   Variable *delay_;
	   Variable *chorus_;
	   InstrumentMods mods_;
	   InstrumentEQ eq_;
	   Variable *customName_;
	   int suggestedRootNote_;

       static bool useDirtyDownsampling_;
       char *fxPresets[4];
} ;
#endif
