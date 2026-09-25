#ifndef _PROJECT_H_
#define _PROJECT_H_

#include "Song.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Persistency/Persistent.h"
#include "Foundation/Variables/VariableContainer.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Observable.h"

#define VAR_TEMPO MAKE_FOURCC('T', 'M', 'P', 'O')
#define VAR_MASTERVOL   	MAKE_FOURCC('M', 'S', 'T', 'R')
#define VAR_WRAP        	MAKE_FOURCC('W', 'R', 'A', 'P')
#define VAR_MIDIDEVICE  	MAKE_FOURCC('M', 'I', 'D', 'I')
#define VAR_TRANSPOSE   	MAKE_FOURCC('T', 'R', 'S', 'P')
#define VAR_SOFTCLIP 		MAKE_FOURCC('S', 'F', 'T', 'C')
#define VAR_SOFTCLIP_GAIN 	MAKE_FOURCC('S', 'F', 'G', 'N')
#define VAR_PREGAIN   		MAKE_FOURCC('P', 'R', 'G', 'N')
#define VAR_SCALE_KEY 		MAKE_FOURCC('S', 'K', 'E', 'Y')
#define VAR_SCALE 			MAKE_FOURCC('S', 'C', 'A', 'L')
#define VAR_NOTE_NAMES 		MAKE_FOURCC('N', 'N', 'A', 'M')
#define VAR_RENDER MAKE_FOURCC('R', 'N', 'D', 'R')
#define VAR_REVERB_SIZE MAKE_FOURCC('R', 'V', 'S', 'Z')
#define VAR_REVERB_DAMP MAKE_FOURCC('R', 'V', 'D', 'P')
#define VAR_DELAY_STEPS MAKE_FOURCC('D', 'L', 'S', 'T')
#define VAR_DELAY_FEEDBACK MAKE_FOURCC('D', 'L', 'F', 'B')
#define VAR_CHORUS_RATE MAKE_FOURCC('C', 'H', 'R', 'T')
#define VAR_CHORUS_DEPTH MAKE_FOURCC('C', 'H', 'D', 'P')
// Master EQ: gain 80 = 0 dB (00 -12 dB, FF +12 dB), frequency 00..FF per band
#define VAR_EQ_LOW_GAIN MAKE_FOURCC('E', 'Q', 'L', 'G')
#define VAR_EQ_LOW_FREQ MAKE_FOURCC('E', 'Q', 'L', 'F')
#define VAR_EQ_MID_GAIN MAKE_FOURCC('E', 'Q', 'M', 'G')
#define VAR_EQ_MID_FREQ MAKE_FOURCC('E', 'Q', 'M', 'F')
#define VAR_EQ_HIGH_GAIN MAKE_FOURCC('E', 'Q', 'H', 'G')
#define VAR_EQ_HIGH_FREQ MAKE_FOURCC('E', 'Q', 'H', 'F')
// Master limiter (LIMIT screen, right of EQ); drive 00 = off
#define VAR_LIM_DRIVE MAKE_FOURCC('L', 'I', 'M', 'D')
#define VAR_LIM_CEILING MAKE_FOURCC('L', 'I', 'M', 'C')
#define VAR_LIM_ATTACK MAKE_FOURCC('L', 'I', 'M', 'A')
#define VAR_LIM_RELEASE MAKE_FOURCC('L', 'I', 'M', 'R')

#define PROJECT_NUMBER "1"
#define PROJECT_RELEASE "6"
#define BUILD_COUNT "0-bacon14"

#define MAX_TAP 3

class Project: public Persistent,public VariableContainer,I_Observer  {
public:
  Project();
  ~Project();
  void Purge();
  void PurgeInstruments(bool removeFromDisk);

  Song *song_;

  int GetMasterVolume();
  bool Wrap();
  void OnTempoTap();
  void NudgeTempo(int value);
  int GetScale();
  int GetNoteNameMode();
  int GetTempo(); // Takes nudging into account
  int GetTranspose();
  int GetSoftclip();
  int GetSoftclipGain();
  int GetPregain();
  int GetRenderMode();
  int GetScaleKey();
  void Trigger();

  static const unsigned int MAX_RENDER_MODE = 3;
  // I_Observer
  virtual void Update(Observable &o, I_ObservableData *d);

  InstrumentBank *GetInstrumentBank();
  virtual void SaveContent(TiXmlNode *node);
  virtual void RestoreContent(TiXmlElement *element);

  void LoadFirstGen(const char *root);

protected:
  void buildMidiDeviceList();

private:
  InstrumentBank *instrumentBank_;
  char **midiDeviceList_;
  int midiDeviceListSize_;
  int tempoNudge_;
  unsigned long lastTap_[MAX_TAP];
  unsigned int tempoTapCount_;
};
#endif
