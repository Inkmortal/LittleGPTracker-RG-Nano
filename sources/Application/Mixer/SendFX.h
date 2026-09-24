#ifndef _SEND_FX_H_
#define _SEND_FX_H_

// Global send effects (M8-style): instruments add a copy of their output
// to the reverb/delay sends while rendering, then this module runs after
// every channel bus in the master mix and adds the wet signal.

#include "Services/Audio/AudioModule.h"
#include "Foundation/T_Singleton.h"

#define SENDFX_COMBS 8
#define SENDFX_ALLPASSES 4
#define SENDFX_MAX_FRAMES 8192
#define SENDFX_DELAY_SECONDS 2

class Project ;

struct SendFXComb {
	float *buffer_ ;
	int size_ ;
	int index_ ;
	float store_ ;
} ;

struct SendFXAllpass {
	float *buffer_ ;
	int size_ ;
	int index_ ;
} ;

class SendFX: public AudioModule, public T_Singleton<SendFX> {
public:
	SendFX() ;
	virtual ~SendFX() ;

	void SetProject(Project *project) ;

	// Called by instruments from their Render: frames of stereo float,
	// 1.0 == 16 bit full scale.
	void AddSend(int channel,const float *stereo,int frames,float reverb,float delay) ;
	// A muted channel must not be heard through the reverb/echo either
	void SetChannelMuted(int channel,bool muted) ;
	bool IsActive() ;
	// Debug: input seen this buffer, frames of tail left, fade frames left
	void GetDebugState(bool &input,int &tail,int &fade) ;

	virtual bool Render(fixed *buffer,int samplecount) ;

	void Clear() ;
	// Fade the wet return to silence over ~0.25 s, then clear the tails
	void FadeOut() ;
	void CancelFade() ;

	// Parameter curves, shared with the Project screen text
	static float ReverbSizeFromParam(int value) ;
	static int DelayStepsFromParam(int value) ;

private:
	void updateSettings() ;
	void allocate(int sampleRate) ;

	Project *project_ ;
	int sampleRate_ ;
	bool allocated_ ;
	bool hasInput_ ;
	int tail_ ;              // frames left to render after the last input
	int fade_ ;              // frames left in a FadeOut, 0 = none
	int fadeLength_ ;

	float reverbIn_[SENDFX_MAX_FRAMES*2] ;
	float delayIn_[SENDFX_MAX_FRAMES*2] ;

	SendFXComb combs_[2][SENDFX_COMBS] ;
	SendFXAllpass allpasses_[2][SENDFX_ALLPASSES] ;
	float feedback_ ;
	float damp_ ;
	float reverbLevel_ ;

	float *delayBuffer_ ;
	int delaySize_ ;
	int delayWrite_ ;
	int delayFrames_ ;
	float delayFeedback_ ;
	float delayLevel_ ;
	float delayLp_[2] ;
	bool muted_[8] ;
} ;

#endif
