#include "RecordSampleDialog.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/WavFileWriter.h"
#include "Application/Player/Player.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"
#include <sstream>
#include <iomanip>

RecordSampleDialog::RecordSampleDialog(View &view)
    : ModalView(view),
      capture_(NULL),
      state_(RS_SELECT_DEVICE),
      recordDuration_(5),
      targetInstrument_(0),
      elapsedTime_(0.0f),
      selectedDeviceIndex_(0),
      statusMessage_("") {

    capture_ = new ALSAAudioCapture();
}

RecordSampleDialog::~RecordSampleDialog() {
    if (capture_) {
        capture_->Close();
        delete capture_;
        capture_ = NULL;
    }
}

void RecordSampleDialog::DrawView() {
    if (state_ == RS_SELECT_DEVICE) {
        drawDeviceSelection();
    } else {
        drawRecordingScreen();
    }
}

void RecordSampleDialog::OnPlayerUpdate(PlayerEventType type, unsigned int currentTick) {
    // Update display during recording
    if (state_ == RS_RECORDING) {
        // Poll ALSA for audio data
        capture_->Update();

        isDirty_ = true;

        // Auto-stop if max duration reached
        if (!capture_->IsRecording()) {
            stopRecording();
        }
    }
}

void RecordSampleDialog::OnFocus() {
    targetInstrument_ = viewData_->currentInstrument_;
    state_ = RS_SELECT_DEVICE;
    selectedDeviceIndex_ = 0;

    // Enumerate available capture devices
    availableDevices_ = ALSAAudioCapture::EnumerateCaptureDevices();
    Trace::Log("RECORD", "Found %d capture devices", (int)availableDevices_.size());

    isDirty_ = true;
}

bool RecordSampleDialog::Back() {
    switch (state_) {
        case RS_SELECT_DEVICE:
            cancel();
            break;
        case RS_READY:
            state_ = RS_SELECT_DEVICE;
            break;
        case RS_RECORDING:
            capture_->StopRecording();
            state_ = RS_READY;
            statusMessage_ = "Take dropped";
            break;
        case RS_STOPPED:
            state_ = RS_READY;
            statusMessage_ = "Take dropped";
            break;
        case RS_SAVING:
            break;
    }
    isDirty_ = true;
    return true;
}

void RecordSampleDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;

    switch (state_) {
        case RS_SELECT_DEVICE:
            if (mask & EPBM_UP) {
                selectedDeviceIndex_--;
                if (selectedDeviceIndex_ < 0) selectedDeviceIndex_ = availableDevices_.size() - 1;
                isDirty_ = true;
            } else if (mask & EPBM_DOWN) {
                selectedDeviceIndex_++;
                if (selectedDeviceIndex_ >= (int)availableDevices_.size()) selectedDeviceIndex_ = 0;
                isDirty_ = true;
            } else if (mask & EPBM_A) {
                selectDevice();
            }
            break;

        case RS_READY:
            if (mask & EPBM_START) {
                startRecording();
            } else if (mask & EPBM_UP) {
                recordDuration_++;
                if (recordDuration_ > MAX_RECORDING_SECONDS) recordDuration_ = MAX_RECORDING_SECONDS;
                isDirty_ = true;
            } else if (mask & EPBM_DOWN) {
                recordDuration_--;
                if (recordDuration_ < 1) recordDuration_ = 1;
                isDirty_ = true;
            }
            break;

        case RS_RECORDING:
            if (mask & EPBM_START) {
                stopRecording();
            }
            break;

        case RS_STOPPED:
            if (mask & EPBM_A) {
                saveRecording();
            }
            break;

        case RS_SAVING:
            // Waiting for save to complete
            break;
    }
}

void RecordSampleDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
	name="RECORD";
	where="External capture";
	edit="Start record/stop";
	field="Recording modal";
	cmd1="Up/Down device/len";
	cmd2="A select/save";
	cmd3="B back one step";
	cmd4="Start rec/stop";
	cmd5="RG Nano has no mic";
	cmd6="Use imported WAVs";
	cmd7="RB+Select helper";
}

void RecordSampleDialog::startRecording() {
    statusMessage_ = "";

    if (!capture_->Init()) {
        statusMessage_ = capture_->GetLastError();
        if (statusMessage_.empty()) {
            statusMessage_ = "Failed to open device";
        }
        Trace::Error("Failed to initialize audio capture");
        isDirty_ = true;
        return;
    }

    if (capture_->StartRecording(recordDuration_)) {
        state_ = RS_RECORDING;

        // Build status message
        char status[128];
        snprintf(status, sizeof(status), "Device: %s | Monitor: %s",
                 capture_->GetCurrentDevice().c_str(),
                 capture_->IsMonitoringEnabled() ? "ON" : "OFF");
        statusMessage_ = status;

        Trace::Log("RECORD", "Started recording for %d seconds", recordDuration_);
        isDirty_ = true;
    } else {
        statusMessage_ = "Failed to start recording";
        Trace::Error("Failed to start recording");
        isDirty_ = true;
    }
}

void RecordSampleDialog::stopRecording() {
    if (state_ != RS_RECORDING) return;

    capture_->StopRecording();
    elapsedTime_ = capture_->GetRecordingTime();
    state_ = RS_STOPPED;

    Trace::Log("RECORD", "Stopped recording, captured %.2f seconds", elapsedTime_);
    isDirty_ = true;
}

void RecordSampleDialog::saveRecording() {
    state_ = RS_SAVING;
    isDirty_ = true;

    // Get captured buffer
    int sampleCount = 0;
    short *buffer = capture_->GetBuffer(sampleCount);

    if (sampleCount == 0 || !buffer) {
        Trace::Error("No audio data to save");
        cancel();
        return;
    }

    // Generate filename
    fileName_ = generateFileName();

    // Get project path
    SamplePool *pool = SamplePool::GetInstance();
    const char *samplePath = pool->GetSampleLib();

    // Create recorded subfolder if it doesn't exist
    std::string recordedFolder = std::string(samplePath) + "/recorded";
    FileSystem::GetInstance()->MakeDir(recordedFolder.c_str());

    // Build full path in recorded subfolder
    std::string fullPath = recordedFolder + "/" + fileName_;

    Trace::Log("RECORD", "Saving %d samples to: %s", sampleCount, fullPath.c_str());

    // Write WAV file
    WavFileWriter writer(fullPath.c_str());

    // Convert from samples to fixed-point format that WavFileWriter expects
    // Note: WavFileWriter::AddBuffer expects fixed-point samples
    // We need to convert our short samples to fixed-point
    int frameCount = sampleCount / 2; // stereo
    fixed *fixedBuffer = new fixed[sampleCount];

    for (int i = 0; i < sampleCount; i++) {
        // Convert 16-bit signed to fixed-point (scale by 65536)
        fixedBuffer[i] = (fixed)buffer[i] << 16;
    }

    writer.AddBuffer(fixedBuffer, sampleCount);
    writer.Close();

    delete[] fixedBuffer;

    // Import the sample into the pool
    Path sampleFile(fullPath.c_str());
    int sampleID = pool->ImportSample(sampleFile);

    if (sampleID >= 0) {
        // Assign to current instrument
        I_Instrument *instr = viewData_->project_->GetInstrumentBank()->GetInstrument(targetInstrument_);
        if (instr && instr->GetType() == IT_SAMPLE) {
            SampleInstrument *sinstr = (SampleInstrument *)instr;
            sinstr->AssignSample(sampleID);
            Trace::Log("RECORD", "Sample imported and assigned to instrument %02X", targetInstrument_);
        }
    } else {
        Trace::Error("Failed to import recorded sample");
    }

    // Close dialog
    EndModal(0);
}

void RecordSampleDialog::cancel() {
    if (state_ == RS_RECORDING) {
        capture_->StopRecording();
    }
    EndModal(0);
}

std::string RecordSampleDialog::generateFileName() {
    // Generate unique filename: rec_001.wav, rec_002.wav, etc.
    static int counter = 1;
    std::ostringstream oss;
    oss << "rec_" << std::setfill('0') << std::setw(3) << counter++ << ".wav";
    return oss.str();
}

void RecordSampleDialog::drawDeviceSelection() {
    // Full screen device selection
    Clear();

    GUITextProperties props;
    SetColor(CD_NORMAL);

    int y = 1;

    // Title
    DrawString(1, y++, "SELECT AUDIO INPUT DEVICE", props);
    y++;

    // List devices
    for (size_t i = 0; i < availableDevices_.size(); i++) {
        if ((int)i == selectedDeviceIndex_) {
            SetColor(CD_HILITE2);
            DrawString(0, y, ">", props);
        } else {
            SetColor(CD_NORMAL);
        }

        char line[80];
        snprintf(line, sizeof(line), " %s", availableDevices_[i].description.c_str());
        DrawString(1, y++, line, props);
    }

    y++;

    // Controls
    SetColor(CD_HILITE1);
    DrawString(1, y++, "UP/DOWN: Select device", props);
    DrawString(1, y++, "A: Choose device", props);
    DrawString(1, y++, "B: Cancel", props);
}

void RecordSampleDialog::drawRecordingScreen() {
    // Full screen recording UI
    Clear();

    GUITextProperties props;
    SetColor(CD_NORMAL);

    int y = 1;

    // Title and state
    const char *stateText = "";
    switch (state_) {
        case RS_READY:
            stateText = "READY TO RECORD";
            break;
        case RS_RECORDING:
            stateText = "RECORDING";
            break;
        case RS_STOPPED:
            stateText = "RECORDING COMPLETE";
            break;
        case RS_SAVING:
            stateText = "SAVING...";
            break;
        default:
            stateText = "";
            break;
    }
    DrawString(1, y++, stateText, props);
    y++;

    // Recording info
    char buffer[64];

    if (state_ == RS_RECORDING || state_ == RS_STOPPED) {
        float time = (state_ == RS_RECORDING) ? capture_->GetRecordingTime() : elapsedTime_;
        sprintf(buffer, "Time: %.2fs / %ds", time, recordDuration_);
    } else {
        sprintf(buffer, "Duration: %d seconds", recordDuration_);
    }
    DrawString(1, y++, buffer, props);

    sprintf(buffer, "Target: Instrument %02X", targetInstrument_);
    DrawString(1, y++, buffer, props);

    if (!availableDevices_.empty() && selectedDeviceIndex_ < (int)availableDevices_.size()) {
        sprintf(buffer, "Input: %s", availableDevices_[selectedDeviceIndex_].name.c_str());
        DrawString(1, y++, buffer, props);
    }

    y++;

    // Status message (device info, errors, warnings)
    if (!statusMessage_.empty()) {
        SetColor(statusMessage_.find("Error") != std::string::npos ||
                 statusMessage_.find("Cannot") != std::string::npos ? CD_HILITE2 : CD_NORMAL);
        DrawString(1, y++, statusMessage_.c_str(), props);
        SetColor(CD_NORMAL);
        y++;
    }

    // Waveform visualization
    if (state_ == RS_RECORDING) {
        drawWaveform();

        // Show warning if no audio detected
        if (capture_->GetPeakLevel() < 0.001f && capture_->GetRecordingTime() > 2.0f) {
            SetColor(CD_HILITE2);
            DrawString(1, y, "WARNING: No audio detected!", props);
            SetColor(CD_NORMAL);
        }
    }

    y = 18;  // Bottom of screen

    // Controls
    SetColor(CD_HILITE1);
    if (state_ == RS_READY) {
        DrawString(1, y++, "START: Begin", props);
        DrawString(1, y++, "UP/DOWN: Duration", props);
        DrawString(1, y++, "B: Back", props);
    } else if (state_ == RS_RECORDING) {
        DrawString(1, y++, "START: Stop", props);
    } else if (state_ == RS_STOPPED) {
        DrawString(1, y++, "A: Save", props);
        DrawString(1, y++, "B: Cancel", props);
    }
}

void RecordSampleDialog::drawWaveform() {
    GUITextProperties props;

    const short *waveform = capture_->GetWaveformBuffer();
    int waveformSize = capture_->GetWaveformSize();
    float peakLevel = capture_->GetPeakLevel();

    // Draw waveform starting at line 8
    int waveformY = 8;
    int waveformHeight = 8;

    // Draw peak meter
    char peakBar[40];
    int peakWidth = (int)(peakLevel * 30);
    for (int i = 0; i < 30; i++) {
        peakBar[i] = (i < peakWidth) ? '=' : ' ';
    }
    peakBar[30] = '\0';

    SetColor(CD_HILITE2);
    DrawString(1, waveformY, "Level:", props);
    DrawString(8, waveformY, peakBar, props);

    // Draw waveform (simple line-based visualization)
    for (int x = 0; x < waveformSize && x < 40; x++) {
        short sample = waveform[x];
        // Normalize to waveform height
        int amplitude = (int)((float)sample / 32768.0f * waveformHeight);
        int centerY = waveformY + 2 + waveformHeight / 2;
        int sampleY = centerY - amplitude / 2;

        if (sampleY >= waveformY + 2 && sampleY < waveformY + 2 + waveformHeight) {
            SetColor(CD_HILITE1);
            DrawString(x, sampleY, "|", props);
        }
    }
}

void RecordSampleDialog::selectDevice() {
    if (selectedDeviceIndex_ >= 0 && selectedDeviceIndex_ < (int)availableDevices_.size()) {
        capture_->SetCaptureDevice(availableDevices_[selectedDeviceIndex_].name);
        state_ = RS_READY;
        isDirty_ = true;
        Trace::Log("RECORD", "Selected device: %s",
                   availableDevices_[selectedDeviceIndex_].description.c_str());
    }
}
