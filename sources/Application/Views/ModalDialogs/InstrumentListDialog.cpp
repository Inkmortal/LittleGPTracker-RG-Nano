#include "InstrumentListDialog.h"
#include "NewProjectDialog.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Model/Phrase.h"
#include "Application/Player/Player.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include "System/Console/Trace.h"
#include <stdio.h>
#include <string.h>

#define LIST_WIDTH 26
#define LIST_HEIGHT 26
#define LIST_Y 2
#define LIST_ROWS 12
#define INFO_Y 15
#define PREVIEW_Y 16
#define PREVIEW_ROWS 6
#define HINT_Y 23

static void renameCallback(View &v, ModalView &dialog) {
    if (dialog.GetReturnCode() > 0) {
        NewProjectDialog &npd = (NewProjectDialog &)dialog;
        ((InstrumentListDialog &)v).Rename(npd.GetTypedName());
    }
}

InstrumentListDialog::InstrumentListDialog(View &view, int current)
    : ModalView(view), selected_(current), top_(0), previewFor_(-1),
      previewColumns_(0) {
    if (selected_ < 0 || selected_ >= MAX_INSTRUMENT_COUNT)
        selected_ = 0;
    memset(activeMask_, 0, sizeof(activeMask_));
    countUsage();
}

InstrumentListDialog::~InstrumentListDialog() {}

// Phrases that use each instrument (a phrase counts once)
void InstrumentListDialog::countUsage() {
    memset(usage_, 0, sizeof(usage_));
    Phrase *phrase = viewData_->song_->phrase_;
    for (int p = 0; p < PHRASE_COUNT; p++) {
        bool seen[MAX_INSTRUMENT_COUNT];
        memset(seen, 0, sizeof(seen));
        for (int s = 0; s < 16; s++) {
            unsigned char i = phrase->instr_[16 * p + s];
            if (i < MAX_INSTRUMENT_COUNT && !seen[i]) {
                seen[i] = true;
                usage_[i]++;
            }
        }
    }
}

bool InstrumentListDialog::playing(int instrument) {
    return (activeMask_[instrument / 32] >> (instrument % 32)) & 1;
}

void InstrumentListDialog::OnFocus() {
    top_ = selected_ - LIST_ROWS / 2;
    move(0);
}

void InstrumentListDialog::move(int delta) {
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
    isDirty_ = true;
}

static const char *typeTag(I_Instrument *instr) {
    switch (instr->GetType()) {
    case IT_SYNTH:
        return "SYN";
    case IT_MIDI:
        return "MID";
    default:
        return instr->IsEmpty() ? "---" : "SMP";
    }
}

void InstrumentListDialog::DrawView() {
    SetWindow(LIST_WIDTH, LIST_HEIGHT);
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    GUITextProperties props;
    char line[40];

    SetColor(CD_HILITE2);
    DrawString(0, 0, "INSTRUMENTS", props);
    sprintf(line, "%02X/%02X", selected_, MAX_INSTRUMENT_COUNT - 1);
    SetColor(CD_MUTE);
    DrawString(LIST_WIDTH - 5, 0, line, props);

    for (int r = 0; r < LIST_ROWS; r++) {
        int i = top_ + r;
        I_Instrument *instr = bank->GetInstrument(i);
        bool empty = instr->GetType() == IT_SAMPLE && instr->IsEmpty();
        char name[13];
        strncpy(name, empty ? "" : instr->GetName(), 12);
        name[12] = 0;
        char used[4] = "  ";
        if (usage_[i] > 0)
            sprintf(used, "%2d", usage_[i] > 99 ? 99 : usage_[i]);
        sprintf(line, "%02X %s %-12s %s", i, typeTag(instr), name, used);
        bool on = (i == selected_);
        props.invert_ = on;
        SetColor(on ? CD_CURSOR : (empty ? CD_MUTE : CD_NORMAL));
        DrawString(0, LIST_Y + r, line, props);
    }
    props.invert_ = false;

    // What the selected slot is and where it's used
    I_Instrument *sel = bank->GetInstrument(selected_);
    const char *kind = sel->GetType() == IT_SYNTH  ? "synth"
                       : sel->GetType() == IT_MIDI ? "midi out"
                                                   : "sample";
    if (!status_.empty()) {
        strncpy(line, status_.c_str(), 26);
        line[26] = 0;
    } else if (sel->GetType() == IT_SAMPLE && sel->IsEmpty()) {
        strcpy(line, "empty: A to set it up");
    } else if (usage_[selected_] == 0) {
        sprintf(line, "%s, not used yet", kind);
    } else {
        sprintf(line, "%s, in %d phrase%s", kind, usage_[selected_],
                usage_[selected_] == 1 ? "" : "s");
    }
    SetColor(CD_HILITE1);
    DrawString(0, INFO_Y, "                          ", props);
    DrawString(0, INFO_Y, line, props);

    SetColor(CD_NORMAL);
    DrawString(0, HINT_Y, "A open      START hear", props);
    DrawString(0, HINT_Y + 1, "SEL name    LB+A copy", props);
    SetColor(CD_NORMAL);
}

// Min/max per column for the selected sound: two cycles of a synth, the
// whole recording of a sample
void InstrumentListDialog::cachePreview() {
    previewFor_ = selected_;
    previewColumns_ = (LIST_WIDTH - 2) * 8 - 4;
    if (previewColumns_ > 200)
        previewColumns_ = 200;
    memset(previewMin_, 0, sizeof(previewMin_));
    memset(previewMax_, 0, sizeof(previewMax_));
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(selected_);
    if (instr->GetType() == IT_SYNTH) {
        float cycle[100];
        ((SynthInstrument *)instr)->RenderCycle(cycle, 100);
        int prev = 0;
        for (int c = 0; c < previewColumns_; c++) {
            float v = cycle[(c * 200 / previewColumns_) % 100];
            int y = (int)(v * 100.0f);
            if (y > 100)
                y = 100;
            if (y < -100)
                y = -100;
            if (c == 0)
                prev = y;
            // Join to the previous column so steep edges (square, saw) show
            previewMin_[c] = (signed char)(y < prev ? y : prev);
            previewMax_[c] = (signed char)(y > prev ? y : prev);
            prev = y;
        }
        return;
    }
    if (instr->GetType() != IT_SAMPLE || instr->IsEmpty())
        return;
    int index = ((SampleInstrument *)instr)->GetSampleIndex();
    SamplePool *pool = SamplePool::GetInstance();
    if (index < 0 || index >= pool->GetNameListSize())
        return;
    SoundSource *source = pool->GetSource(index);
    if (!source)
        return;
    int size = source->GetSize(-1);
    int channels = source->GetChannelCount(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || size <= 0)
        return;
    if (channels <= 0)
        channels = 1;
    int peak = 1;
    int lo[200], hi[200];
    for (int c = 0; c < previewColumns_; c++) {
        int start = (int)(((long long)c * size) / previewColumns_);
        int end = (int)(((long long)(c + 1) * size) / previewColumns_);
        if (end <= start)
            end = start + 1;
        if (end > size)
            end = size;
        // Long samples: a strided scan per column is plenty for a picture
        int stride = (end - start) / 64 + 1;
        lo[c] = hi[c] = 0;
        for (int i = start; i < end; i += stride) {
            int s = samples[i * channels];
            if (s < lo[c])
                lo[c] = s;
            if (s > hi[c])
                hi[c] = s;
        }
        if (-lo[c] > peak)
            peak = -lo[c];
        if (hi[c] > peak)
            peak = hi[c];
    }
    for (int c = 0; c < previewColumns_; c++) {
        previewMin_[c] = (signed char)((lo[c] * 100) / peak);
        previewMax_[c] = (signed char)((hi[c] * 100) / peak);
    }
}

void InstrumentListDialog::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    const int ox = windowLeft() * 8;
    const int oy = windowTop() * 8;

    // Sounding right now: a dot after the row
    GUIColor play = AppWindow::ThemeColor(CD_PLAY);
    imp->SetColor(play);
    for (int r = 0; r < LIST_ROWS; r++) {
        if (playing(top_ + r)) {
            GUIRect dot(ox + (LIST_WIDTH - 1) * 8 + 2, oy + (LIST_Y + r) * 8 + 2,
                        ox + (LIST_WIDTH - 1) * 8 + 6, oy + (LIST_Y + r) * 8 + 6);
            imp->DrawRect(dot);
        }
    }

    // Scroll bar at the left edge of the list
    GUIColor track = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 35);
    GUIColor thumb = AppWindow::ThemeColor(CD_HILITE1);
    int barTop = oy + LIST_Y * 8, barH = LIST_ROWS * 8;
    GUIRect bar(ox - 5, barTop, ox - 3, barTop + barH);
    imp->SetColor(track);
    imp->DrawRect(bar);
    int t0 = barTop + (top_ * barH) / MAX_INSTRUMENT_COUNT;
    int t1 = barTop + ((top_ + LIST_ROWS) * barH) / MAX_INSTRUMENT_COUNT;
    GUIRect th(ox - 5, t0, ox - 3, t1);
    imp->SetColor(thumb);
    imp->DrawRect(th);

    // Preview box
    if (previewFor_ != selected_)
        cachePreview();
    int bx = ox, by = oy + PREVIEW_Y * 8 + 2;
    int bw = (LIST_WIDTH - 1) * 8, bh = PREVIEW_ROWS * 8 - 4;
    GUIColor frame = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 45);
    GUIColor panel = AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor trace = AppWindow::ThemeColor(CD_HILITE2);
    GUIRect outer(bx, by, bx + bw, by + bh);
    imp->SetColor(frame);
    imp->DrawRect(outer);
    GUIRect inner(bx + 1, by + 1, bx + bw - 1, by + bh - 1);
    imp->SetColor(panel);
    imp->DrawRect(inner);
    int mid = by + bh / 2;
    int half = bh / 2 - 3;
    imp->SetColor(trace);
    for (int c = 0; c < previewColumns_ && c < bw - 4; c++) {
        int y0 = mid - (previewMax_[c] * half) / 100;
        int y1 = mid - (previewMin_[c] * half) / 100;
        if (y1 < y0) {
            int t = y0;
            y0 = y1;
            y1 = t;
        }
        GUIRect col(bx + 2 + c, y0, bx + 3 + c, y1 + 1);
        imp->DrawRect(col);
    }
#endif
}

void InstrumentListDialog::OnPlayerUpdate(PlayerEventType,
                                          unsigned int currentTick) {
    unsigned int mask[(MAX_INSTRUMENT_COUNT + 31) / 32];
    memset(mask, 0, sizeof(mask));
    Player *player = Player::GetInstance();
    for (int ch = 0; ch < SONG_CHANNEL_COUNT; ch++) {
        if (!player->IsRunning() || !player->IsChannelPlaying(ch))
            continue;
        int i = player->GetChannelInstrumentIndex(ch);
        if (i >= 0 && i < MAX_INSTRUMENT_COUNT)
            mask[i / 32] |= 1u << (i % 32);
    }
    if (memcmp(mask, activeMask_, sizeof(mask)) != 0) {
        memcpy(activeMask_, mask, sizeof(mask));
        isDirty_ = true;
    }
}

void InstrumentListDialog::audition() {
    Player *player = Player::GetInstance();
    if (player->IsRunning()) {
        player->Stop();
        return;
    }
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(selected_);
    int note = 60;
    if (instr->GetType() == IT_SAMPLE) {
        if (instr->IsEmpty())
            return;
        Variable *root = instr->FindVariable(SIP_ROOTNOTE);
        if (root)
            note = root->GetInt();
    }
    player->AuditionInstrument(selected_, note);
}

void InstrumentListDialog::duplicate() {
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    // Messages go on the info line: a notification would sit behind the list
    if (bank->GetInstrument(selected_)->GetType() == IT_MIDI) {
        status_ = "MIDI can't be copied";
        isDirty_ = true;
        return;
    }
    int from = selected_;
    unsigned short next = bank->Clone(selected_);
    Trace::Log("INSTRLIST", "duplicate %02X -> %X", from, next);
    if (next == NO_MORE_INSTRUMENT) {
        status_ = "no free slot to copy to";
        isDirty_ = true;
        return;
    }
    countUsage();
    move((int)next - selected_);
    char msg[32];
    sprintf(msg, "copy of %02X now in %02X", from, next);
    status_ = msg;
}

void InstrumentListDialog::Rename(const std::string &name) {
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(selected_);
    Variable *v = instr->FindVariable(INSTRUMENT_NAME_ID);
    if (!v)
        return;
    v->SetString(name.c_str());
    isDirty_ = true;
}

void InstrumentListDialog::ProcessButtonMask(unsigned short mask,
                                             bool pressed) {
    if (!pressed)
        return;
    if (mask == (EPBM_L | EPBM_A)) {
        duplicate();
        return;
    }
    if (mask == EPBM_UP) {
        move(-1);
    } else if (mask == EPBM_DOWN) {
        move(1);
    } else if (mask == EPBM_LEFT) { // a page
        move(-LIST_ROWS);
    } else if (mask == EPBM_RIGHT) {
        move(LIST_ROWS);
    } else if (mask == EPBM_A) {
        EndModal(1);
    } else if (mask == EPBM_B) {
        EndModal(0);
    } else if (mask == EPBM_START) {
        audition();
    } else if (mask == EPBM_SELECT) {
        I_Instrument *instr =
            viewData_->project_->GetInstrumentBank()->GetInstrument(selected_);
        Variable *v = instr->FindVariable(INSTRUMENT_NAME_ID);
        if (!v) {
            status_ = "MIDI can't be named";
            isDirty_ = true;
            return;
        }
        NewProjectDialog *npd = new NewProjectDialog(*this);
        const char *custom = v->GetString();
        npd->SetRename("NAME THIS SOUND",
                       (custom && custom[0]) ? custom : "");
        DoModal(npd, renameCallback);
    }
}

void InstrumentListDialog::GetGuideTopic(const char *&page,
                                         const char *&section) {
    page = "screens";
    section = "Instrument list";
}

void InstrumentListDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
    name = "INSTRUMENTS";
    where = "RB+Up on Instrument";
    edit = "B back";
    field = "Every sound at a glance";
    cmd1 = "Up/Down pick, L/R page";
    cmd2 = "A open that instrument";
    cmd3 = "START hear it, again stop";
    cmd4 = "SEL give it a name";
    cmd5 = "LB+A copy to a free slot";
    cmd6 = "number = phrases using it";
    cmd7 = "green dot = playing now";
}
