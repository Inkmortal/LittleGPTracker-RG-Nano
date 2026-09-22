#ifndef _HELP_DIALOG_H_
#define _HELP_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"

// Start-screen guide: a few short pages that teach the app without the
// website. Left/Right (or A) flips pages, B closes.
class HelpDialog:public ModalView {
public:
  HelpDialog(View &view);
  virtual ~HelpDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);

private:
  int page_;
};
#endif
