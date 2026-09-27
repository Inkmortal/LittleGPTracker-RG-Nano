#ifndef _SONG_H_
#define _SONG_H_

#include "Chain.h"
#include "Phrase.h"
#include "Application/Persistency/Persistent.h"

#define SONG_CHANNEL_COUNT 8
#define SONG_ROW_COUNT 256

#define MAX_SAMPLEINSTRUMENT_COUNT 0x80
#define MAX_MIDIINSTRUMENT_COUNT 0x10

#define MAX_INSTRUMENT_COUNT (MAX_SAMPLEINSTRUMENT_COUNT+MAX_MIDIINSTRUMENT_COUNT)

class Song:Persistent {
public:
	Song() ;
	~Song() ;

	virtual void SaveContent(TiXmlNode *node) ;
	virtual void RestoreContent(TiXmlElement *element);

	unsigned char *data_ ;
	Chain *chain_ ;
	Phrase *phrase_ ;
	// Song rows marked on the Song screen (A + Select), 1 = bookmarked;
	// LB + Up/Down stops at them
	unsigned char bookmarks_[SONG_ROW_COUNT] ;
	// Every phrase of 'chain' replaced by a new copy, so editing them
	// leaves the originals alone (a phrase used twice gets one copy).
	// False when the phrases ran out part way.
	bool DeepClonePhrases(unsigned char chain) ;
	// A new chain whose phrases are new copies of src's: NO_MORE_CHAIN
	// when there's no free chain. *complete=false: phrases ran out part
	// way, so some rows still share the original phrase.
	unsigned short DeepCloneChain(unsigned char src, bool *complete) ;
	bool IsBookmarked(int row) ;
	void ToggleBookmark(int row) ;
} ;

#endif
