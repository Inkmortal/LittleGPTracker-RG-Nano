#include "SoundBrowserDialog.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SoundLibrary.h"
#include "Application/Instruments/SynthEngines.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Player/Player.h"
#include "System/Console/Trace.h"
#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SB_WIDTH 26
#define SB_HEIGHT 22
#define SB_LIST_Y 2
#define SB_ROWS 15
#define SB_STATUS_Y 18
#define SB_HINT_Y 20

SoundBrowserDialog::SoundBrowserDialog(View &view, int slot, int note)
    : ModalView(view), slot_(slot), note_(note), inCategory_(false),
      category_(SC_SOUNDS), selected_(0), top_(0), categorySelected_(0),
      triedOn_(false) {
    // Keep the slot as it is now, to put it back if nothing is taken
    TiXmlElement root("ORIGINAL");
    TiXmlNode *node = original_.InsertEndChild(root);
    InstrumentBank::SaveInstrument(
        node, -1, viewData_->project_->GetInstrumentBank()->GetInstrument(slot_));
    Path lib(SamplePool::GetInstance()->GetSampleLib());
    sampleRoot_ = lib.GetPath();
    while (sampleRoot_.size() > 1 &&
           (sampleRoot_[sampleRoot_.size() - 1] == '/' ||
            sampleRoot_[sampleRoot_.size() - 1] == '\\'))
        sampleRoot_.erase(sampleRoot_.size() - 1);
    showCategories();
}

SoundBrowserDialog::~SoundBrowserDialog() {}

void SoundBrowserDialog::OnFocus() {}

void SoundBrowserDialog::OnPlayerUpdate(PlayerEventType, unsigned int) {}

void SoundBrowserDialog::showCategories() {
    inCategory_ = false;
    items_.clear();
    std::vector<std::string> saved;
    SoundLibrary::ListSounds(saved);
    if (!saved.empty()) {
        Item item = {SB_CATEGORY, 0, "", "", SC_SOUNDS};
        char name[32];
        sprintf(name, "My sounds (%d)", (int)saved.size());
        item.name = name;
        items_.push_back(item);
    }
    // Sets of presets that belong together ("Chinese instruments")
    for (int group = 0; group < SynthInstrument::GetPresetGroupCount(); group++) {
        Item item = {SB_CATEGORY, group, SynthInstrument::GetPresetGroupName(group),
                     "", SC_GROUP};
        items_.push_back(item);
    }
    // One entry per synth engine that has presets
    for (int engine = 0; engine < SE_LAST; engine++) {
        int first, last;
        SynthInstrument::GetPresetRange(engine, first, last);
        if (first < 0 || last < first)
            continue;
        Item item = {SB_CATEGORY, engine, "", "", SC_SYNTH};
        // "Synth presets", "FM4 synth presets" ...
        std::string engineName = SynthInstrument::GetEngineName(engine);
        if (engine == 0) {
            item.name = "Synth presets";
        } else {
            for (size_t i = 0; i < engineName.size(); i++)
                engineName[i] = toupper(engineName[i]);
            item.name = engineName + " synth presets";
        }
        items_.push_back(item);
    }
    Item macro = {SB_CATEGORY, 0, "Macro synth presets", "", SC_MACRO};
    items_.push_back(macro);
    if (FileSystem::GetInstance()->GetFileType(sampleRoot_.c_str()) == FT_DIR) {
        Item samples = {SB_CATEGORY, 0, "Samples", sampleRoot_, SC_SAMPLES};
        items_.push_back(samples);
    }
    selected_ = categorySelected_;
    if (selected_ >= (int)items_.size())
        selected_ = 0;
    top_ = 0;
    move(0);
    isDirty_ = true;
}

void SoundBrowserDialog::openCategory(const Item &cat) {
    categorySelected_ = selected_;
    inCategory_ = true;
    items_.clear();
    status_.clear();
    if (cat.category == SC_SYNTH) {
        category_ = SC_SYNTH;
        int first, last;
        SynthInstrument::GetPresetRange(cat.index, first, last);
        for (int p = first; p <= last; p++) {
            Item item = {SB_SYNTH, p, SynthInstrument::GetPresetName(p), "", 0};
            items_.push_back(item);
        }
    } else if (cat.category == SC_GROUP) {
        category_ = SC_GROUP;
        groupTitle_ = SynthInstrument::GetPresetGroupTitle(cat.index);
        std::vector<int> presets;
        SynthInstrument::GetPresetGroup(cat.index, presets);
        for (size_t i = 0; i < presets.size(); i++) {
            Item item = {SB_SYNTH, presets[i], SynthInstrument::GetPresetName(presets[i]),
                         "", 0};
            items_.push_back(item);
        }
    } else if (cat.category == SC_SOUNDS) {
        category_ = SC_SOUNDS;
        std::vector<std::string> saved;
        SoundLibrary::ListSounds(saved);
        for (size_t i = 0; i < saved.size(); i++) {
            Item item = {SB_SOUND, (int)i, saved[i], "", 0};
            items_.push_back(item);
        }
    } else if (cat.category == SC_MACRO) {
        category_ = SC_MACRO;
        for (int p = 0; p < MacroInstrument::GetPresetCount(); p++) {
            Item item = {SB_MACRO, p, MacroInstrument::GetPresetName(p), "", 0};
            items_.push_back(item);
        }
    } else {
        category_ = SC_SAMPLES;
        openFolder(sampleRoot_);
        return;
    }
    selected_ = 0;
    top_ = 0;
    move(0);
    hear();
}

void SoundBrowserDialog::openFolder(const std::string &path) {
    stopSound();
    std::string from = folder_;
    folder_ = path;
    items_.clear();
    I_Dir *dir = FileSystem::GetInstance()->Open(path.c_str());
    if (dir) {
        dir->GetContent((char *)"*");
        dir->Sort();
        IteratorPtr<Path> it(dir->GetIterator());
        for (it->Begin(); !it->IsDone(); it->Next()) {
            Path &p = it->CurrentItem();
            std::string name = p.GetName();
            if (p.IsDirectory() && name[0] != '.') {
                Item item = {SB_FOLDER, 0, name + "/", p.GetPath(), 0};
                items_.push_back(item);
            }
        }
        for (it->Begin(); !it->IsDone(); it->Next()) {
            Path &p = it->CurrentItem();
            std::string name = p.GetName();
            if (!p.IsDirectory() && name[0] != '.' && p.Matches("*.wav")) {
                Item item = {SB_SAMPLE, 0, name, p.GetPath(), 0};
                items_.push_back(item);
            }
        }
        delete dir;
    }
    // Back up a folder: the cursor lands on the folder you came from
    selected_ = 0;
    for (size_t i = 0; i < items_.size(); i++) {
        if (items_[i].kind == SB_FOLDER && items_[i].path == from)
            selected_ = (int)i;
    }
    top_ = 0;
    move(0);
    isDirty_ = true;
}

void SoundBrowserDialog::move(int delta) {
    int count = (int)items_.size();
    if (count == 0) {
        selected_ = 0;
        top_ = 0;
        return;
    }
    selected_ += delta;
    if (selected_ < 0)
        selected_ = 0;
    if (selected_ >= count)
        selected_ = count - 1;
    if (selected_ < top_)
        top_ = selected_;
    if (selected_ >= top_ + SB_ROWS)
        top_ = selected_ - SB_ROWS + 1;
    isDirty_ = true;
}

void SoundBrowserDialog::stopSound() {
    Player *player = Player::GetInstance();
    player->StopStreaming();
    if (viewData_->playMode_ == PM_AUDITION)
        player->Stop();
}

void SoundBrowserDialog::audition() {
    Player *player = Player::GetInstance();
    // Never cut the song short: while it plays you hear the sound in it
    if (player->IsRunning() && viewData_->playMode_ != PM_AUDITION)
        return;
    player->AuditionInstrument(slot_, note_);
}

// Play the sound under the cursor: synth sounds go in the slot and play,
// samples stream from the card
void SoundBrowserDialog::hear() {
    if (!inCategory_ || items_.empty())
        return;
    stopSound();
    const Item &item = items_[selected_];
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    std::string error;
    switch (item.kind) {
    case SB_SYNTH: {
        bank->SetInstrumentType(slot_, IT_SYNTH);
        ((SynthInstrument *)bank->GetInstrument(slot_))->ApplyPreset(item.index);
        triedOn_ = true;
        audition();
        break;
    }
    case SB_MACRO: {
        bank->SetInstrumentType(slot_, IT_MACRO);
        ((MacroInstrument *)bank->GetInstrument(slot_))->ApplyPreset(item.index);
        triedOn_ = true;
        audition();
        break;
    }
    case SB_SOUND: {
        std::string wav = SoundLibrary::SampleOf(item.name);
        if (!wav.empty()) {
            Path path(wav);
            Player::GetInstance()->StartStreaming(path);
        } else if (SoundLibrary::TryOnSound(bank, slot_, item.name, error)) {
            triedOn_ = true;
            audition();
        } else {
            status_ = error;
        }
        break;
    }
    case SB_SAMPLE: {
        Path path(item.path);
        Player::GetInstance()->StartStreaming(path);
        break;
    }
    default:
        break;
    }
    isDirty_ = true;
}

// Put the slot back as it was when the browser opened
void SoundBrowserDialog::restore() {
    stopSound();
    if (!triedOn_)
        return;
    TiXmlElement *root = original_.FirstChildElement("ORIGINAL");
    TiXmlElement *instrument = root ? root->FirstChildElement("INSTRUMENT") : 0;
    if (instrument) {
        viewData_->project_->GetInstrumentBank()->RestoreInstrument(
            instrument, slot_, 100, true);
    }
    triedOn_ = false;
}

void SoundBrowserDialog::take() {
    if (items_.empty())
        return;
    const Item &item = items_[selected_];
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    char done[48];
    std::string error;
    switch (item.kind) {
    case SB_CATEGORY:
        openCategory(item);
        return;
    case SB_FOLDER:
        openFolder(item.path);
        return;
    case SB_SYNTH:
    case SB_MACRO:
        // Already in the slot from hearing it
        if (!triedOn_)
            hear();
        break;
    case SB_SOUND:
        // The whole sound this time: its table, its sample into the song
        stopSound();
        if (!SoundLibrary::LoadSound(bank, slot_, item.name, error)) {
            status_ = error;
            isDirty_ = true;
            return;
        }
        break;
    case SB_SAMPLE: {
        stopSound();
        SamplePool *pool = SamplePool::GetInstance();
        Path path(item.path);
        int index = pool->FindSample(path.GetName().c_str());
        if (index < 0)
            index = pool->ImportSample(path);
        if (index < 0) {
            status_ = pool->TakeLoadTooBig() ? "too long: no memory"
                                             : "can't load it";
            isDirty_ = true;
            return;
        }
        if (bank->GetInstrument(slot_)->GetType() != IT_SAMPLE) {
            bank->SetInstrumentType(slot_, IT_SAMPLE);
        }
        ((SampleInstrument *)bank->GetInstrument(slot_))->AssignSample(index);
        break;
    }
    }
    stopSound();
    triedOn_ = false; // it stays: nothing to put back
    snprintf(done, sizeof(done), "%s in %02X", item.name.c_str(), slot_);
    result_ = done;
    Trace::Log("RACK", "took %s", done);
    EndModal(1);
}

bool SoundBrowserDialog::Back() {
    if (inCategory_ && category_ == SC_SAMPLES && folder_ != sampleRoot_) {
        Path up = Path(folder_).GetParent();
        std::string parent = up.GetPath();
        while (parent.size() > 1 && (parent[parent.size() - 1] == '/' ||
                                     parent[parent.size() - 1] == '\\'))
            parent.erase(parent.size() - 1);
        openFolder(parent);
        return true;
    }
    restore();
    if (inCategory_) {
        folder_.clear();
        showCategories();
        return true;
    }
    EndModal(0);
    return true;
}

void SoundBrowserDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    switch (mask) {
    case EPBM_UP:
    case EPBM_DOWN:
        if (items_.empty())
            return;
        move(mask == EPBM_UP ? -1 : 1);
        status_.clear();
        hear();
        break;
    case EPBM_LEFT:
    case EPBM_RIGHT:
        if (items_.empty())
            return;
        move(mask == EPBM_LEFT ? -SB_ROWS : SB_ROWS);
        status_.clear();
        hear();
        break;
    case EPBM_START:
        hear();
        break;
    case EPBM_A:
        take();
        break;
    default:
        break;
    }
}

void SoundBrowserDialog::DrawView() {
    SetWindow(SB_WIDTH, SB_HEIGHT);
    GUITextProperties props;
    char line[48];

    SetColor(CD_HILITE2);
    snprintf(line, sizeof(line), "SOUND FOR %02X", slot_);
    DrawString(0, 0, line, props);
    SetColor(CD_MUTE);
    if (!inCategory_) {
        DrawString(13, 0, "pick a kind", props);
    } else if (category_ == SC_SAMPLES) {
        std::string where = folder_ == sampleRoot_ ? "samples" : Path(folder_).GetName();
        snprintf(line, sizeof(line), "%.12s", where.c_str());
        DrawString(13, 0, line, props);
    } else {
        DrawString(13, 0, category_ == SC_SOUNDS  ? "my sounds"
                          : category_ == SC_MACRO ? "macro synth"
                          : category_ == SC_GROUP ? groupTitle_.c_str()
                                                  : "synth presets",
                   props);
    }

    if (items_.empty()) {
        SetColor(CD_MUTE);
        DrawString(0, SB_LIST_Y, "nothing here", props);
    }
    for (int r = 0; r < SB_ROWS; r++) {
        int i = top_ + r;
        if (i >= (int)items_.size())
            break;
        const Item &item = items_[i];
        bool on = (i == selected_);
        props.invert_ = on;
        SetColor(on ? CD_CURSOR
                    : ((item.kind == SB_CATEGORY || item.kind == SB_FOLDER)
                           ? CD_HILITE1
                           : CD_NORMAL));
        snprintf(line, sizeof(line), "%-26.26s", item.name.c_str());
        DrawString(0, SB_LIST_Y + r, line, props);
    }
    props.invert_ = false;

    SetColor(CD_HILITE1);
    DrawString(0, SB_STATUS_Y, status_.substr(0, SB_WIDTH).c_str(), props);
    SetColor(CD_NORMAL);
    if (!inCategory_) {
        DrawString(0, SB_HINT_Y, "A open       B close", props);
    } else {
        DrawString(0, SB_HINT_Y, "moving plays  START again", props);
        DrawString(0, SB_HINT_Y + 1, "A take it    B put back", props);
    }
}

void SoundBrowserDialog::GetGuideTopic(const char *&page, const char *&section) {
    page = "sounds-first";
    section = "The sound browser";
}

void SoundBrowserDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
    name = "SOUND BROWSER";
    where = "Select on the Rack";
    edit = "A take  B put back";
    field = "Hear sounds as you move";
    cmd1 = "Up/Down next sound: plays";
    cmd2 = "Left/Right a page";
    cmd3 = "Start: hear it again";
    cmd4 = "A: keep it in the slot";
    cmd5 = "B: slot back as it was";
    cmd6 = "Samples: A opens folders";
    cmd7 = "My sounds: your saved ones";
}
