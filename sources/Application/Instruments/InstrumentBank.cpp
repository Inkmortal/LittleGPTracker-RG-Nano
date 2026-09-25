
#include "InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/MidiInstrument.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Player/Player.h"
#include "System/io/Status.h"
#include "System/Console/Trace.h"
#include "Application/Utils/char.h"
#include "Application/Model/Config.h"
#include "Application/Persistency/PersistencyService.h"
#include "Filters.h"

char *InstrumentTypeData[IT_LAST]= {
	"Sample",
	"Midi",
	"Synth",
	"Macro"
} ;

// New projects start with a playable synth kit so the first note makes sound.
static const char *starterKit[]= {
	"kick","snare","hat","openhat","clap","bass","lead","pad",
	"pluck","keys","bell","acid","subbass","chip","tom","perc"
} ;
#define STARTER_KIT_SIZE ((int)(sizeof(starterKit)/sizeof(char *)))

static I_Instrument *createInstrument(InstrumentType type) {
	switch (type) {
		case IT_MIDI:
			return new MidiInstrument() ;
		case IT_SYNTH:
			return new SynthInstrument() ;
		case IT_MACRO:
			return new MacroInstrument() ;
		default:
			return new SampleInstrument() ;
	}
}


// Contain all instrument definition

InstrumentBank::InstrumentBank():Persistent("INSTRUMENTBANK") {

   	for (int i=0;i<MAX_SAMPLEINSTRUMENT_COUNT;i++) {
        SampleInstrument *s=new SampleInstrument() ;
        instrument_[i]=s ;
    }
	for (int i=0;i<MAX_MIDIINSTRUMENT_COUNT;i++) {
        MidiInstrument *s=new MidiInstrument() ;
        s->SetChannel(i) ;
        instrument_[MAX_SAMPLEINSTRUMENT_COUNT+i]=s ;
    }
    Status::Set("All instrument loaded") ;
} ;

//
// Assigns default instruments value for new project
//

void InstrumentBank::AssignDefaults() {

	SamplePool *pool=SamplePool::GetInstance() ;
	int sampleCount=pool->GetNameListSize() ;
   	for (int i=0;i<MAX_SAMPLEINSTRUMENT_COUNT;i++) {
		if (sampleCount==0 && i<STARTER_KIT_SIZE) {
			SetInstrumentType(i,IT_SYNTH) ;
			((SynthInstrument *)instrument_[i])->LoadPreset(starterKit[i]) ;
			continue ;
		}
		SetInstrumentType(i,IT_SAMPLE) ;
		SampleInstrument *s=(SampleInstrument*)instrument_[i] ;
		if (i<sampleCount) {
	        s->AssignSample(i) ;
		} else {
			s->AssignSample(-1) ;
		} 
    } ;
} ;

bool InstrumentBank::SetInstrumentType(int i,InstrumentType type) {
	if (i<0 || i>=MAX_SAMPLEINSTRUMENT_COUNT) return false ;
	if (type!=IT_SAMPLE && type!=IT_SYNTH && type!=IT_MACRO) return false ;
	I_Instrument *old=instrument_[i] ;
	if (old && old->GetType()==type) return true ;

	// Nothing in the player may keep pointing at the instrument we delete
	if (old) {
		Player::GetInstance()->ForgetInstrument(old) ;
	}
	I_Instrument *instr=createInstrument(type) ;
	instr->Init() ;
	instrument_[i]=instr ;
	delete old ;
	Trace::Log("INSTRUMENT","slot %02X type %s",i,InstrumentTypeData[type]) ;
	return true ;
}

InstrumentBank::~InstrumentBank() {
	for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {
		delete instrument_[i] ;
	}	
} ;

I_Instrument *InstrumentBank::GetInstrument(int i) {
	return instrument_[i] ;
} ;

void InstrumentBank::SaveContent(TiXmlNode *node) {
	char hex[3] ;
	for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {

		I_Instrument *instr=instrument_[i] ;
		if (!instr->IsEmpty()) {
			TiXmlElement data("INSTRUMENT") ;
			hex2char(i,hex) ;
			data.SetAttribute("ID",hex) ;
			data.SetAttribute("TYPE",InstrumentTypeData[instr->GetType()]) ;

			IteratorPtr<Variable> it(instr->GetIterator()) ;
			int count=0 ;
			for (it->Begin();!it->IsDone();it->Next()) {
				Variable &v=it->CurrentItem() ;
				TiXmlElement param("PARAM") ;
				param.SetAttribute("NAME",v.GetName()) ;
				param.SetAttribute("VALUE",v.GetString()) ;
				data.InsertEndChild(param) ;
				count++ ;
			}
			if (count) node->InsertEndChild(data) ;
		}
	}
} ;

void InstrumentBank::RestoreContent(TiXmlElement *element) {

	TiXmlElement *current=element->FirstChildElement() ;

	PersistencyDocument *doc=(PersistencyDocument *)element->GetDocument() ;
  if (doc->version_ < 130)
  {
    if (Config::GetInstance()->GetValue("LEGACYDOWNSAMPLING") != NULL)
    {
      SampleInstrument::EnableDownsamplingLegacy();
    }
  }
	while (current) {

		// Check it is an instrument
		
		if (!strcmp(current->Value(),"INSTRUMENT")) {

			// Get the instrument ID
			
			const char* hexid=current->Attribute("ID") ;
			unsigned char b1=(c2h__(hexid[0]))<<4 ;
			unsigned char b2=c2h__(hexid[1]) ;
			unsigned char id=b1+b2 ;			

			InstrumentType it=IT_LAST ;
			const char* instype=current->Attribute("TYPE") ;
			if (instype) {
				for (int i=0;i<IT_LAST;i++) {
					if (!strcmp(instype,InstrumentTypeData[i])) {
						it=(InstrumentType)i ;
						break ;
					}
				}
			} else {
				it=(id<MAX_SAMPLEINSTRUMENT_COUNT)?IT_SAMPLE:IT_MIDI ;
			} ;
			if (id<MAX_INSTRUMENT_COUNT) {
        I_Instrument *instr=instrument_[id] ;
				if (instr->GetType()!=it) {
					delete instr ;
					instr=createInstrument(it) ;
					instrument_[id]=instr ;
				} ;

        TiXmlElement *param=current->FirstChildElement() ;
				while (param) {
					const char *name=param->Attribute("NAME") ;
					const char *value=param->Attribute("VALUE") ;

          // Convert old filter dist to newer filter mode

          if (!strcmp(name,"filter dist"))
          {
            name = "filter mode";
            if (!strcmp(value,"none"))
            {
              value = "original";
            }
            else
            {
              value = "scream";
            }
          }

					IteratorPtr<Variable> it(instr->GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						Variable &v=it->CurrentItem() ;
						if (!strcmp(v.GetName(),name)) {
							v.SetString(value) ;
						} ;
					}
					param=param->NextSiblingElement() ;
				}
				if (doc->version_<38) {
					Variable *cvl=instr->FindVariable(SIP_CRUSHVOL) ;
					Variable *vol=instr->FindVariable(SIP_VOLUME);
					Variable *crs=instr->FindVariable(SIP_CRUSH) ;
					if ((vol)&&(cvl)&&(crs)) {
						if (crs->GetInt()!=16) {
							int temp=vol->GetInt() ;
							vol->SetInt(cvl->GetInt()) ;
							cvl->SetInt(temp) ;
						}
					} ;
				}
			}
		}
		current=current->NextSiblingElement() ;
	} ;
};

void InstrumentBank::Init() {
	for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {
		instrument_[i]->Init() ;
	}
}

unsigned short InstrumentBank::GetNext() {
	for (int i=0;i<MAX_SAMPLEINSTRUMENT_COUNT;i++) {
		if (instrument_[i]->GetType()!=IT_SAMPLE) continue ;
		SampleInstrument *si=(SampleInstrument *)instrument_[i] ;
		Variable *sample=si->FindVariable(SIP_SAMPLE) ;
		if (sample) {
			if (sample->GetInt()==-1) {
				return i ;
			}
		}
	}
	return NO_MORE_INSTRUMENT ;
} ;

unsigned short InstrumentBank::Clone(unsigned short i) {
	// can't clone midi instruments

	unsigned short next=GetNext() ;
	if (next==NO_MORE_INSTRUMENT) 
  {
		return NO_MORE_INSTRUMENT ;
	}

	I_Instrument *src=instrument_[i] ;
	I_Instrument *dst=instrument_[next] ;

  if (src == dst)
  {
		return NO_MORE_INSTRUMENT ;
	}

	Player::GetInstance()->ForgetInstrument(dst) ;
	delete dst ;
  
	dst=createInstrument(src->GetType()) ;
	instrument_[next]=dst ;
	IteratorPtr<Variable> it(src->GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		Variable &srcV=it->CurrentItem() ;
		Variable *dstV=dst->FindVariable(srcV.GetID()) ;
		if (dstV) {
			dstV->CopyFrom(srcV) ;
		}
	}
	// A sample instrument picks up its sound from the copied sample index
	dst->Init() ;
	return next ;

}

void InstrumentBank::OnStart() {
	for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {
		instrument_[i]->OnStart() ;
	}
	init_filters() ;
} ;
