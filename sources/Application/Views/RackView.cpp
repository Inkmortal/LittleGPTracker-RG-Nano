#include "RackView.h"
#include "KeyboardStrip.h"
#include "ModalDialogs/SoundBrowserDialog.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Model/Project.h"
#include "Application/Model/Scale.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "System/Console/Trace.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string.h>

// Text rows (8 px each)
// (row 3, top right, is the play label: STOP, AUDITION)
#define LIST_Y 4
#define LIST_ROWS 11
#define INFO_Y 15
#define PREVIEW_Y 16
#define PREVIEW_ROWS 4
#define NOTE_Y 20
#define KEYS_Y 21
#define HINT_Y 26
#define ROW_WIDTH 28
// Keyboard picture: 7 white keys 30 px wide
#define KEYS_X 15
#define KEYS_WHITE_W 30
#define KEYS_H 30
#define KEYS_BLACK_W 18
#define KEYS_BLACK_H 18

// "C3", "F#4": the note as a musician reads it
static void noteLabel(int note, char *out) {
    const char *name = getNoteName(note % 12);
    int n = 0;
    for (int i = 0; i < 2 && name[i] && name[i] != ' ' && name[i] != '-'; i++)
        out[n++] = name[i];
    sprintf(out + n, "%d", note / 12 - 2);
}

static void browserCallback(View &v, ModalView &dialog) {
    SoundBrowserDialog &browser = (SoundBrowserDialog &)dialog;
    ((RackView &)v).SoundChanged(browser.GetResult());
}

RackView::RackView(GUIWindow &w, ViewData *viewData)
    : View(w, viewData), selected_(0), top_(0), noteOffset_(0),
      holding_(false) {
    viewType_ = VT_RACK;
    memset(usage_, 0, sizeof(usage_));
}

RackView::~RackView() {}

void RackView::OnFocus() {
    selected_ = viewData_->currentInstrument_;
    if (selected_ < 0 || selected_ >= MAX_INSTRUMENT_COUNT)
        selected_ = 0;
    top_ = selected_ - LIST_ROWS / 2;
    CountInstrumentUsage(viewData_->song_, usage_);
    // Back from editing: the sound may look different now
    preview_.Invalidate();
    holding_ = false;
    status_.clear();
    move(0);
}

void RackView::move(int delta) {
    if (delta != 0)
        status_.clear();
    selected_ += delta;
    if (selected_ < 0)
        selected_ = 0;
    if (selected_ >= MAX_INSTRUMENT_COUNT)
        selected_ = MAX_INSTRUMENT_COUNT - 1;
    if (selected_ < top_)
        top_ = selected_;
    if (selected_ >= top_ + LIST_ROWS)
        top_ = selected_ - LIST_ROWS + 1;
    if (top_ > MAX_INSTRUMENT_COUNT - LIST_ROWS)
        top_ = MAX_INSTRUMENT_COUNT - LIST_ROWS;
    if (top_ < 0)
        top_ = 0;
    // Picking a sound here is picking it for the next note you write
    viewData_->currentInstrument_ = selected_;
    viewData_->instrumentPicked_ = true;
    isDirty_ = true;
}

// Where the keyboard starts for this sound: a synth at C3, a sample at the
// note it was recorded at (its root), so the middle of the keyboard sounds
// natural
int RackView::baseNote() {
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(selected_);
    if (instr->GetType() == IT_SAMPLE) {
        Variable *root = instr->FindVariable(SIP_ROOTNOTE);
        if (root)
            return root->GetInt();
    }
    return 60;
}

int RackView::PlayNote() {
    int note = baseNote() + noteOffset_;
    if (note < 0)
        note = 0;
    if (note > 127)
        note = 127;
    return note;
}

void RackView::play() {
    Player *player = Player::GetInstance();
    // Never cut the song short for a preview
    if (player->IsRunning() && viewData_->playMode_ != PM_AUDITION) {
        status_ = "song playing: Start stops it";
        isDirty_ = true;
        return;
    }
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(selected_);
    if (instr->GetType() == IT_SAMPLE && instr->IsEmpty()) {
        status_ = "empty: Select to pick a sound";
        isDirty_ = true;
        return;
    }
    player->AuditionInstrument(selected_, PlayNote());
    holding_ = true;
    isDirty_ = true;
}

void RackView::stop() {
    if (!holding_)
        return;
    holding_ = false;
    if (viewData_->playMode_ == PM_AUDITION)
        Player::GetInstance()->Stop();
    isDirty_ = true;
}

// The next note of the song's scale that way (every note without a key)
void RackView::stepNote(int direction) {
    Project *project = viewData_->project_;
    int base = baseNote();
    int offset = noteOffset_;
    for (int tries = 0; tries < 12; tries++) {
        offset += direction;
        int note = base + offset;
        if (note < 0 || note > 127)
            return;
        if (project->IsNoteInScale(note))
            break;
    }
    noteOffset_ = offset;
    play();
}

void RackView::octave(int direction) {
    int note = baseNote() + noteOffset_ + 12 * direction;
    if (note < 0 || note > 127)
        return;
    noteOffset_ += 12 * direction;
    play();
}

void RackView::duplicate() {
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (bank->GetInstrument(selected_)->GetType() == IT_MIDI) {
        status_ = "MIDI can't be copied";
        isDirty_ = true;
        return;
    }
    int from = selected_;
    unsigned short next = bank->Clone(selected_);
    if (next == NO_MORE_INSTRUMENT) {
        status_ = "no free slot to copy to";
        isDirty_ = true;
        return;
    }
    CountInstrumentUsage(viewData_->song_, usage_);
    move((int)next - selected_);
    char msg[32];
    sprintf(msg, "copy of %02X now in %02X", from, next);
    status_ = msg;
}

void RackView::switchTo(ViewType type) {
    stop();
    if (type == VT_INSTRUMENT) {
        viewData_->currentInstrument_ = selected_;
        // Its RB+Left comes back here instead of going to a phrase
        viewData_->instrumentFromRack_ = true;
    }
    ViewType vt = type;
    ViewEvent ve(VET_SWITCH_VIEW, &vt);
    SetChanged();
    NotifyObservers(&ve);
}

bool RackView::Back() {
    switchTo(VT_SONG);
    return true;
}

void RackView::SoundChanged(const char *message) {
    CountInstrumentUsage(viewData_->song_, usage_);
    preview_.Invalidate();
    status_ = message ? message : "";
    viewData_->currentInstrument_ = selected_;
    viewData_->instrumentPicked_ = true;
    isDirty_ = true;
}

void RackView::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) {
        // Let go of A and the note stops
        if (!(mask & EPBM_A))
            stop();
        return;
    }
    if (mask & EPBM_A) {
        if (mask == EPBM_A) {
            play();
        } else if (mask == (EPBM_A | EPBM_LEFT)) {
            stepNote(-1);
        } else if (mask == (EPBM_A | EPBM_RIGHT)) {
            stepNote(1);
        } else if (mask == (EPBM_A | EPBM_UP)) {
            octave(1);
        } else if (mask == (EPBM_A | EPBM_DOWN)) {
            octave(-1);
        } else if (mask == (EPBM_A | EPBM_L)) {
            duplicate();
        }
        return;
    }
    if (mask & EPBM_R) {
        if (mask == (EPBM_R | EPBM_RIGHT)) {
            switchTo(VT_INSTRUMENT);
        } else if (mask == (EPBM_R | EPBM_LEFT)) {
            status_ = "B: back to the Song";
            isDirty_ = true;
        }
        return;
    }
    switch (mask) {
    case EPBM_UP:
        move(-1);
        break;
    case EPBM_DOWN:
        move(1);
        break;
    case EPBM_LEFT:
        move(-LIST_ROWS);
        break;
    case EPBM_RIGHT:
        move(LIST_ROWS);
        break;
    case EPBM_SELECT:
        if (selected_ >= MAX_SAMPLEINSTRUMENT_COUNT) {
            status_ = "MIDI: edit it with RB+Right";
            isDirty_ = true;
            break;
        }
        stop();
        DoModal(new SoundBrowserDialog(*this, selected_, PlayNote()),
                browserCallback);
        break;
    default:
        break;
    }
}

void RackView::DrawView() {
    Clear();
    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    char line[48];

    SetColor(CD_NORMAL);
    DrawString(pos._x, pos._y, "Rack", props);
    SetColor(CD_MUTE);
    DrawString(pos._x + 5, pos._y, "your sounds", props);
    sprintf(line, "%02X/%02X", selected_, MAX_INSTRUMENT_COUNT - 1);
    DrawString(pos._x + ROW_WIDTH - 5, pos._y, line, props);

    SetColor(CD_MUTE);
    DrawString(pos._x, LIST_Y - 1, "   kind name", props);
    for (int r = 0; r < LIST_ROWS; r++) {
        int i = top_ + r;
        I_Instrument *instr = bank->GetInstrument(i);
        bool empty = instr->GetType() == IT_SAMPLE && instr->IsEmpty();
        char name[15];
        strncpy(name, empty ? "" : instr->GetName(), 14);
        name[14] = 0;
        char used[4] = "  ";
        if (usage_[i] > 0)
            sprintf(used, "%2d", usage_[i] > 99 ? 99 : usage_[i]);
        sprintf(line, "%02X %s %-14s %s", i, InstrumentTypeTag(instr), name,
                used);
        bool on = (i == selected_);
        props.invert_ = on;
        SetColor(on ? CD_CURSOR : (empty ? CD_MUTE : CD_NORMAL));
        DrawString(pos._x, LIST_Y + r, line, props);
    }
    props.invert_ = false;

    // What the selected slot is, or the last thing that happened
    I_Instrument *sel = bank->GetInstrument(selected_);
    if (!status_.empty()) {
        strncpy(line, status_.c_str(), ROW_WIDTH);
        line[ROW_WIDTH] = 0;
    } else if (sel->GetType() == IT_SAMPLE && sel->IsEmpty()) {
        strcpy(line, "empty: Select picks a sound");
    } else if (usage_[selected_] == 0) {
        strcpy(line, "not in a phrase yet");
    } else {
        sprintf(line, "in %d phrase%s", usage_[selected_],
                usage_[selected_] == 1 ? "" : "s");
    }
    SetColor(CD_HILITE1);
    DrawString(pos._x, INFO_Y, line, props);

    // The keyboard's note, and the song's scale it steps in
    char note[8];
    noteLabel(PlayNote(), note);
    Project *project = viewData_->project_;
    int key = project->GetScaleKey();
    SetColor(holding_ ? CD_CURSOR : CD_NORMAL);
    sprintf(line, "note %s", note);
    DrawString(pos._x, NOTE_Y, line, props);
    SetColor(CD_MUTE);
    DrawString(pos._x + 9, NOTE_Y,
               key < 0 ? "every note" : "steps in the scale", props);

    SetColor(CD_NORMAL);
    DrawString(pos._x, HINT_Y, "A play A+L/R note A+U/D oct", props);
    DrawString(pos._x, HINT_Y + 1, "SEL browse RB+R edit B back", props);
    SetColor(CD_NORMAL);
    View::EnableNotification();
}

void RackView::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    GUIPoint pos = GetTitlePosition();
    int ox = pos._x * 8;
    preview_.Draw(imp, viewData_->project_->GetInstrumentBank(), selected_, ox,
                  PREVIEW_Y * 8 + 2, ROW_WIDTH * 8, PREVIEW_ROWS * 8 - 4);
    Project *project = viewData_->project_;
    KeyboardStrip keys = {KEYS_X, KEYS_Y * 8,       KEYS_WHITE_W,
                          KEYS_H, KEYS_BLACK_W,     KEYS_BLACK_H};
    DrawKeyboardStrip(imp, keys, project->GetScaleKey(),
                      scaleMask(project->GetScale(),
                                project->GetScaleCustomMask()),
                      PlayNote() % 12);
#endif
}

void RackView::OnPlayerUpdate(PlayerEventType type, unsigned int currentTick) {
    View::OnPlayerUpdate(type, currentTick);
}

void RackView::GetGuideTopic(const char *&page, const char *&section) {
    page = "sounds-first";
    section = "The Rack";
}

void RackView::CustomizeContextOverlay(const char *&name, const char *&where,
                                       const char *&edit, const char *&field,
                                       const char *&cmd1, const char *&cmd2,
                                       const char *&cmd3, const char *&cmd4,
                                       const char *&cmd5, const char *&cmd6,
                                       const char *&cmd7) {
    name = "RACK";
    where = "Song RB+Left  B back";
    edit = "A play  RB+Right edit";
    field = "Build sounds, then compose";
    cmd1 = "Up/Down pick, L/R page";
    cmd2 = "A hold: play the sound";
    cmd3 = "A+L/R note  A+U/D octave";
    cmd4 = "Select: browse & hear";
    cmd5 = "RB+Right edit the sound";
    cmd6 = "LB+A copy to a free slot";
    cmd7 = "B back to the Song";
}

void RackView::CustomizeHowToSteps(const char **lines) {
    lines[0] = "Rack = all your sounds.";
    lines[1] = "Build them before notes:";
    lines[2] = "1 Select: browse, you hear";
    lines[3] = "  each one as you move";
    lines[4] = "2 A takes it, hold A plays";
    lines[5] = "3 RB+Right to shape it";
    lines[6] = "Then write notes with it";
}
