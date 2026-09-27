#include "SoundFilesDialog.h"
#include "MessageBox.h"
#include "NewProjectDialog.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SoundLibrary.h"
#include "Application/Player/Player.h"
#include "System/Console/Trace.h"
#include <stdio.h>
#include <string.h>

#define SF_WIDTH 26
#define SF_HEIGHT 18
#define SF_LIST_Y 2
#define SF_ROWS 10
#define SF_STATUS_Y 14
#define SF_HINT_Y 16

static void namedCallback(View &v, ModalView &dialog) {
    SoundFilesDialog &sf = (SoundFilesDialog &)v;
    if (dialog.GetReturnCode() > 0) {
        sf.Named(((NewProjectDialog &)dialog).GetTypedName());
    }
}

static void answeredCallback(View &v, ModalView &dialog) {
    ((SoundFilesDialog &)v).Answered(dialog.GetReturnCode() == MBL_YES);
}

SoundFilesDialog::SoundFilesDialog(View &view, int slot)
    : ModalView(view), slot_(slot), mode_(SF_MENU), selected_(0), top_(0),
      pending_(SFA_SAVE_SOUND), changed_(false) {
    buildMenu();
}

SoundFilesDialog::~SoundFilesDialog() {}

void SoundFilesDialog::buildMenu() {
    menu_.clear();
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    bool soundSlot = slot_ >= 0 && slot_ < MAX_SAMPLEINSTRUMENT_COUNT;
    if (soundSlot && !bank->GetInstrument(slot_)->IsEmpty())
        menu_.push_back(SFA_SAVE_SOUND);
    if (soundSlot)
        menu_.push_back(SFA_LOAD_SOUND);
    menu_.push_back(SFA_SAVE_KIT);
    menu_.push_back(SFA_LOAD_KIT);
    menu_.push_back(SFA_SET_TEMPLATE);
    if (SoundLibrary::HasTemplate())
        menu_.push_back(SFA_CLEAR_TEMPLATE);
    if (selected_ >= (int)menu_.size())
        selected_ = (int)menu_.size() - 1;
}

void SoundFilesDialog::OnFocus() {}

void SoundFilesDialog::OnPlayerUpdate(PlayerEventType, unsigned int) {}

std::string SoundFilesDialog::songName() {
    Path project("project:");
    std::string path = project.GetPath();
    while (!path.empty() &&
           (path[path.size() - 1] == '/' || path[path.size() - 1] == '\\'))
        path.erase(path.size() - 1);
    std::string name = Path(path).GetName();
    // Song folders are lgpt_<name>
    if (name.compare(0, 5, "lgpt_") == 0)
        name.erase(0, 5);
    return name.substr(0, MAX_NAME_LENGTH);
}

static const char *menuText(int action, int slot, char *buffer) {
    switch (action) {
    case 0:
        sprintf(buffer, "Save sound %02X", slot);
        return buffer;
    case 1:
        sprintf(buffer, "Load a sound into %02X", slot);
        return buffer;
    case 2:
        return "Save this kit";
    case 3:
        return "Load a kit";
    case 4:
        return "New songs: this kit";
    default:
        return "New songs: starter kit";
    }
}

void SoundFilesDialog::DrawView() {
    SetWindow(SF_WIDTH, SF_HEIGHT);
    GUITextProperties props;
    char line[40];

    SetColor(CD_HILITE2);
    const char *title = mode_ == SF_MENU     ? "MY SOUNDS"
                        : mode_ == SF_SOUNDS ? "SAVED SOUNDS"
                                             : "SAVED KITS";
    DrawString(0, 0, title, props);
    if (mode_ == SF_MENU && SoundLibrary::HasTemplate()) {
        SetColor(CD_MUTE);
        DrawString(SF_WIDTH - 9, 0, "kit: mine", props);
    }

    if (mode_ == SF_MENU) {
        for (int i = 0; i < (int)menu_.size(); i++) {
            char buffer[32];
            bool on = (i == selected_);
            props.invert_ = on;
            SetColor(on ? CD_CURSOR : CD_NORMAL);
            snprintf(line, sizeof(line), "%-26s",
                     menuText(menu_[i], slot_, buffer));
            DrawString(0, SF_LIST_Y + i, line, props);
        }
    } else if (names_.empty()) {
        SetColor(CD_MUTE);
        DrawString(0, SF_LIST_Y,
                   mode_ == SF_SOUNDS ? "no saved sounds yet" : "no saved kits yet",
                   props);
    } else {
        for (int r = 0; r < SF_ROWS; r++) {
            int i = top_ + r;
            if (i >= (int)names_.size())
                break;
            bool on = (i == selected_);
            props.invert_ = on;
            SetColor(on ? CD_CURSOR : CD_NORMAL);
            snprintf(line, sizeof(line), "%-26.26s", names_[i].c_str());
            DrawString(0, SF_LIST_Y + r, line, props);
        }
    }
    props.invert_ = false;

    SetColor(CD_HILITE1);
    DrawString(0, SF_STATUS_Y, "                          ", props);
    DrawString(0, SF_STATUS_Y, status_.substr(0, SF_WIDTH).c_str(), props);

    SetColor(CD_NORMAL);
    if (mode_ == SF_MENU) {
        DrawString(0, SF_HINT_Y, "A choose   B close", props);
    } else if (mode_ == SF_SOUNDS) {
        sprintf(line, "A load into %02X  B back", slot_);
        DrawString(0, SF_HINT_Y, line, props);
    } else {
        DrawString(0, SF_HINT_Y, "A load kit   B back", props);
    }
}

void SoundFilesDialog::openList(Mode mode) {
    mode_ = mode;
    if (mode == SF_SOUNDS)
        SoundLibrary::ListSounds(names_);
    else
        SoundLibrary::ListKits(names_);
    selected_ = 0;
    top_ = 0;
    status_.clear();
    isDirty_ = true;
}

bool SoundFilesDialog::Back() {
    if (mode_ != SF_MENU) {
        mode_ = SF_MENU;
        selected_ = 0;
        buildMenu();
        isDirty_ = true;
        return true;
    }
    EndModal(changed_ ? 1 : 0);
    return true;
}

void SoundFilesDialog::askName(const char *title, const std::string &current) {
    NewProjectDialog *npd = new NewProjectDialog(*this);
    npd->SetRename(title, current);
    DoModal(npd, namedCallback);
}

void SoundFilesDialog::ask(const char *question) {
    MessageBox *mb = new MessageBox(*this, question, MBBF_YES | MBBF_NO);
    DoModal(mb, answeredCallback);
}

void SoundFilesDialog::report(bool ok, const std::string &done,
                              const std::string &error) {
    status_ = ok ? done : error;
    Trace::Log("SOUNDLIB", "%s", status_.c_str());
    buildMenu();
    isDirty_ = true;
}

void SoundFilesDialog::activate() {
    if (mode_ == SF_SOUNDS || mode_ == SF_KITS) {
        if (names_.empty())
            return;
        pendingName_ = names_[selected_];
        if (mode_ == SF_SOUNDS) {
            loadSound(pendingName_);
        } else {
            pending_ = SFA_LOAD_KIT;
            ask("Replace every sound?");
        }
        return;
    }
    pending_ = (Action)menu_[selected_];
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    switch (pending_) {
    case SFA_SAVE_SOUND: {
        std::string current = bank->GetInstrument(slot_)->GetName();
        askName("NAME THIS SOUND", SoundLibrary::CleanName(current));
        break;
    }
    case SFA_LOAD_SOUND:
        openList(SF_SOUNDS);
        break;
    case SFA_SAVE_KIT:
        askName("NAME THIS KIT", songName());
        break;
    case SFA_LOAD_KIT:
        openList(SF_KITS);
        break;
    case SFA_SET_TEMPLATE:
        ask("New songs start with it?");
        break;
    case SFA_CLEAR_TEMPLATE: {
        bool ok = SoundLibrary::ClearTemplate();
        report(ok, "new songs: starter kit", "can't remove it");
        break;
    }
    }
}

void SoundFilesDialog::Named(const std::string &name) {
    pendingName_ = SoundLibrary::CleanName(name);
    if (pendingName_.empty()) {
        report(false, "", "give it a name");
        return;
    }
    bool exists = pending_ == SFA_SAVE_SOUND
                      ? SoundLibrary::SoundExists(pendingName_)
                      : SoundLibrary::KitExists(pendingName_);
    if (exists) {
        ask("That name exists. Replace?");
        return;
    }
    if (pending_ == SFA_SAVE_SOUND)
        saveSound(pendingName_);
    else
        saveKit(pendingName_);
}

void SoundFilesDialog::Answered(bool yes) {
    if (!yes) {
        status_.clear();
        isDirty_ = true;
        return;
    }
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    switch (pending_) {
    case SFA_SAVE_SOUND:
        saveSound(pendingName_);
        break;
    case SFA_SAVE_KIT:
        saveKit(pendingName_);
        break;
    case SFA_LOAD_KIT:
        loadKit(pendingName_);
        break;
    case SFA_SET_TEMPLATE: {
        std::string error;
        bool ok = SoundLibrary::SetTemplate(bank, error);
        report(ok, "new songs start with it", error);
        break;
    }
    default:
        break;
    }
}

void SoundFilesDialog::saveSound(const std::string &name) {
    std::string error;
    bool ok = SoundLibrary::SaveSound(viewData_->project_->GetInstrumentBank(),
                                      slot_, name, error);
    report(ok, "saved " + name, error);
}

void SoundFilesDialog::saveKit(const std::string &name) {
    std::string error;
    bool ok = SoundLibrary::SaveKit(viewData_->project_->GetInstrumentBank(),
                                    name, error);
    report(ok, "kit saved: " + name, error);
}

void SoundFilesDialog::loadSound(const std::string &name) {
    Player::GetInstance()->Stop();
    std::string error;
    bool ok = SoundLibrary::LoadSound(viewData_->project_->GetInstrumentBank(),
                                      slot_, name, error);
    if (ok) {
        changed_ = true;
        viewData_->currentInstrument_ = slot_;
        viewData_->instrumentPicked_ = true;
    }
    char done[40];
    snprintf(done, sizeof(done), "%s now in %02X", name.c_str(), slot_);
    report(ok, done, error);
}

void SoundFilesDialog::loadKit(const std::string &name) {
    Player::GetInstance()->Stop();
    std::string error;
    bool ok = SoundLibrary::LoadKit(viewData_->project_->GetInstrumentBank(),
                                    name, error);
    // Even a kit that stopped half way has changed sounds
    changed_ = true;
    report(ok, "kit loaded: " + name, error);
}

void SoundFilesDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    int count = mode_ == SF_MENU ? (int)menu_.size() : (int)names_.size();
    if (mask == EPBM_UP || mask == EPBM_DOWN) {
        if (count == 0)
            return;
        selected_ += (mask == EPBM_UP) ? -1 : 1;
        if (selected_ < 0)
            selected_ = count - 1;
        if (selected_ >= count)
            selected_ = 0;
        if (selected_ < top_)
            top_ = selected_;
        if (selected_ >= top_ + SF_ROWS)
            top_ = selected_ - SF_ROWS + 1;
        status_.clear();
        isDirty_ = true;
    } else if (mask == EPBM_A) {
        activate();
        isDirty_ = true;
    }
}

void SoundFilesDialog::GetGuideTopic(const char *&page, const char *&section) {
    page = "sounds-first";
    section = "Save and load";
}

void SoundFilesDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
    name = "MY SOUNDS";
    where = "LB+Start: Rack, list";
    edit = "B back";
    field = "Sounds kept for all songs";
    cmd1 = "Up/Down pick, A choose";
    cmd2 = "Save sound: this one";
    cmd3 = "Save kit: all 128 slots";
    cmd4 = "Load kit replaces all";
    cmd5 = "New songs: this kit";
    cmd6 = "  = your template";
    cmd7 = "Files: Applications/Sounds";
}
