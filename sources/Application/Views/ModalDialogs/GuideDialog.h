#ifndef _GUIDE_DIALOG_H_
#define _GUIDE_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>
#include <vector>

// The full user guide inside the app, read from bin:guide.txt (generated
// from the wiki by tools/build_ingame_guide.py). Opens on the topic list, or
// straight at a page section (A in the RB+Select helper).
class GuideDialog:public ModalView {
public:
  GuideDialog(View &view, const char *page = 0, const char *section = 0);
  virtual ~GuideDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void GetGuideTopic(const char *&page, const char *&section) {
    page = 0;
    section = 0;
  }
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);

  struct Page {
    std::string id;
    std::string title;
    std::vector<std::string> lines;   // first char = kind, see the tool
    std::vector<int> sections;        // line index of each '=' heading
  };

private:
  void openPage(int page, int line);
  void scrollTo(int line);
  int sectionAt(int line);

  std::string wantedPage_;
  std::string wantedSection_;
  bool reading_;
  int page_;      // selected / open page
  int listTop_;
  int top_;       // first visible line while reading
};
#endif
