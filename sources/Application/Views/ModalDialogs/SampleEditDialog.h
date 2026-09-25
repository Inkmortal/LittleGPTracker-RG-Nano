#ifndef _SAMPLE_EDIT_DIALOG_H_
#define _SAMPLE_EDIT_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Instruments/SampleProcessor.h"
#include <string>

// The M8's sample editor "processes" for the current sample instrument:
// normalize, crop, fades, reverse, trim silence, on the S..E range.
// A picture of before and after; A writes the result as a new WAV next to
// the original and puts it on the instrument (B+Select undoes that, the
// original file stays). Select on the SOURCE/LOOP pages opens it.
class SampleEditDialog : public ModalView {
public:
  SampleEditDialog(View &view, int instrument);
  virtual ~SampleEditDialog();

  virtual void DrawView();
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void GetGuideTopic(const char *&page, const char *&section);
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);

protected:
  virtual void drawGraphics();

private:
  class SampleInstrument *instrument();
  // Read the sample and markers, plan the selected edit, redraw pictures
  void plan();
  void apply();
  void audition();
  void drawWave(int x, int y, int w, int h, const short *lo, const short *hi,
                int shadeFrom, int shadeTo);

  int instrument_;
  int op_;
  bool haveSample_;
  const char *problem_; // why there's nothing to edit
  SampleEdit edit_;
  int sampleIndex_;
  int start_, loop_, end_;
  std::string status_;
  std::string made_; // the file the last edit made
  bool statusGood_;
  // Pictures, one min/max pair per column, both drawn to the scale of
  // the louder one (so normalize shows as a taller wave)
  short beforeLo_[200], beforeHi_[200];
  short afterLo_[200], afterHi_[200];
  int columns_;
  int pictureScale_;
};

#endif
