#ifndef _NEW_PROJECT_DIALOG_H_
#define _NEW_PROJECT_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>

#define MAX_NAME_LENGTH 12

// D-pad name editor: an on-screen keyboard of spaced key caps in
// alphabetical order plus an action row. Opens with a free random name and
// DONE selected, so A creates a song straight away.
class NewProjectDialog:public ModalView {
public:
  NewProjectDialog(View &view, Path currentPath = "root:");
  virtual ~NewProjectDialog();

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

  std::string GetName();

protected:
  virtual void drawGraphics();

private:
  bool nameTaken();
  void typeChar(char c);
  void erase();
  void randomName();
  void activate();
  void confirm();

  Path currentPath_;
  std::string name_;
  int cursor_;   // insert position in name_
  int row_;      // grid row, the last row is the action row
  int col_;
  bool lower_;     // letter grid types lowercase
  bool suggested_; // name is an untouched random pick; typing replaces it
};
#endif
