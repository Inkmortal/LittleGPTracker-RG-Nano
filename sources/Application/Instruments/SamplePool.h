
#ifndef _SAMPLE_POOL_H_
#define _SAMPLE_POOL_H_

#include "Foundation/T_Singleton.h"
#include "WavFile.h"
#include "Application/Model/Song.h"
#include "Foundation/Observable.h"

#define MAX_PIG_SAMPLES MAX_SAMPLEINSTRUMENT_COUNT

enum SamplePoolEventType {
	SPET_INSERT,
	SPET_DELETE
} ;

struct SamplePoolEvent: public I_ObservableData {
	SamplePoolEventType type_ ;
	int index_ ;
} ;

class SamplePool: public T_Singleton<SamplePool>,public Observable {
public:
	void Load() ;
    void Sort();
    SamplePool();
	void Reset() ;
	~SamplePool() ;
	SoundSource *GetSource(int i) ;
	const char *GetName(int i) ;
	char **GetNameList() ;
	int GetNameListSize();
    int ImportSample(Path &path);
    // Load a file already in the project's samples folder (e.g. a render)
    int AddProjectSample(const char *name);
    bool IsImported(std::string name);
    // int InsertSample(const std::string& sampleName, bool imported, std::string fi);
    int Reassign(std::string name, bool imported);
    void PurgeSample(int i) ;
	const char *GetSampleLib() ;
protected:
  void unload(int i);
  // showStatus: the full-screen "Loading" line, for song loading only
  bool loadSample(const char *path, bool showStatus = true);
  bool loadSoundFont(const char *path);
  int getIndexOf(const char *path);
  int count_;
  char *names_[MAX_PIG_SAMPLES];
  SoundSource *wav_[MAX_PIG_SAMPLES];
};

#endif
