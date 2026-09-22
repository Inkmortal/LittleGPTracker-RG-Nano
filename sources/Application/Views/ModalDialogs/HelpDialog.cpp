#include "HelpDialog.h"
#include <stdio.h>
#include <string.h>

#define HELP_WIDTH 26
#define HELP_HEIGHT 21
#define HELP_LINES 14

struct HelpPage {
    const char *title;
    const char *lines[HELP_LINES];
};

// Lines are at most 24 chars. A leading '*' draws the line as a heading.
static const HelpPage helpPages[] = {
    {"WHAT IS THIS",
     {"A tracker: music written",
      "as rows that play from",
      "top to bottom.",
      "",
      "*SONG    the whole tune",
      "*CHAIN   bars in a row",
      "*PHRASE  1 bar, 16 steps",
      "*INSTR   the sound",
      "",
      "A song lists chains, a",
      "chain lists phrases, a",
      "phrase holds the notes.",
      "New songs come with 16",
      "ready sounds."}},
    {"BUTTONS",
     {"*Dpad      move",
      "*A         place / type",
      "*A+Dpad    change value",
      "*B+A       delete",
      "*RB+Dpad   other screen",
      "*START     play / stop",
      "*LB+L/R    next page",
      "*MENU      quit app",
      "",
      "Lost? Hold RB and press",
      "SELECT (the FN key) on",
      "any screen for a map and",
      "the buttons for it."}},
    {"THE MAP",
     {"Hold RB + a direction to",
      "walk the map:",
      "",
      "*PROJ       GROOVE",
      "*  |          |",
      "*SONG>CHAIN>PHRASE>INSTR",
      "*  |          |",
      "*MIXER      TABLE",
      "",
      "RB+Right goes deeper,",
      "RB+Left comes back.",
      "PROJ: tempo, key, save.",
      "The RB+SELECT helper",
      "draws this map too."}},
    {"FIRST BEAT",
     {"1 New, then A on OK",
      "2 Song: A on the --",
      "3 RB+Right, then A",
      "4 RB+Right: the phrase",
      "5 A on rows 00 04 08 0C",
      "6 START: four kicks!",
      "",
      "A on empty: reuse last",
      "A again: a new one",
      "",
      "",
      "I column = which sound:",
      "00 kick 01 snare 02 hat",
      "05 bass 07 pad 06 lead"}},
    {"KEEP GOING",
     {"Open a demo song and",
      "change notes, sounds,",
      "tempo. Poke everything.",
      "",
      "Save: RB+Up to PROJ,",
      "then Save Song.",
      "",
      "Every screen has help:",
      "RB+SELECT, Down flips.",
      "",
      "*Full guide online:",
      "inkmortal.github.io/",
      "LittleGPTracker-RG-Nano-",
      "Audio-In-Sampling"}},
};

static const int helpPageCount = sizeof(helpPages) / sizeof(helpPages[0]);

HelpDialog::HelpDialog(View &view) : ModalView(view), page_(0) {}

HelpDialog::~HelpDialog() {}

void HelpDialog::DrawView() {
    SetWindow(HELP_WIDTH, HELP_HEIGHT);
    GUITextProperties props;
    const HelpPage &page = helpPages[page_];

    char title[40];
    sprintf(title, "HELP %d/%d  %s", page_ + 1, helpPageCount, page.title);
    SetColor(CD_HILITE1);
    DrawString(1, 0, title, props);

    for (int i = 0; i < HELP_LINES; i++) {
        const char *line = page.lines[i];
        if (!line || !*line)
            continue;
        if (line[0] == '*') {
            SetColor(CD_HILITE2);
            line++;
        } else {
            SetColor(CD_NORMAL);
        }
        DrawString(1, 2 + i, line, props);
    }

    SetColor(CD_MUTE);
    DrawString(1, HELP_HEIGHT - 2,
               page_ + 1 < helpPageCount ? "<> or A: next   B: close"
                                         : "<> pages        B: close",
               props);
    SetColor(CD_NORMAL);
}

void HelpDialog::OnPlayerUpdate(PlayerEventType, unsigned int currentTick) {}

void HelpDialog::OnFocus() { page_ = 0; }

void HelpDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
    name = "HELP";
    where = "Start screen guide";
    edit = "B closes";
    field = "Learn the basics";
    cmd1 = "Left/Right page";
    cmd2 = "A next page";
    cmd3 = "B close";
    cmd4 = "START close";
    cmd5 = "";
    cmd6 = "";
    cmd7 = "RB+Select helper";
}

void HelpDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    switch (mask) {
    case EPBM_RIGHT:
    case EPBM_A:
        if (page_ + 1 < helpPageCount) {
            page_++;
        } else if (mask == EPBM_A) {
            EndModal(0);
            return;
        }
        break;
    case EPBM_LEFT:
        if (page_ > 0)
            page_--;
        break;
    case EPBM_B:
    case EPBM_START:
        EndModal(0);
        return;
    default:
        return;
    }
    isDirty_ = true;
}
