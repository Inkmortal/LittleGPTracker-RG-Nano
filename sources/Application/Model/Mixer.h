
#ifndef _MIXER_H_
#define _MIXER_H_

#include "Foundation/T_Singleton.h"
#include "Application/Persistency/Persistent.h"

#include "Song.h"
#include "Application/Utils/fixed.h"

// Levels on the Mixer screen, 00-FF with C0 = unity (0 dB): the 8 tracks,
// then the effect returns
#define MIXER_LEVEL_REVERB 8
#define MIXER_LEVEL_DELAY 9
#define MIXER_LEVEL_CHORUS 10
#define MIXER_LEVELS 12
#define MIXER_UNITY 0xC0

class Mixer:public T_Singleton<Mixer>,Persistent {
public:
	Mixer() ;
	~Mixer() ;
	void Clear() ;

	inline int GetBus(int i) { return channelBus_[i]  ; } ;

	int GetLevel(int i) { return (i>=0 && i<MIXER_LEVELS)?levels_[i]:MIXER_UNITY ; } ;
	void SetLevel(int i,int value) ;
	// Linear gain for a level: C0 -> 1.0
	float GetGain(int i) { return GetLevel(i)/(float)MIXER_UNITY ; } ;

	virtual void SaveContent(TiXmlNode *node) ;
	virtual void RestoreContent(TiXmlElement *element);
private:
	char channelBus_[SONG_CHANNEL_COUNT] ;
	unsigned char levels_[MIXER_LEVELS] ;
} ;

#endif
