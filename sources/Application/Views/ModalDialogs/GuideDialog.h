#ifndef _GUIDE_DIALOG_H_
#define _GUIDE_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>
#include <vector>

// The full user guide inside the app, read from bin:guide.txt (generated
// from the wiki by tools/build_ingame_guide.py). Three places, all full
// screen: Contents (topics, each opens up to its sections), a page, and an
// Index of every key, command and heading. A on a page opens "Go to" with
// the links on screen. B steps back through where you have been; RB+Select
// (the combo that opened it from the helper) closes the guide from
// anywhere. Opens on the contents, or straight at a page section.
class GuideDialog:public ModalView {
public:
  GuideDialog(View &view, const char *page = 0, const char *section = 0);
  virtual ~GuideDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual bool HandlesHelperCombo() { return true; }
  // The guide walks B back through its own pages in ProcessButtonMask
  virtual bool Back() { return false; }
  virtual void GetGuideTopic(const char *&page, const char *&section) {
    page = 0;
    section = 0;
  }

  struct Link {
    int line;             // index in Page::lines
    int col, len;         // characters of that line
    std::string page;     // page id
    std::string section;  // heading, "" = top of the page
  };
  struct Page {
    std::string id;
    std::string title;
    std::vector<std::string> lines;   // first char = kind, see the tool
    std::vector<int> sections;        // line index of each '='/'+' heading
    std::vector<Link> links;
  };
  struct IndexEntry {
    std::string label;
    int page;
    int line;
  };

protected:
  virtual void drawGraphics();

private:
  enum Mode { MODE_CONTENTS, MODE_PAGE, MODE_INDEX };
  struct Place {
    Mode mode;
    int page;
    int top;
    int selection;
  };
  struct Row {             // one line of the contents
    int page;              // -1: the index
    int line;              // -1: the page itself, else its heading
    std::string label;
  };
  struct Target {          // one choice in Go to
    std::string label;
    int page;              // -1: contents, -2: index
    int line;
  };

  void paint();
  void paintContents();
  void paintPage();
  void paintIndex();
  void paintGoTo();
  void paintTitle(const char *left, const char *right);
  void paintFooter(const char *line1, const char *line2);
  void paintList(const std::vector<std::string> &labels,
                 const std::vector<std::string> &notes,
                 const std::vector<int> &indents, int selection, int &top);
  void paintLine(const Page &p, int index, int y);
  void paintExample(const char *text, int x, int y);

  void fill(int x, int y, int w, int h, int color, int blendTo = -1,
            int percent = 0);
  // note: the simulator's screen checks see it (not for pieces of a line)
  void text(int x, int y, const char *s, int color, bool note = true);
  void hint(int x, int y, const char *s);

  void layoutPage();
  int lineHeight(const Page &p, int index) const;
  int visibleEnd() const;   // first line below the screen
  void scrollTo(int line);
  int sectionAt(int line);
  int findSection(int page, const std::string &section);

  void openPage(int page, int line);
  void go(Mode mode, int page, int line);  // remembers where we were
  void back();
  void buildRows();
  void openGoTo();

  std::string wantedPage_;
  std::string wantedSection_;
  Mode mode_;
  int page_;        // open page, or the page picked in the contents
  int top_;         // first line on screen while reading
  int selection_;   // contents row / index entry
  int listTop_;
  int expanded_;    // page whose sections the contents shows, -1 none
  std::vector<Row> rows_;
  std::vector<int> lineY_;   // each line's y in the page, plus the total
  std::vector<Place> history_;
  bool goTo_;
  int goToSelection_;
  std::vector<Target> targets_;
};
#endif
