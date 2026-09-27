#ifndef _RACK_VIEW_H_
#define _RACK_VIEW_H_

#include "BaseClasses/View.h"
#include "SoundPreview.h"
#include <string>

// The Rack: every sound of the song on one screen, to build and play your
// instruments before writing a note. Song RB+Left opens it.
//   Up/Down pick a sound, Left/Right a page
//   A (hold) plays it at the keyboard note; A+Left/Right a scale step,
//   A+Up/Down an octave
//   Select: the sound browser (presets, samples, your saved sounds) that
//   plays each one as you move
//   RB+Right edits it on the Instrument screen (RB+Left there comes back)
//   LB+A copies it to a free slot; B back to the Song
class RackView : public View {
public:
  RackView(GUIWindow &w, ViewData *viewData);
  virtual ~RackView();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void GetGuideTopic(const char *&page, const char *&section);

  // The browser took a sound (or put the old one back)
  void SoundChanged(const char *message);
  // The note the Rack's keyboard plays the selected sound at (MIDI)
  int PlayNote();

  int GetSelection() { return selected_; }

protected:
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void drawGraphics();
  // B: back to the Song (the Rack is where you went from there)
  virtual bool Back();
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);
  virtual void CustomizeHowToSteps(const char **lines);

private:
  void move(int delta);
  void play();
  void stop();
  void stepNote(int direction);
  void octave(int direction);
  void duplicate();
  void switchTo(ViewType type);
  int baseNote();

  int selected_;
  int top_;
  int noteOffset_; // semitones from the sound's base note (synth C3, sample root)
  bool holding_;   // A is down: its note is sounding
  std::string status_;
  int usage_[MAX_INSTRUMENT_COUNT];
  SoundPreview preview_;
};

#endif
