#ifndef _SOUND_FILES_DIALOG_H_
#define _SOUND_FILES_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>
#include <vector>

// Your sound library from inside a song: save the selected sound or the
// whole kit, load one back, and choose the kit new songs start with. The
// files live outside the song (SoundLibrary), so they carry over.
// Return code 1: the song's sounds changed (the opener refreshes).
class SoundFilesDialog : public ModalView {
public:
  // slot: the sound "save/load sound" works on
  SoundFilesDialog(View &view, int slot);
  virtual ~SoundFilesDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  // In a list: back to the menu. In the menu: close.
  virtual bool Back();
  virtual void GetGuideTopic(const char *&page, const char *&section);
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);

  // Called back by the name and question dialogs
  void Named(const std::string &name);
  void Answered(bool yes);

private:
  enum Mode { SF_MENU, SF_SOUNDS, SF_KITS };
  enum Action {
    SFA_SAVE_SOUND,
    SFA_LOAD_SOUND,
    SFA_SAVE_KIT,
    SFA_LOAD_KIT,
    SFA_SET_TEMPLATE,
    SFA_CLEAR_TEMPLATE
  };
  void buildMenu();
  void activate();
  void openList(Mode mode);
  void askName(const char *title, const std::string &current);
  void ask(const char *question);
  void saveSound(const std::string &name);
  void saveKit(const std::string &name);
  void loadSound(const std::string &name);
  void loadKit(const std::string &name);
  void report(bool ok, const std::string &done, const std::string &error);
  std::string songName();

  int slot_;
  Mode mode_;
  std::vector<int> menu_;
  std::vector<std::string> names_;
  int selected_;
  int top_;
  Action pending_;
  std::string pendingName_;
  std::string status_;
  bool changed_;
};

#endif
