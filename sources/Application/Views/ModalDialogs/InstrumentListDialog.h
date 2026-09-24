#ifndef _INSTRUMENT_LIST_DIALOG_H_
#define _INSTRUMENT_LIST_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Model/Song.h"
#include <string>

// Every instrument at a glance: number, type, name, how many phrases use
// it, what is sounding right now, and a picture of the selected sound.
// A opens it, Start hears it, Select renames it, LB+A duplicates it.
class InstrumentListDialog : public ModalView {
public:
  InstrumentListDialog(View &view, int current);
  virtual ~InstrumentListDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void GetGuideTopic(const char *&page, const char *&section);
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);

  int GetSelection() { return selected_; }
  void Rename(const std::string &name);

protected:
  virtual void drawGraphics();

private:
  void countUsage();
  void move(int delta);
  void audition();
  void duplicate();
  void cachePreview();
  bool playing(int instrument);

  int selected_;
  std::string status_; // one-off message on the info line
  int top_;
  int usage_[MAX_INSTRUMENT_COUNT];
  // Preview of the selected sound, one min/max pair per column
  int previewFor_;
  signed char previewMin_[200];
  signed char previewMax_[200];
  int previewColumns_;
  unsigned int activeMask_[(MAX_INSTRUMENT_COUNT + 31) / 32];
};

#endif
