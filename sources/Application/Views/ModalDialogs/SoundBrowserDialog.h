#ifndef _SOUND_BROWSER_DIALOG_H_
#define _SOUND_BROWSER_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Externals/TinyXML/tinyxml.h"
#include "System/FileSystem/FileSystem.h"
#include <string>
#include <vector>

// Every sound you can put in a slot, in one place: your saved sounds, the
// synth presets of each engine, the macro synth's, and the sample packs.
// Moving onto one plays it (a preset or saved synth sound goes in the slot
// to be heard, a sample streams from the card); A takes it, B puts the
// slot back as it was and goes back one step.
class SoundBrowserDialog : public ModalView {
public:
  // note: the pitch to play sounds at (the Rack's keyboard note)
  SoundBrowserDialog(View &view, int slot, int note);
  virtual ~SoundBrowserDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  // In a category: the slot as it was, back to the categories (a sample
  // folder: up one folder). In the categories: the slot as it was, closed.
  virtual bool Back();
  virtual void GetGuideTopic(const char *&page, const char *&section);
  virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7);

  // What happened, for the Rack's info line ("" if nothing changed)
  const char *GetResult() { return result_.c_str(); }

private:
  enum Kind {
    SB_CATEGORY, // top level: opens a list
    SB_SOUND,    // a saved sound
    SB_SYNTH,    // a synth preset (index)
    SB_MACRO,    // a macro synth preset (index)
    SB_FOLDER,   // a sample folder
    SB_SAMPLE    // a WAV
  };
  struct Item {
    Kind kind;
    int index;        // preset index; a synth category: its engine
    std::string name; // shown
    std::string path; // folder / WAV
    int category;     // a category: which (Category)
  };
  enum Category { SC_SOUNDS, SC_SYNTH, SC_MACRO, SC_SAMPLES };

  void showCategories();
  void openCategory(const Item &item);
  void openFolder(const std::string &path);
  void move(int delta);
  void hear();
  void take();
  void restore();
  void stopSound();
  void audition();

  int slot_;
  int note_;
  bool inCategory_;
  Category category_;
  std::string folder_; // the sample folder shown
  std::string sampleRoot_;
  std::vector<Item> items_;
  int selected_;
  int top_;
  int categorySelected_; // where the cursor was among the categories
  // The slot as it was, to put back on B
  TiXmlDocument original_;
  bool triedOn_;
  std::string status_;
  std::string result_;
};

#endif
