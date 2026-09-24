#ifndef _RENDER_TO_SAMPLE_DIALOG_H_
#define _RENDER_TO_SAMPLE_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>

// Plays the current phrase (or chain) once and records the mix into a new
// WAV in the song's samples folder, then puts it on a free instrument:
// resampling, since the RG Nano has no audio input.
class RenderToSampleDialog : public ModalView {
public:
  // bars: how many 16-step bars to record (1 for a phrase, the chain length
  // for a chain); mode is PM_PHRASE or PM_CHAIN
  RenderToSampleDialog(View &view, int mode, int bars);
  virtual ~RenderToSampleDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void GetGuideTopic(const char *&page, const char *&section);

  // The instrument that got the new sample (-1 if none)
  int GetInstrument() { return instrument_; }

protected:
  virtual void drawGraphics();

private:
  int framesFor(int bars);
  void finish();
  void cancel();

  int mode_;
  int bars_;
  std::string file_;
  bool started_;
  bool done_;
  bool cancelled_;
  int instrument_;
  std::string error_;
};

#endif
