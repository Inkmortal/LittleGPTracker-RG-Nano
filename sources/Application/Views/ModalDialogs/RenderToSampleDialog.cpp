#include "RenderToSampleDialog.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Groove.h"
#include "Application/Player/Player.h"
#include "Services/Audio/Audio.h"
#include "System/Console/Trace.h"
#include "System/FileSystem/FileSystem.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string.h>

#define DIALOG_WIDTH 26
#define DIALOG_HEIGHT 12
#define BAR_Y 5

RenderToSampleDialog::RenderToSampleDialog(View &view, int mode, int bars)
    : ModalView(view), mode_(mode), bars_(bars < 1 ? 1 : bars), started_(false),
      done_(false), cancelled_(false), instrument_(-1) {}

RenderToSampleDialog::~RenderToSampleDialog() {}

// One tick lasts 2.5/tempo s (a 6-tick step is a 16th note); a step lasts
// as many ticks as the channel's groove says
int RenderToSampleDialog::framesFor(int bars) {
    int channel = viewData_->songX_;
    int grooveIndex = 0, position = 0;
    Groove::GetInstance()->GetChannelData(channel, &grooveIndex, &position);
    unsigned char *groove = Groove::GetInstance()->GetGrooveData(grooveIndex);
    int count = 0;
    while (count < 16 && groove[count] != NO_GROOVE_DATA)
        count++;
    int ticks = 0;
    for (int step = 0; step < 16 * bars; step++) {
        ticks += count ? groove[step % count] : 6;
    }
    int tempo = viewData_->project_->GetTempo();
    if (tempo < 1)
        tempo = 120;
    float rate = (float)Audio::GetInstance()->GetSampleRate();
    return (int)(ticks * rate * 2.5f / tempo + 0.5f);
}

void RenderToSampleDialog::OnFocus() {
    // First free name: rs_01.wav, rs_02.wav ...
    char name[32];
    for (int i = 1; i < 100; i++) {
        sprintf(name, "rs_%02d.wav", i);
        // Exists() checks the literal path, so resolve the alias first
        Path alias(std::string("samples:") + name);
        Path candidate(alias.GetPath());
        if (!candidate.Exists())
            break;
    }
    file_ = name;
    if (viewData_->project_->GetInstrumentBank()->GetNext() == NO_MORE_INSTRUMENT) {
        error_ = "no free instrument slot";
        done_ = true;
        return;
    }
    Player *player = Player::GetInstance();
    if (player->IsRunning())
        player->Stop();
    std::string path = std::string("samples:") + file_;
    Path target(path);
    MixerService *mixer = MixerService::GetInstance();
    mixer->Lock();
    int frames = framesFor(bars_);
    mixer->GetAudioOut()->StartCapture(target.GetPath().c_str(), frames);
    player->OnStartButton((PlayMode)mode_, viewData_->songX_, false, viewData_->chainRow_);
    mixer->Unlock();
    started_ = true;
    Trace::Log("RENDER", "to sample %s: %d bars, %d frames", file_.c_str(), bars_, frames);
    isDirty_ = true;
}

void RenderToSampleDialog::finish() {
    // Stopping the player sends a player update straight back here
    done_ = true;
    Player::GetInstance()->Stop();
    int index = SamplePool::GetInstance()->AddProjectSample(file_.c_str());
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    int slot = bank->GetNext();
    if (index < 0 || slot == NO_MORE_INSTRUMENT) {
        error_ = index < 0 ? "could not load the render" : "no free instrument slot";
    } else {
        ((SampleInstrument *)bank->GetInstrument(slot))->AssignSample(index);
        instrument_ = slot;
    }
    Trace::Log("RENDER", "done %s -> instrument %02X", file_.c_str(), instrument_);
    isDirty_ = true;
}

void RenderToSampleDialog::cancel() {
    MixerService *mixer = MixerService::GetInstance();
    mixer->Lock();
    mixer->GetAudioOut()->CancelCapture();
    mixer->Unlock();
    cancelled_ = true; // the stop's player update must not load the file
    Player::GetInstance()->Stop();
    Path target(std::string("samples:") + file_);
    FileSystem::GetInstance()->Delete(target.GetPath().c_str());
}

void RenderToSampleDialog::OnPlayerUpdate(PlayerEventType, unsigned int) {
    if (started_ && !done_ && !cancelled_) {
        if (!MixerService::GetInstance()->GetAudioOut()->CaptureActive()) {
            finish();
        }
        // Player ticks only flush the screen, so draw the new state now
        DrawView();
    }
}

void RenderToSampleDialog::DrawView() {
    SetWindow(DIALOG_WIDTH, DIALOG_HEIGHT);
    GUITextProperties props;
    char line[40];

    SetColor(CD_HILITE2);
    DrawString(0, 0, "RENDER TO SAMPLE", props);
    SetColor(CD_NORMAL);
    if (mode_ == PM_CHAIN) {
        sprintf(line, "chain %02X, %d bar%s", viewData_->currentChain_, bars_, bars_ == 1 ? "" : "s");
    } else {
        sprintf(line, "phrase %02X, 1 bar", viewData_->currentPhrase_);
    }
    DrawString(0, 2, line, props);
    sprintf(line, "-> %s", file_.c_str());
    DrawString(0, 3, line, props);

    if (!error_.empty()) {
        SetColor(CD_CURSOR);
        DrawString(0, 7, error_.c_str(), props);
        SetColor(CD_NORMAL);
        DrawString(0, 10, "B back", props);
        return;
    }
    int progress = done_ ? 100 : MixerService::GetInstance()->GetAudioOut()->CaptureProgress();
    sprintf(line, "%3d%%", progress);
    DrawString(DIALOG_WIDTH - 4, BAR_Y, line, props);
    if (done_) {
        SetColor(CD_HILITE1);
        sprintf(line, "saved in instrument %02X", instrument_);
        DrawString(0, 7, line, props);
        SetColor(CD_NORMAL);
        DrawString(0, 10, "A open it    B back", props);
    } else {
        SetColor(CD_MUTE);
        DrawString(0, 7, "playing it once...", props);
        DrawString(0, 10, "B cancel", props);
        SetColor(CD_NORMAL);
    }
}

void RenderToSampleDialog::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    int progress = done_ ? 100 : MixerService::GetInstance()->GetAudioOut()->CaptureProgress();
    const int x = windowLeft() * 8;
    const int y = (windowTop() + BAR_Y) * 8;
    const int w = (DIALOG_WIDTH - 5) * 8;
    GUIColor frame = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 45);
    GUIColor fill = AppWindow::ThemeColor(done_ ? CD_PLAY : CD_HILITE2);
    imp->SetColor(frame);
    GUIRect outer(x, y, x + w, y + 8);
    imp->DrawRect(outer);
    imp->SetColor(fill);
    GUIRect bar(x + 1, y + 1, x + 1 + ((w - 2) * progress) / 100, y + 7);
    imp->DrawRect(bar);
#endif
}

void RenderToSampleDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    if (mask == EPBM_B) {
        if (started_ && !done_)
            cancel();
        EndModal(0);
    } else if (mask == EPBM_A && done_ && instrument_ >= 0) {
        EndModal(1);
    }
}

void RenderToSampleDialog::GetGuideTopic(const char *&page, const char *&section) {
    page = "samples";
    section = "Render to sample";
}
