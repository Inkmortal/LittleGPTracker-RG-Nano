#ifndef _INSTRUMENT_BANK_H_
#define _INSTRUMENT_BANK_H_

#include "Application/Persistency/Persistent.h"
#include "Application/Model/Song.h"
#include "Application/Instruments/I_Instrument.h"
#include <string>

#define NO_MORE_INSTRUMENT 0x100

class InstrumentBank: public Persistent {
public:
	InstrumentBank() ;
	~InstrumentBank() ;
	void AssignDefaults() ;
	I_Instrument *GetInstrument(int i) ;
	virtual void SaveContent(TiXmlNode *node);
	virtual void RestoreContent(TiXmlElement *element);
	void Init() ;
	void OnStart() ;
	unsigned short GetNext() ;
	unsigned short Clone(unsigned short i) ;
	// Swap a sample slot between the sample and synth engines
	bool SetInstrumentType(int i,InstrumentType type) ;
	// Apply one saved parameter the way a song load does (old names too)
	static void RestoreParam(I_Instrument *instr,const char *name,const char *value) ;
	// One instrument as the song file writes it: <INSTRUMENT TYPE> with a
	// <PARAM> per setting (id<0: no ID, for sound and kit files)
	static void SaveInstrument(TiXmlNode *node,int id,I_Instrument *instr) ;
	// Put an <INSTRUMENT> element into slot id, as a song load does.
	// live: a song is open, so the player lets go of the old sound first
	// and the new one is set up (Init) right away. version: of the file.
	bool RestoreInstrument(TiXmlElement *element,int id,int version,bool live) ;
	static InstrumentType TypeFromName(const char *name) ;
	// A sample slot back to empty (no sample), letting go of the old sound
	void ClearSlot(int id) ;
	// Why the new-song template kit didn't load ("" when it did or there
	// is none): AssignDefaults can't show it, the song screen does
	std::string TakeTemplateError() { std::string e=templateError_ ; templateError_.clear() ; return e ; }
private:
	std::string templateError_ ;
	I_Instrument *instrument_[MAX_INSTRUMENT_COUNT] ;
} ;

#endif
