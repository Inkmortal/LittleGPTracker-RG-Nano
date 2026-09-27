#include "SoundLibrary.h"
#include "InstrumentBank.h"
#include "SampleInstrument.h"
#include "SamplePool.h"
#include "Application/Model/Config.h"
#include "Application/Model/Project.h"
#include "Application/Model/Table.h"
#include "Application/Persistency/PersistencyService.h"
#include "System/Console/Trace.h"
#include "System/FileSystem/FileSystem.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOUND_EXT ".lgs"
#define KIT_EXT ".lgk"
#define TEMPLATE_NAME "template"

std::string SoundLibrary::Folder() {
	const char *lib=Config::GetInstance()->GetValue("SOUNDLIB") ;
	Path path((lib && lib[0])?lib:"root:../Sounds") ;
	std::string folder=path.GetPath() ;
	while (folder.size()>1 && (folder[folder.size()-1]=='/' || folder[folder.size()-1]=='\\')) {
		folder.erase(folder.size()-1) ;
	}
	return folder ;
}

static std::string kitFolder() {
	return SoundLibrary::Folder()+"/kits" ;
}

static std::string sampleFolder() {
	return SoundLibrary::Folder()+"/samples" ;
}

static std::string templatePath() {
	return SoundLibrary::Folder()+"/" TEMPLATE_NAME KIT_EXT ;
}

static bool ensureDir(const std::string &path) {
	FileSystem *fs=FileSystem::GetInstance() ;
	if (fs->GetFileType(path.c_str())==FT_DIR) return true ;
	if (fs->MakeDir(path.c_str()).Failed()) {
		Trace::Error("SOUNDLIB can't create %s",path.c_str()) ;
		return false ;
	}
	return true ;
}

static bool ensureFolders() {
	return ensureDir(SoundLibrary::Folder()) && ensureDir(kitFolder()) &&
	       ensureDir(sampleFolder()) ;
}

static long fileSize(const std::string &path) {
	I_File *f=FileSystem::GetInstance()->Open(path.c_str(),(char *)"r") ;
	if (!f) return -1 ;
	f->Seek(0,SEEK_END) ;
	long size=f->Tell() ;
	f->Close() ;
	delete f ;
	return size ;
}

std::string SoundLibrary::CleanName(const std::string &name) {
	std::string clean ;
	for (size_t i=0;i<name.size();i++) {
		char c=name[i] ;
		bool ok=(c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') ||
		        c==' ' || c=='-' || c=='_' || c=='.' ;
		clean+=ok?c:'_' ;
	}
	// No leading dots or spaces (hidden files, odd names), none at the end
	while (!clean.empty() && (clean[0]=='.' || clean[0]==' ')) clean.erase(0,1) ;
	while (!clean.empty() && clean[clean.size()-1]==' ') clean.erase(clean.size()-1) ;
	return clean ;
}

static void listFolder(const std::string &folder,const char *ext,std::vector<std::string> &names) {
	names.clear() ;
	I_Dir *dir=FileSystem::GetInstance()->Open(folder.c_str()) ;
	if (!dir) return ;
	std::string mask="*" ;
	mask+=ext ;
	dir->GetContent((char *)mask.c_str()) ;
	IteratorPtr<Path> it(dir->GetIterator()) ;
	size_t extLen=strlen(ext) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		std::string name=it->CurrentItem().GetName() ;
		if (name.empty() || name[0]=='.' || name.size()<=extLen) continue ;
		name.erase(name.size()-extLen) ;
		if (name==TEMPLATE_NAME && !strcmp(ext,KIT_EXT)) continue ;
		names.push_back(name) ;
	}
	delete dir ;
	std::sort(names.begin(),names.end()) ;
}

void SoundLibrary::ListSounds(std::vector<std::string> &names) {
	listFolder(Folder(),SOUND_EXT,names) ;
}

void SoundLibrary::ListKits(std::vector<std::string> &names) {
	listFolder(kitFolder(),KIT_EXT,names) ;
}

static bool readDoc(const std::string &path,PersistencyDocument &doc) {
	I_File *file=FileSystem::GetInstance()->Open(path.c_str(),(char *)"r") ;
	if (!file) return false ;
	file->Seek(0,SEEK_END) ;
	long length=file->Tell() ;
	file->Seek(0,SEEK_SET) ;
	char *buffer=(char *)malloc(length+1) ;
	if (!buffer) {
		file->Close() ;
		delete file ;
		return false ;
	}
	file->Read(buffer,1,length) ;
	buffer[length]=0 ;
	file->Close() ;
	delete file ;
	doc.Parse(buffer) ;
	free(buffer) ;
	return !doc.Error() ;
}

static Variable *findNamed(I_Instrument *instr,const char *name) {
	IteratorPtr<Variable> it(instr->GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		Variable &v=it->CurrentItem() ;
		if (!strcmp(v.GetName(),name)) return &v ;
	}
	return 0 ;
}

// The library's copy of a song sample: the same file when it's there
// already, else a new name so a different sample of that name stays
static bool storeSample(const std::string &name,std::string &stored) {
	std::string src=Path(std::string("samples:")+name).GetPath() ;
	long size=fileSize(src) ;
	if (size<0) return false ;
	std::string stem=name,ext ;
	size_t dot=name.rfind('.') ;
	if (dot!=std::string::npos) {
		stem=name.substr(0,dot) ;
		ext=name.substr(dot) ;
	}
	for (int n=1;n<100;n++) {
		std::string candidate=stem ;
		if (n>1) {
			char suffix[8] ;
			sprintf(suffix,"-%d",n) ;
			candidate+=suffix ;
		}
		candidate+=ext ;
		std::string dst=sampleFolder()+"/"+candidate ;
		long have=fileSize(dst) ;
		if (have==size) {
			stored=candidate ;
			return true ;
		}
		if (have<0) {
			if (!SamplePool::CopyFile(src.c_str(),dst.c_str())) return false ;
			stored=candidate ;
			return true ;
		}
	}
	return false ;
}

// One slot into node: <INSTRUMENT>, its <TABLE> and <SAMPLE> if it has them
static bool writeSound(TiXmlNode *node,InstrumentBank *bank,int slot,std::string &error) {
	I_Instrument *instr=bank->GetInstrument(slot) ;
	InstrumentBank::SaveInstrument(node,-1,instr) ;
	TiXmlElement *saved=node->LastChild()?node->LastChild()->ToElement():0 ;
	if (!saved) {
		error="nothing to save" ;
		return false ;
	}
	if (instr->GetType()==IT_SAMPLE) {
		int index=((SampleInstrument *)instr)->GetSampleIndex() ;
		const char *name=SamplePool::GetInstance()->GetName(index) ;
		if (name) {
			std::string stored ;
			if (!storeSample(name,stored)) {
				error="can't copy the sample" ;
				return false ;
			}
			TiXmlElement sample("SAMPLE") ;
			sample.SetAttribute("FILE",stored.c_str()) ;
			node->InsertEndChild(sample) ;
			// The sound plays the library's copy, by its name there
			for (TiXmlElement *p=saved->FirstChildElement();p;p=p->NextSiblingElement()) {
				const char *n=p->Attribute("NAME") ;
				if (n && !strcmp(n,"sample")) p->SetAttribute("VALUE",stored.c_str()) ;
			}
		}
	}
	Variable *table=findNamed(instr,"table") ;
	if (table && table->GetInt()>=0 && table->GetInt()<TABLE_COUNT) {
		Table &t=TableHolder::GetInstance()->GetTable(table->GetInt()) ;
		if (!t.IsEmpty()) {
			TiXmlElement data("TABLE") ;
			TiXmlNode *tableNode=node->InsertEndChild(data) ;
			t.Save(tableNode) ;
		}
	}
	return true ;
}

// A sound element's content into slot: sample into the song first, then
// the instrument, then its table in a free table of this song
static bool readSound(TiXmlElement *sound,InstrumentBank *bank,int slot,int version,std::string &error) {
	TiXmlElement *instrument=sound->FirstChildElement("INSTRUMENT") ;
	if (!instrument) {
		error="not a sound file" ;
		return false ;
	}
	TiXmlElement *sample=sound->FirstChildElement("SAMPLE") ;
	if (sample && sample->Attribute("FILE")) {
		const char *file=sample->Attribute("FILE") ;
		SamplePool *pool=SamplePool::GetInstance() ;
		if (pool->FindSample(file)<0) {
			Path src(sampleFolder()+"/"+file) ;
			if (!src.Exists()) {
				error=std::string("missing ")+file ;
				return false ;
			}
			if (pool->ImportSample(src)<0) {
				error=pool->TakeLoadTooBig()?"sample too long: no memory":"can't load the sample" ;
				return false ;
			}
		}
	}
	int tableId=-1 ;
	TiXmlElement *table=sound->FirstChildElement("TABLE") ;
	if (table) {
		tableId=TableHolder::GetInstance()->GetNext() ;
		if (tableId==NO_MORE_TABLE) {
			error="no free table" ;
			return false ;
		}
	}
	if (!bank->RestoreInstrument(instrument,slot,version,true)) {
		error="can't use it in this slot" ;
		return false ;
	}
	I_Instrument *instr=bank->GetInstrument(slot) ;
	Variable *tableVar=findNamed(instr,"table") ;
	if (table) {
		Table &t=TableHolder::GetInstance()->GetTable(tableId) ;
		t.Reset() ;
		t.Restore(table) ;
		if (tableVar) tableVar->SetInt(tableId) ;
	} else if (tableVar) {
		tableVar->SetInt(-1) ;
	}
	return true ;
}

static int fileVersion(TiXmlElement *root) {
	const char *v=root->Attribute("VERSION") ;
	return v?int(atof(v)*100):100 ;
}

static bool saveDoc(TiXmlDocument &doc,std::string &error) {
	if (!doc.SaveFile()) {
		error="can't write to the card" ;
		return false ;
	}
	return true ;
}

bool SoundLibrary::SaveSound(InstrumentBank *bank,int slot,const std::string &name,std::string &error) {
	std::string clean=CleanName(name) ;
	if (clean.empty()) {
		error="give it a name" ;
		return false ;
	}
	if (slot<0 || slot>=MAX_SAMPLEINSTRUMENT_COUNT) {
		error="MIDI can't be saved" ;
		return false ;
	}
	I_Instrument *instr=bank->GetInstrument(slot) ;
	if (instr->GetType()==IT_SAMPLE && instr->IsEmpty()) {
		error="empty slot" ;
		return false ;
	}
	if (!ensureFolders()) {
		error="can't make the Sounds folder" ;
		return false ;
	}
	std::string path=Folder()+"/"+clean+SOUND_EXT ;
	TiXmlDocument doc(path.c_str()) ;
	TiXmlElement root("SOUND") ;
	root.SetAttribute("VERSION",PROJECT_NUMBER) ;
	root.SetAttribute("NAME",clean.c_str()) ;
	TiXmlNode *node=doc.InsertEndChild(root) ;
	if (!writeSound(node,bank,slot,error)) return false ;
	if (!saveDoc(doc,error)) return false ;
	Trace::Log("SOUNDLIB","saved slot %02X as %s",slot,path.c_str()) ;
	return true ;
}

bool SoundLibrary::LoadSound(InstrumentBank *bank,int slot,const std::string &name,std::string &error) {
	if (slot<0 || slot>=MAX_SAMPLEINSTRUMENT_COUNT) {
		error="not a sound slot" ;
		return false ;
	}
	std::string path=Folder()+"/"+name+SOUND_EXT ;
	PersistencyDocument doc(path) ;
	if (!readDoc(path,doc)) {
		error="can't read that sound" ;
		return false ;
	}
	TiXmlElement *root=doc.FirstChildElement("SOUND") ;
	if (!root) {
		error="not a sound file" ;
		return false ;
	}
	if (!readSound(root,bank,slot,fileVersion(root),error)) return false ;
	Trace::Log("SOUNDLIB","loaded %s into slot %02X",path.c_str(),slot) ;
	return true ;
}

static bool saveKitTo(InstrumentBank *bank,const std::string &path,const std::string &name,std::string &error) {
	if (!ensureFolders()) {
		error="can't make the Sounds folder" ;
		return false ;
	}
	TiXmlDocument doc(path.c_str()) ;
	TiXmlElement root("KIT") ;
	root.SetAttribute("VERSION",PROJECT_NUMBER) ;
	root.SetAttribute("NAME",name.c_str()) ;
	TiXmlNode *node=doc.InsertEndChild(root) ;
	int count=0 ;
	for (int i=0;i<MAX_SAMPLEINSTRUMENT_COUNT;i++) {
		I_Instrument *instr=bank->GetInstrument(i) ;
		if (instr->IsEmpty()) continue ;
		TiXmlElement sound("SOUND") ;
		char hex[3] ;
		sprintf(hex,"%02X",i) ;
		sound.SetAttribute("SLOT",hex) ;
		TiXmlNode *soundNode=node->InsertEndChild(sound) ;
		if (!writeSound(soundNode,bank,i,error)) return false ;
		count++ ;
	}
	if (count==0) {
		error="no sounds to save" ;
		return false ;
	}
	if (!saveDoc(doc,error)) return false ;
	Trace::Log("SOUNDLIB","saved kit %s: %d sounds",path.c_str(),count) ;
	return true ;
}

static bool loadKitFrom(InstrumentBank *bank,const std::string &path,std::string &error) {
	PersistencyDocument doc(path) ;
	if (!readDoc(path,doc)) {
		error="can't read that kit" ;
		return false ;
	}
	TiXmlElement *root=doc.FirstChildElement("KIT") ;
	if (!root) {
		error="not a kit file" ;
		return false ;
	}
	int version=fileVersion(root) ;
	TiXmlElement *bySlot[MAX_SAMPLEINSTRUMENT_COUNT] ;
	memset(bySlot,0,sizeof(bySlot)) ;
	for (TiXmlElement *s=root->FirstChildElement("SOUND");s;s=s->NextSiblingElement("SOUND")) {
		const char *slot=s->Attribute("SLOT") ;
		if (!slot) continue ;
		int id=(int)strtol(slot,0,16) ;
		if (id>=0 && id<MAX_SAMPLEINSTRUMENT_COUNT) bySlot[id]=s ;
	}
	int count=0 ;
	for (int i=0;i<MAX_SAMPLEINSTRUMENT_COUNT;i++) {
		if (!bySlot[i]) {
			if (!bank->GetInstrument(i)->IsEmpty()) bank->ClearSlot(i) ;
			continue ;
		}
		std::string slotError ;
		if (!readSound(bySlot[i],bank,i,version,slotError)) {
			char msg[40] ;
			snprintf(msg,sizeof(msg),"%02X: %s",i,slotError.c_str()) ;
			error=msg ;
			Trace::Error("SOUNDLIB kit %s slot %02X: %s",path.c_str(),i,slotError.c_str()) ;
			return false ;
		}
		count++ ;
	}
	Trace::Log("SOUNDLIB","loaded kit %s: %d sounds",path.c_str(),count) ;
	return true ;
}

bool SoundLibrary::SaveKit(InstrumentBank *bank,const std::string &name,std::string &error) {
	std::string clean=CleanName(name) ;
	if (clean.empty() || clean==TEMPLATE_NAME) {
		error="give it a name" ;
		return false ;
	}
	return saveKitTo(bank,kitFolder()+"/"+clean+KIT_EXT,clean,error) ;
}

bool SoundLibrary::LoadKit(InstrumentBank *bank,const std::string &name,std::string &error) {
	return loadKitFrom(bank,kitFolder()+"/"+name+KIT_EXT,error) ;
}

bool SoundLibrary::SoundExists(const std::string &name) {
	std::string clean=CleanName(name) ;
	return !clean.empty() && fileSize(Folder()+"/"+clean+SOUND_EXT)>=0 ;
}

bool SoundLibrary::KitExists(const std::string &name) {
	std::string clean=CleanName(name) ;
	return !clean.empty() && fileSize(kitFolder()+"/"+clean+KIT_EXT)>=0 ;
}

bool SoundLibrary::SetTemplate(InstrumentBank *bank,std::string &error) {
	return saveKitTo(bank,templatePath(),TEMPLATE_NAME,error) ;
}

bool SoundLibrary::HasTemplate() {
	return fileSize(templatePath())>0 ;
}

bool SoundLibrary::ClearTemplate() {
	if (!HasTemplate()) return false ;
	FileSystem::GetInstance()->Delete(templatePath().c_str()) ;
	return !HasTemplate() ;
}

bool SoundLibrary::LoadTemplate(InstrumentBank *bank,std::string &error) {
	return loadKitFrom(bank,templatePath(),error) ;
}
