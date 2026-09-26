#ifndef _AUDIO_DRIVER_H_
#define _AUDIO_DRIVER_H_

#include "Foundation/Observable.h"
#include "AudioSettings.h"

#define SOUND_BUFFER_COUNT 50
#define SOUND_BUFFER_MAX 20000

struct AudioBufferData {
   char *buffer_ ;
   int size_ ;
   void *driverData_ ;
} ;

class AudioDriver: public Observable {

public:
  class Event: public I_ObservableData
  {
  public:
    enum Type 
    {
      ADET_DRIVERTICK,
      ADET_BUFFERNEEDED
    };
    
    Event(Type type)
    {
      type_=type;
    };
    Type type_;
  };
  
public:
	AudioDriver(AudioSettings &settings) ;
	virtual ~AudioDriver() ;

	virtual bool Init() ;
	virtual void Close() ;	
	virtual bool Start() ;
	virtual void Stop() ;	

	virtual bool InitDriver()=0 ;
	virtual void CloseDriver()=0 ;
	virtual bool StartDriver()=0 ;
	virtual void StopDriver()=0 ; 

	virtual bool Interlaced()=0 ;
	virtual int GetPlayedBufferPercentage()=0 ;   
 
	virtual double GetStreamTime()=0 ; // in secs

	void AddBuffer(short *buffer,int size) ; // size in samples

	AudioSettings GetAudioSettings() ;
#ifdef PLATFORM_RGNANO_SIM
	static void ResetSimAudioStats();
	static int GetSimAudioPeak();
	static void SetSimAudioMuted(bool muted);
	static unsigned long GetSimAudioNonSilentBytes();
	static bool BeginSimAudioCapture(const char *path);
	static void EndSimAudioCapture();
	static unsigned long GetSimAudioCaptureBytes();
#endif

	void OnNewBufferNeeded() ;

	// Audio engine load: time spent rendering a buffer / the buffer's play
	// time, in percent (smoothed, and the worst since the last read).
	// Over 100 means the device cannot keep up and the output drops out.
	static int GetRenderLoadPercent() ;
	static int TakeRenderLoadPeak() ;
	// Highest smoothed load since the last call, from wall time (what
	// GetRenderLoadPercent shows) and from the render thread's own CPU
	// time; wall well above cpu means other threads (the screen) took the
	// CPU while a buffer was being rendered
	static void TakeSmoothedLoadPeaks(int &wall,int &cpu) ;
	// Times the output ran dry while playing (heard as clicks or crackle)
	static unsigned long GetUnderrunCount() ;

protected:
	static void noteUnderrun() ;
	void eatBuffer(void *buffer,int size) ; // size in bytes
	void onAudioBufferTick() ;
	bool hasData() ;
	AudioSettings settings_ ;
  
protected:
	bool isPlaying_ ;
	AudioBufferData pool_[SOUND_BUFFER_COUNT] ;
	int poolQueuePosition_ ;
	int poolPlayPosition_ ;
	int bufferPos_ ;
	int bufferSize_ ;
	bool hasData_ ;
} ;
#endif
