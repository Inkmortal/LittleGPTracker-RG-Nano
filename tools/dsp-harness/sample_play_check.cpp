// Sampler play modes and sample editing built for the device, run under
// qemu-arm: a known ramp sample goes through the real SampleInstrument
// voice (forward, reverse, loops, ping-pong, a slice in reverse, the PLAY
// command) and through the sample editor's processes, and the output is
// checked frame by frame for order and direction.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SampleProcessor.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/WavFileWriter.h"
#include "Application/Instruments/CommandList.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include "System/FileSystem/FileSystem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static int failures = 0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-48s got=%9.2f want=%9.2f %s\n", what, got, want, ok ? "ok" : "WRONG");
	if (!ok) failures++;
}

// The test sample: frame k holds (k - 1000) * 16 + 8, a rising ramp that
// is never 0, so every output value names the frame it came from and 0 is
// silence
static const int FRAMES = 2000;
static short ramp[FRAMES];
static double gScale = 0; // output level of one sample unit

static SampleInstrument *instrument;

static void setMarkers(int s, int l, int e) {
	instrument->FindVariable(SIP_START)->SetInt(s);
	instrument->FindVariable(SIP_LOOPSTART)->SetInt(l);
	instrument->FindVariable(SIP_END)->SetInt(e);
}

// Play a note for `count` output frames: the left channel's values
static std::vector<double> render(const char *mode, int count, int note = 60,
                                  FourCC cmd = 0, ushort param = 0) {
	instrument->FindVariable(SIP_LOOPMODE)->SetString(mode);
	static fixed buffer[256 * 2];
	std::vector<double> out;
	instrument->Start(0, note, true);
	if (cmd) instrument->ProcessCommand(0, cmd, param);
	while ((int)out.size() < count) {
		bool sound = instrument->Render(0, buffer, 256, true);
		for (int i = 0; i < 256 && (int)out.size() < count; i++) {
			out.push_back(sound ? fp2fl(buffer[i * 2]) : 0.0);
		}
	}
	return out;
}

// ... as source frames (-1 = silence)
static std::vector<int> play(const char *mode, int count, int note = 60,
                             FourCC cmd = 0, ushort param = 0) {
	std::vector<double> v = render(mode, count, note, cmd, param);
	std::vector<int> frames;
	for (size_t i = 0; i < v.size(); i++) {
		if (fabs(v[i]) < gScale * 4) {
			frames.push_back(-1);
		} else {
			frames.push_back((int)floor((v[i] / gScale - 8.0) / 16.0 + 1000.0 + 0.5));
		}
	}
	return frames;
}

// n frames starting at `from`, stepping by `step`, all as expected
static bool runOf(const std::vector<int> &f, int at, int from, int step, int n) {
	for (int i = 0; i < n; i++) {
		if (at + i >= (int)f.size() || f[at + i] != from + i * step) {
			printf("   output %d: frame %d, want %d\n", at + i, at + i < (int)f.size() ? f[at + i] : -2, from + i * step);
			return false;
		}
	}
	return true;
}

static int firstSilent(const std::vector<int> &f) {
	for (size_t i = 0; i < f.size(); i++)
		if (f[i] == -1) return (int)i;
	return -1;
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	Path::SetAlias("bin", ".");
	Path::SetAlias("root", ".");
	mkdir("/tmp/sample_play_check", 0777);
	Path::SetAlias("samples", "/tmp/sample_play_check");
	AudioSettings settings;
	settings.bufferSize_ = 1024;
	settings.preBufferCount_ = 0;
	Audio::Install(new HarnessAudio(settings));
	int rate = Audio::GetInstance()->GetSampleRate();

	// The ramp, written with the app's WAV writer at the output rate (so a
	// note at the root plays one frame per output frame) and loaded back
	for (int k = 0; k < FRAMES; k++) ramp[k] = (short)((k - 1000) * 16 + 8);
	remove("/tmp/sample_play_check/ramp.wav");
	remove("/tmp/sample_play_check/ramp_rev.wav");
	{
		WavFileWriter w("samples:ramp.wav", 1, rate);
		expect(w.IsOpen(), "writer opens samples:ramp.wav", w.IsOpen(), 1);
		w.AddFrames(ramp, FRAMES);
		w.Close();
	}
	SamplePool *pool = SamplePool::GetInstance();
	int index = pool->AddProjectSample("ramp.wav");
	expect(index >= 0, "ramp.wav loads into the pool", index, 0);
	SoundSource *src = pool->GetSource(index);
	expect(src && src->GetSize(-1) == FRAMES, "ramp.wav has every frame", src ? src->GetSize(-1) : -1, FRAMES);
	expect(src && ((short *)src->GetSampleBuffer(-1))[1234] == ramp[1234], "ramp.wav frame 1234 reads back", src ? ((short *)src->GetSampleBuffer(-1))[1234] : 0, ramp[1234]);

	instrument = new SampleInstrument();
	instrument->Init();
	instrument->AssignSample(index);
	expect(instrument->GetSampleSize(0) == FRAMES, "instrument sees the sample", instrument->GetSampleSize(0), FRAMES);

	const int S = 200, L = 800, E = 1400;
	setMarkers(S, L, E);

	// Calibrate the output level on the forward note's first frame (S)
	{
		std::vector<double> v = render("forward", 1);
		gScale = v[0] / ramp[S];
		expect(gScale > 0.1 && gScale < 1.0, "output gain (volume 80, centre pan)", gScale, 0.35);
	}

	// FORWARD: S .. E-2, then silence
	std::vector<int> f = play("forward", 1600);
	expect(runOf(f, 0, S, 1, E - 1 - S), "forward: S up to just before E", f[0], S);
	expect(firstSilent(f) == E - 1 - S, "forward: stops at E", firstSilent(f), E - 1 - S);

	// REVERSE: E-1 down to S, then silence
	f = play("reverse", 1600);
	expect(runOf(f, 0, E - 1, -1, E - S), "reverse: E-1 down to S", f[0], E - 1);
	expect(firstSilent(f) == E - S, "reverse: stops after S", firstSilent(f), E - S);

	// LOOP: S .. E-2 once, then L .. E-2 again and again
	f = play("loop", 3000);
	int pass = E - 1 - S, loopLen = E - 1 - L;
	expect(runOf(f, 0, S, 1, pass), "loop: first pass from S", f[0], S);
	expect(runOf(f, pass, L, 1, loopLen) && runOf(f, pass + loopLen, L, 1, loopLen),
	       "loop: then L..E twice", f[pass], L);

	// REV-LOOP: E-1 down to L, again and again (S not used)
	f = play("rev-loop", 2000);
	expect(runOf(f, 0, E - 1, -1, E - L) && runOf(f, E - L, E - 1, -1, E - L),
	       "rev-loop: E-1 down to L, twice", f[E - L], E - 1);

	// PINGPONG: S up to E-1, down to L, up again
	f = play("pingpong", 3000);
	int up = E - S;          // S .. E-1
	int down = E - 1 - L;    // E-2 .. L
	expect(runOf(f, 0, S, 1, up), "pingpong: S up to E-1", f[up - 1], E - 1);
	expect(runOf(f, up, E - 2, -1, down), "pingpong: turns, down to L", f[up + down - 1], L);
	expect(runOf(f, up + down, L + 1, 1, 50), "pingpong: turns at L, up again", f[up + down], L + 1);

	// REV-PINGPONG: E-1 down to L, up to E-1, down again
	f = play("rev-pingpong", 3000);
	expect(runOf(f, 0, E - 1, -1, E - L), "rev-pingpong: E-1 down to L", f[E - L - 1], L);
	expect(runOf(f, E - L, L + 1, 1, E - 1 - L), "rev-pingpong: up to E-1", f[2 * (E - L) - 2], E - 1);
	expect(runOf(f, 2 * (E - L) - 1, E - 2, -1, 50), "rev-pingpong: and down again", f[2 * (E - L) - 1], E - 2);

	// An octave up travels two frames per output frame, still backwards
	f = play("reverse", 700, 72);
	expect(runOf(f, 0, E - 1, -2, (E - S) / 2), "reverse an octave up: every 2nd frame", f[1], E - 3);

	// Slices: 4 slices of E; slice 1 (note 1) reverse plays its own frames
	instrument->FindVariable(SIP_SLICES)->SetInt(4);
	f = play("reverse", 400, 1);
	int slice = E / 4;
	expect(runOf(f, 0, 2 * slice - 1, -1, slice), "slice 1 reverse: its end down to its start", f[0], 2 * slice - 1);
	expect(firstSilent(f) == slice, "slice 1 reverse: stops at its start", firstSilent(f), slice);
	instrument->FindVariable(SIP_SLICES)->SetInt(1);

	// PLAY command on the note's own step: forward instrument, reverse note
	f = play("forward", 1300, 60, I_CMD_PLAY, 0x0001);
	expect(runOf(f, 0, E - 1, -1, E - S), "PLAY 0001 on the note: plays it reverse", f[0], E - 1);
	expect(!strcmp(instrument->FindVariable(SIP_LOOPMODE)->GetString(), "forward"),
	       "PLAY leaves the instrument's mode alone", 1, 1);

	// Old songs: saved names still mean the same modes
	expect(!strcmp(SampleInstrument::CanonicalLoopModeName("none"), "forward"), "old 'none' = forward", 1, 1);
	expect(!strcmp(SampleInstrument::CanonicalLoopModeName("ping pong"), "pingpong"), "old 'ping pong' = pingpong", 1, 1);
	expect(!strcmp(SampleInstrument::CanonicalLoopModeName("oscillator"), "osc"), "old 'oscillator' = osc", 1, 1);
	expect(!strcmp(SampleInstrument::CanonicalLoopModeName("looper sync"), "loop-sync"), "old 'looper sync' = loop-sync", 1, 1);

	// Old songs that reversed by putting E before S still play backwards
	setMarkers(1400, 1000, 200);
	f = play("forward", 1300);
	expect(runOf(f, 0, 1400, -1, 1000), "old E-before-S song: plays backwards", f[0], 1400);
	setMarkers(S, L, E);

	// ---- Sample editing: the processes on the same ramp ----
	SampleEdit e;
	bool ok = SampleProcessor::Plan(SEO_REVERSE, ramp, FRAMES, 1, rate, S, L, E, e);
	bool rev = ok;
	for (int i = 0; i < FRAMES && rev; i++) {
		int want = (i >= S && i < E) ? ramp[S + E - 1 - i] : ramp[i];
		if (SampleProcessor::Value(e, i, 0) != want) rev = false;
	}
	expect(rev, "edit reverse: S..E backwards, rest untouched", rev, 1);

	ok = SampleProcessor::Plan(SEO_CROP, ramp, FRAMES, 1, rate, S, L, E, e);
	expect(ok && e.outFrames == E - S && SampleProcessor::Value(e, 0, 0) == ramp[S] &&
	       SampleProcessor::Value(e, e.outFrames - 1, 0) == ramp[E - 1],
	       "edit crop: exactly S..E", e.outFrames, E - S);
	expect(e.newStart == 0 && e.newLoop == L - S && e.newEnd == E - S, "edit crop: L moves with the sound", e.newLoop, L - S);

	ok = SampleProcessor::Plan(SEO_NORMALIZE, ramp, FRAMES, 1, rate, S, L, E, e);
	int peak = 0;
	for (int i = S; i < E; i++) {
		int v = abs(SampleProcessor::Value(e, i, 0));
		if (v > peak) peak = v;
	}
	expect(ok && peak == 32767, "edit normalize: S..E peaks at full scale", peak, 32767);

	ok = SampleProcessor::Plan(SEO_FADE_IN, ramp, FRAMES, 1, rate, S, L, E, e);
	int mid = (S + E - 1) / 2;
	expect(ok && SampleProcessor::Value(e, S, 0) == 0 && SampleProcessor::Value(e, E - 1, 0) == ramp[E - 1] &&
	       abs(SampleProcessor::Value(e, mid, 0) - ramp[mid] / 2) <= 16,
	       "edit fade in: 0 at S, half way, full at E", SampleProcessor::Value(e, mid, 0), ramp[mid] / 2);

	ok = SampleProcessor::Plan(SEO_FADE_OUT, ramp, FRAMES, 1, rate, S, L, E, e);
	expect(ok && SampleProcessor::Value(e, S, 0) == ramp[S] && SampleProcessor::Value(e, E - 1, 0) == 0,
	       "edit fade out: full at S, 0 at E", SampleProcessor::Value(e, E - 1, 0), 0);

	// Trim: silence either side of a burst
	static short burst[FRAMES];
	memset(burst, 0, sizeof(burst));
	for (int i = 500; i < 900; i++) burst[i] = (short)(i % 2 ? 8000 : -8000);
	ok = SampleProcessor::Plan(SEO_TRIM, burst, FRAMES, 1, rate, 0, 0, FRAMES, e);
	int lead = rate / 500, tail = rate / 100;
	expect(ok && e.offset == 500 - lead && e.outFrames == (900 + tail) - (500 - lead),
	       "edit trim: keeps the burst (+2 ms, +10 ms)", e.outFrames, (900 + tail) - (500 - lead));

	// The written file reads back as the edit
	ok = SampleProcessor::Plan(SEO_REVERSE, ramp, FRAMES, 1, rate, S, L, E, e);
	std::string name = SampleProcessor::OutputName("ramp.wav", SEO_REVERSE);
	expect(name == "ramp_rev.wav", "edit file name: ramp_rev.wav", name == "ramp_rev.wav", 1);
	std::string path = std::string("samples:") + name;
	SampleProcessor::Write(e, path.c_str());
	int revIndex = pool->AddProjectSample(name.c_str());
	SoundSource *revSrc = revIndex >= 0 ? pool->GetSource(revIndex) : 0;
	short *revData = revSrc ? (short *)revSrc->GetSampleBuffer(-1) : 0;
	expect(revData && revSrc->GetSize(-1) == FRAMES && revData[S] == ramp[E - 1] && revData[E - 1] == ramp[S] &&
	       revData[0] == ramp[0], "edit file: written reverse reads back", revData ? revData[S] : 0, ramp[E - 1]);
	expect(SampleProcessor::OutputName("ramp.wav", SEO_REVERSE) == "ramp_rev2.wav", "edit file name: next is ramp_rev2.wav", 1, 1);

	// The instrument takes it with its markers
	instrument->ReplaceSample(revIndex, S, L, E);
	f = play("forward", 10);
	expect(runOf(f, 0, E - 1, -1, 10), "instrument plays the reversed file forward", f[0], E - 1);

	remove("/tmp/sample_play_check/ramp.wav");
	remove("/tmp/sample_play_check/ramp_rev.wav");
	if (failures) printf("FAILED: %d\n", failures);
	else printf("sample play ok\n");
	return failures ? 1 : 0;
}
