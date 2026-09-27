#ifndef _SOUND_PREVIEW_H_
#define _SOUND_PREVIEW_H_

#include "Application/Model/Song.h"

class I_Instrument;
class InstrumentBank;
class SDLGUIWindowImp;

// A picture of one instrument's sound in a framed box: a sample's whole
// recording, two cycles of a synth, a macro synth model's output. Shared by
// the instrument list and the Rack.
class SoundPreview {
public:
  SoundPreview();
  // The sound changed (edited, replaced): draw it again from scratch
  void Invalidate() { for_ = 0; }
  // Box at x,y (pixels), w by h
  void Draw(SDLGUIWindowImp *imp, InstrumentBank *bank, int slot, int x, int y,
            int w, int h);

private:
  void cache(I_Instrument *instr, int columns);
  I_Instrument *for_;
  int columns_;
  signed char min_[200];
  signed char max_[200];
};

// "SYN", "SMP", "MAC", "MID", "---" (empty sample slot)
const char *InstrumentTypeTag(I_Instrument *instr);
// Phrases that use each instrument (a phrase counts once)
void CountInstrumentUsage(Song *song, int usage[MAX_INSTRUMENT_COUNT]);

#endif
