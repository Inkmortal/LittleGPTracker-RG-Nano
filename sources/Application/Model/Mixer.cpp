
#include "Mixer.h"
#include "Application/Utils/HexBuffers.h"

Mixer::Mixer():Persistent("MIXER")  {
	Clear() ;
} ;

Mixer::~Mixer() {
} ;

void Mixer::Clear() {

	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		channelBus_[i]=i ;
	}
	for (int i=0;i<MIXER_LEVELS;i++) {
		levels_[i]=MIXER_UNITY ;
	}
} ;

void Mixer::SetLevel(int i,int value) {
	if (i<0 || i>=MIXER_LEVELS) return ;
	if (value<0) value=0 ;
	if (value>0xFF) value=0xFF ;
	levels_[i]=(unsigned char)value ;
}

void Mixer::SaveContent(TiXmlNode *node) {
	saveHexBuffer(node,"LEVELS",levels_,MIXER_LEVELS) ;
} ;

void Mixer::RestoreContent(TiXmlElement *element) {
	// Songs saved before the mixer had levels keep the unity defaults
	TiXmlElement *current=element->FirstChildElement() ;
	if (current) {
		restoreHexBuffer(current,levels_) ;
	}
}
