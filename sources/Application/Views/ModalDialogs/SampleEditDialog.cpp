#include "SampleEditDialog.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Player/Player.h"
#include "System/Console/Trace.h"
#include "System/FileSystem/FileSystem.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string.h>

#define EDIT_WIDTH 26
#define EDIT_HEIGHT 26
#define BEFORE_Y 3   // label row; the picture fills the 4 rows below
#define AFTER_Y 8
#define PICTURE_ROWS 4
#define LIST_Y 14
#define HELP_Y 21
#define STATUS_Y 22
#define HINT_Y 24

SampleEditDialog::SampleEditDialog(View &view, int instrument)
    : ModalView(view), instrument_(instrument), op_(SEO_NORMALIZE),
      haveSample_(false), problem_(""), sampleIndex_(-1), start_(0),
      loop_(0), end_(0), statusGood_(false), columns_(0), pictureScale_(32767) {
    memset(&edit_, 0, sizeof(edit_));
    memset(beforeLo_, 0, sizeof(beforeLo_));
    memset(beforeHi_, 0, sizeof(beforeHi_));
    memset(afterLo_, 0, sizeof(afterLo_));
    memset(afterHi_, 0, sizeof(afterHi_));
}

SampleEditDialog::~SampleEditDialog() {}

SampleInstrument *SampleEditDialog::instrument() {
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(instrument_);
    if (!instr || instr->GetType() != IT_SAMPLE)
        return 0;
    return (SampleInstrument *)instr;
}

void SampleEditDialog::OnFocus() { plan(); }

// Min/max of channel 0 per column; long samples are scanned with a stride
// (plenty for a picture)
template <class F>
static void columnsOf(F value, int frames, int columns, short *lo, short *hi) {
    for (int c = 0; c < columns; c++) {
        int from = (int)(((long long)c * frames) / columns);
        int to = (int)(((long long)(c + 1) * frames) / columns);
        if (to <= from)
            to = from + 1;
        if (to > frames)
            to = frames;
        int stride = (to - from) / 64 + 1;
        int mn = 0, mx = 0;
        for (int i = from; i < to; i += stride) {
            int v = value(i);
            if (v < mn)
                mn = v;
            if (v > mx)
                mx = v;
        }
        lo[c] = (short)mn;
        hi[c] = (short)mx;
    }
}

struct SourceValue {
    const short *src;
    int channels;
    int operator()(int i) const { return src[i * channels]; }
};

struct EditValue {
    const SampleEdit *edit;
    int operator()(int i) const { return SampleProcessor::Value(*edit, i, 0); }
};

void SampleEditDialog::plan() {
    haveSample_ = false;
    problem_ = "no sample on this instrument";
    columns_ = EDIT_WIDTH * 8 - 8;
    if (columns_ > 200)
        columns_ = 200;
    SampleInstrument *instr = instrument();
    if (!instr || instr->IsEmpty()) {
        isDirty_ = true;
        return;
    }
    sampleIndex_ = instr->GetSampleIndex();
    SamplePool *pool = SamplePool::GetInstance();
    if (sampleIndex_ < 0 || sampleIndex_ >= pool->GetNameListSize()) {
        isDirty_ = true;
        return;
    }
    SoundSource *source = pool->GetSource(sampleIndex_);
    if (!source) {
        isDirty_ = true;
        return;
    }
    if (source->IsMulti()) {
        problem_ = "SoundFont sounds can't be edited";
        isDirty_ = true;
        return;
    }
    const short *samples = (const short *)source->GetSampleBuffer(-1);
    int frames = source->GetSize(-1);
    int channels = source->GetChannelCount(-1);
    if (!samples || frames < 2) {
        problem_ = "the sample is empty";
        isDirty_ = true;
        return;
    }
    if (channels < 1)
        channels = 1;
    haveSample_ = true;
    start_ = instr->FindVariable(SIP_START)->GetInt();
    loop_ = instr->FindVariable(SIP_LOOPSTART)->GetInt();
    end_ = instr->FindVariable(SIP_END)->GetInt();
    SampleProcessor::Plan(op_, samples, frames, channels, source->GetSampleRate(-1),
                          start_, loop_, end_, edit_);

    SourceValue before = {samples, channels};
    columnsOf(before, frames, columns_, beforeLo_, beforeHi_);
    if (edit_.ok) {
        EditValue after = {&edit_};
        columnsOf(after, edit_.outFrames, columns_, afterLo_, afterHi_);
    } else {
        memcpy(afterLo_, beforeLo_, sizeof(afterLo_));
        memcpy(afterHi_, beforeHi_, sizeof(afterHi_));
    }
    pictureScale_ = 256;
    for (int c = 0; c < columns_; c++) {
        int m = beforeHi_[c];
        if (-beforeLo_[c] > m) m = -beforeLo_[c];
        if (afterHi_[c] > m) m = afterHi_[c];
        if (-afterLo_[c] > m) m = -afterLo_[c];
        if (m > pictureScale_) pictureScale_ = m;
    }
    isDirty_ = true;
}

void SampleEditDialog::apply() {
    made_.clear();
    if (!haveSample_) {
        status_ = problem_;
        statusGood_ = false;
        return;
    }
    if (!edit_.ok) {
        status_ = edit_.why;
        statusGood_ = false;
        return;
    }
    SamplePool *pool = SamplePool::GetInstance();
    if (pool->GetNameListSize() >= MAX_PIG_SAMPLES) {
        status_ = "the sample list is full";
        statusGood_ = false;
        return;
    }
    std::string name = SampleProcessor::OutputName(pool->GetName(sampleIndex_), op_);
    if (name.empty()) {
        status_ = "no free file name";
        statusGood_ = false;
        return;
    }
    std::string path = std::string("samples:") + name;
    if (!pool->EnsureProjectSampleDir() || !SampleProcessor::Write(edit_, path.c_str())) {
        status_ = "could not write the file";
        statusGood_ = false;
        return;
    }
    int index = pool->AddProjectSample(name.c_str());
    if (index < 0) {
        // Don't leave a file the song doesn't know about
        Path alias(path);
        FileSystem::GetInstance()->Delete(alias.GetPath().c_str());
        status_ = "could not load the result";
        statusGood_ = false;
        return;
    }
    // The audio thread may be starting a note of this instrument
    MixerService *mixer = MixerService::GetInstance();
    mixer->Lock();
    instrument()->ReplaceSample(index, edit_.newStart, edit_.newLoop, edit_.newEnd);
    mixer->Unlock();
    Trace::Log("SAMPLE_EDIT", "%s: %s -> %s frames=%d->%d markers=%d/%d/%d",
               SampleProcessor::Name(op_), pool->GetName(sampleIndex_), name.c_str(),
               edit_.frames, edit_.outFrames, edit_.newStart, edit_.newLoop,
               edit_.newEnd);
    status_ = "made a new file:";
    made_ = name;
    statusGood_ = true;
    // The new sample is now "before": edits chain
    plan();
}

void SampleEditDialog::audition() {
    Player *player = Player::GetInstance();
    if (player->IsRunning() && viewData_->playMode_ == PM_AUDITION) {
        player->Stop();
        return;
    }
    SampleInstrument *instr = instrument();
    if (!instr || instr->IsEmpty())
        return;
    player->AuditionInstrument(instrument_, instr->FindVariable(SIP_ROOTNOTE)->GetInt());
}

void SampleEditDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    if (mask == EPBM_B) {
        EndModal(0);
    } else if (mask == EPBM_UP || mask == EPBM_DOWN) {
        op_ = (op_ + (mask == EPBM_DOWN ? 1 : SEO_COUNT - 1)) % SEO_COUNT;
        status_.clear();
        made_.clear();
        plan();
    } else if (mask == EPBM_A) {
        apply();
        isDirty_ = true;
    } else if (mask == EPBM_START) {
        audition();
        isDirty_ = true;
    }
}

// A file name in the dialog's width: long ones keep their end (the edits'
// suffixes) and lose the start
static void fitName(char *out, const char *name) {
    int len = strlen(name);
    if (len <= EDIT_WIDTH) {
        strcpy(out, name);
        return;
    }
    strcpy(out, "..");
    strcat(out, name + len - (EDIT_WIDTH - 2));
}

static void secondsText(char *out, int frames, int rate) {
    if (rate <= 0)
        rate = 44100;
    sprintf(out, "%.2f", frames / (float)rate);
}

void SampleEditDialog::DrawView() {
    SetWindow(EDIT_WIDTH, EDIT_HEIGHT);
    GUITextProperties props;
    char line[64];

    SetColor(CD_HILITE2);
    DrawString(0, 0, "SAMPLE EDIT", props);
    SetColor(CD_MUTE);
    sprintf(line, "I%02X", instrument_);
    DrawString(EDIT_WIDTH - 3, 0, line, props);

    if (!haveSample_) {
        SetColor(CD_CURSOR);
        DrawString(0, 3, problem_, props);
        SetColor(CD_NORMAL);
        DrawString(0, HINT_Y, "B back", props);
        return;
    }
    SamplePool *pool = SamplePool::GetInstance();
    const char *name = pool->GetName(sampleIndex_);
    fitName(line, name ? name : "");
    SetColor(CD_NORMAL);
    DrawString(0, 1, line, props);
    char a[16], b[16], c[16];
    secondsText(a, edit_.rangeStart, edit_.rate);
    secondsText(b, edit_.rangeEnd, edit_.rate);
    secondsText(c, edit_.frames, edit_.rate);
    if (edit_.op == SEO_TRIM) {
        secondsText(a, start_ < 0 ? 0 : start_, edit_.rate);
        int e = (end_ <= 0 || end_ > edit_.frames) ? edit_.frames : end_;
        secondsText(b, e, edit_.rate);
    }
    sprintf(line, "S..E %s-%s of %s s", a, b, c);
    SetColor(CD_MUTE);
    DrawString(0, 2, line, props);

    SetColor(CD_MUTE);
    DrawString(0, BEFORE_Y, "BEFORE", props);
    DrawString(0, AFTER_Y, "AFTER", props);
    SetColor(edit_.ok ? CD_HILITE1 : CD_MUTE);
    if (!edit_.ok) {
        strcpy(line, "no change");
    } else if (op_ == SEO_NORMALIZE) {
        sprintf(line, "gain x%.2f", edit_.gain);
    } else if (op_ == SEO_CROP || op_ == SEO_TRIM) {
        secondsText(a, edit_.outFrames, edit_.rate);
        sprintf(line, "%s s long", a);
    } else {
        strcpy(line, "same length");
    }
    DrawString(7, AFTER_Y, line, props);

    for (int i = 0; i < SEO_COUNT; i++) {
        bool on = (i == op_);
        props.invert_ = on;
        SetColor(on ? CD_CURSOR : CD_NORMAL);
        sprintf(line, " %-22s ", SampleProcessor::Name(i));
        DrawString(0, LIST_Y + i, line, props);
    }
    props.invert_ = false;

    // What the selected edit does, or why it would change nothing
    SetColor(edit_.ok ? CD_MUTE : CD_CURSOR);
    DrawString(0, HELP_Y, "                          ", props);
    DrawString(0, HELP_Y, edit_.ok ? SampleProcessor::Describe(op_) : edit_.why, props);
    if (!status_.empty()) {
        strncpy(line, status_.c_str(), EDIT_WIDTH);
        line[EDIT_WIDTH] = 0;
        SetColor(statusGood_ ? CD_PLAY : CD_CURSOR);
        DrawString(0, STATUS_Y, line, props);
        if (!made_.empty()) {
            fitName(line, made_.c_str());
            DrawString(0, STATUS_Y + 1, line, props);
        }
    }
    SetColor(CD_NORMAL);
    DrawString(0, HINT_Y, "A apply START hear B back", props);
    SetColor(CD_MUTE);
    DrawString(0, HINT_Y + 1, "new file; B+Sel undoes", props);
    SetColor(CD_NORMAL);
}

void SampleEditDialog::drawWave(int x, int y, int w, int h, const short *lo,
                                const short *hi, int shadeFrom, int shadeTo) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    GUIColor frame = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 45);
    GUIColor panel = AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor shade = AppWindow::ThemeBlend(CD_BACKGROUND, CD_HILITE2, 22);
    GUIColor trace = AppWindow::ThemeColor(CD_HILITE2);
    GUIColor traceIn = AppWindow::ThemeColor(CD_NORMAL);
    GUIRect outer(x, y, x + w, y + h);
    imp->SetColor(frame);
    imp->DrawRect(outer);
    GUIRect inner(x + 1, y + 1, x + w - 1, y + h - 1);
    imp->SetColor(panel);
    imp->DrawRect(inner);
    // The range the edit works on
    if (shadeTo > shadeFrom) {
        GUIRect range(x + 2 + shadeFrom, y + 1, x + 2 + shadeTo, y + h - 1);
        imp->SetColor(shade);
        imp->DrawRect(range);
    }
    int mid = y + h / 2;
    int half = h / 2 - 2;
    for (int c = 0; c < columns_ && c < w - 4; c++) {
        // the louder of before/after fills the height
        int y0 = mid - (hi[c] * half) / pictureScale_;
        int y1 = mid - (lo[c] * half) / pictureScale_;
        if (y1 < y0) {
            int t = y0;
            y0 = y1;
            y1 = t;
        }
        imp->SetColor((c >= shadeFrom && c < shadeTo) ? traceIn : trace);
        GUIRect col(x + 2 + c, y0, x + 3 + c, y1 + 1);
        imp->DrawRect(col);
    }
#endif
}

void SampleEditDialog::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    if (!haveSample_)
        return;
    const int ox = windowLeft() * 8;
    const int oy = windowTop() * 8;
    const int w = columns_ + 4;
    const int h = PICTURE_ROWS * 8 - 2;
    int frames = edit_.frames > 0 ? edit_.frames : 1;
    int from = (int)(((long long)edit_.rangeStart * columns_) / frames);
    // S..E (trim: the part it keeps)
    int to = (int)(((long long)edit_.rangeEnd * columns_) / frames);
    drawWave(ox, oy + (BEFORE_Y + 1) * 8, w, h, beforeLo_, beforeHi_, from, to);
    int afterFrom = 0, afterTo = columns_;
    if (edit_.ok && edit_.op != SEO_CROP && edit_.op != SEO_TRIM) {
        afterFrom = from;
        afterTo = to;
    }
    if (!edit_.ok) {
        afterFrom = afterTo = 0;
    }
    drawWave(ox, oy + (AFTER_Y + 1) * 8, w, h, afterLo_, afterHi_, afterFrom, afterTo);
#endif
}

void SampleEditDialog::GetGuideTopic(const char *&page, const char *&section) {
    page = "samples";
    section = "Sample editing";
}

void SampleEditDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
    name = "SAMPLE EDIT";
    where = "Sel on SOURCE/LOOP page";
    edit = "B back to the instrument";
    field = "Edits work on S..E";
    cmd1 = "Up/Down pick an edit";
    cmd2 = "A make it: a new file";
    cmd3 = "original file is kept";
    cmd4 = "START hear the sample";
    cmd5 = "B+Sel on the instrument:";
    cmd6 = "  undo, back to before";
    cmd7 = "S, E: LB keys on the page";
}
