// Per-instrument EQ built for the device, run under qemu-arm: sines through
// both render paths (the sampler's stereo buffer, the synth's mono tick),
// level measured after the filters settle and compared with the designed
// curve; flat is skipped; voices keep their own filter memory; the shared
// design gives the same result as the master EQ.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/InstrumentEQ.h"
#include "Application/Mixer/MasterEQ.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static int failures = 0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-46s got=%9.3f want=%9.3f %s\n", what, got, want, ok ? "ok" : "WRONG");
	if (!ok) failures++;
}

#define TWO_PI 6.283185307
static const int FRAMES = 512;

static void setKnobs(VariableContainer &c, const int knobs[6]) {
	for (int i = 0; i < 6; i++) c.FindVariable(InstrumentEQ::Ids[i])->SetInt(knobs[i]);
}

// Level change in dB of a sine at freq through one voice of the EQ, on the
// sampler's stereo path (stereo=true) or the synth's mono path
static double gainAt(InstrumentEQ &eq, int channel, double freq, bool stereo) {
	const int rate = Audio::GetInstance()->GetSampleRate();
	eq.ResetVoice(channel);
	fixed buffer[FRAMES * 2];
	double in = 0, out = 0;
	// Wrapped phase: sin() of thousands of radians is not reliable on the
	// device's libm (the app's oscillators wrap their phase too)
	double phase = 0.0, step = TWO_PI * freq / rate;
	for (int block = 0; block < 80; block++) {
		eq.Prepare();
		double blockIn = 0, blockOut = 0;
		if (stereo) {
			for (int i = 0; i < FRAMES; i++) {
				float s = 8000.0f * (float)sin(phase);
				phase += step;
				if (phase > TWO_PI) phase -= TWO_PI;
				buffer[i * 2] = buffer[i * 2 + 1] = fl2fp(s);
				blockIn += (double)s * s;
			}
			eq.ProcessStereo(channel, buffer, FRAMES);
			for (int i = 0; i < FRAMES; i++) {
				double l = fp2fl(buffer[i * 2]), r = fp2fl(buffer[i * 2 + 1]);
				blockOut += (l * l + r * r) / 2.0;
			}
		} else {
			for (int i = 0; i < FRAMES; i++) {
				float s = 0.25f * (float)sin(phase);  // synth voices run at +-1
				phase += step;
				if (phase > TWO_PI) phase -= TWO_PI;
				float y = eq.TickMono(channel, s);
				blockIn += (double)s * s;
				blockOut += (double)y * y;
			}
		}
		if (block >= 20) {  // after the filters settle
			in += blockIn;
			out += blockOut;
		}
	}
	return 10.0 * log10(out / in);
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	AudioSettings settings;
	settings.bufferSize_ = 1024;
	settings.preBufferCount_ = 0;
	Audio::Install(new HarnessAudio(settings));
	const float rate = (float)Audio::GetInstance()->GetSampleRate();

	VariableContainer vars;
	InstrumentEQ eq;
	eq.Create(vars);

	// Defaults: flat, and a flat EQ is skipped entirely
	int now[6];
	for (int i = 0; i < 6; i++) now[i] = vars.FindVariable(InstrumentEQ::Ids[i])->GetInt();
	expect(!memcmp(now, ThreeBandEQ::DefaultParams, sizeof(now)), "new instrument: knobs at the defaults", 0, 0);
	expect(!eq.Prepare(), "flat EQ: skipped (Prepare false)", eq.Prepare(), 0);

	struct Case { const char *name; int knobs[6]; double freqs[3]; };
	static const Case cases[] = {
		{"low shelf +12", {0xFF, 0x70, 0x80, 0x80, 0x80, 0x90}, {40.0, 94.0, 5000.0}},
		{"mid bell -12", {0x80, 0x70, 0x00, 0x80, 0x80, 0x90}, {300.0, 949.0, 6000.0}},
		{"high shelf +12 at 1.5k", {0x80, 0x70, 0x80, 0x80, 0xFF, 0x00}, {200.0, 1500.0, 12000.0}},
		{"all three: bass cut, mid lift, air", {0x30, 0xA0, 0xB0, 0x60, 0xD0, 0xC0}, {60.0, 700.0, 14000.0}},
	};
	for (unsigned c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
		setKnobs(vars, cases[c].knobs);
		expect(eq.Prepare(), "shaped EQ: runs (Prepare true)", eq.Prepare(), 1);
		float coeffs[3][5];
		ThreeBandEQ::Design(cases[c].knobs, rate, coeffs);
		for (int f = 0; f < 3; f++) {
			double freq = cases[c].freqs[f];
			double want = ThreeBandEQ::ResponseDb(coeffs, (float)freq, rate);
			for (int path = 0; path < 2; path++) {
				double got = gainAt(eq, 3, freq, path == 0);
				char what[80];
				sprintf(what, "%s, %s %5.0f Hz (dB)", cases[c].name, path == 0 ? "sampler" : "synth", freq);
				expect(fabs(got - want) < 0.35, what, got, want);
			}
		}
	}
	{
		// The boost really is +12 at the bottom of the low shelf
		const int bass[6] = {0xFF, 0x70, 0x80, 0x80, 0x80, 0x90};
		setKnobs(vars, bass);
		double g = gainAt(eq, 0, 40.0, true);
		expect(g > 10.5 && g < 12.5, "low shelf +12: 40 Hz boosted (dB)", g, 12.0);
		g = gainAt(eq, 0, 5000.0, false);
		expect(fabs(g) < 0.7, "low shelf +12: 5 kHz untouched (dB)", g, 0.0);
	}

	// Voices are independent: ringing on voice 1 leaves voice 2 silent
	{
		const int ring[6] = {0x80, 0x70, 0xFF, 0x80, 0x80, 0x90};
		setKnobs(vars, ring);
		eq.Prepare();
		eq.ResetVoice(1);
		eq.ResetVoice(2);
		fixed a[FRAMES * 2], b[FRAMES * 2];
		memset(a, 0, sizeof(a));
		memset(b, 0, sizeof(b));
		a[0] = a[1] = fl2fp(20000.0f);  // an impulse on voice 1
		eq.ProcessStereo(1, a, FRAMES);
		eq.ProcessStereo(2, b, FRAMES);
		int nonZero = 0;
		for (int i = 0; i < FRAMES * 2; i++) if (b[i] != 0) nonZero++;
		expect(nonZero == 0, "voice 2 untouched by voice 1's ringing", nonZero, 0);
		double tail = fabs(fp2fl(a[20]));
		expect(tail > 1.0, "voice 1 rings (the bell's impulse tail)", tail, 1.0);
	}

	// One design for both: all bands active, the instrument EQ's stereo path
	// gives the master EQ's samples exactly
	{
		const int knobs[6] = {0x30, 0xA0, 0xB0, 0x60, 0xD0, 0xC0};
		setKnobs(vars, knobs);
		eq.Prepare();
		eq.ResetVoice(0);
		MasterEQ master;
		master.SetParams(knobs);
		fixed a[FRAMES * 2], b[FRAMES * 2];
		double phase = 0.0, step = TWO_PI * 1234.0 / rate;
		int diffs = 0;
		for (int block = 0; block < 10; block++) {
			for (int i = 0; i < FRAMES; i++) {
				float s = 12000.0f * (float)sin(phase) + 3000.0f * (float)sin(phase * 7.0);
				phase += step;
				if (phase > TWO_PI) phase -= TWO_PI;
				a[i * 2] = b[i * 2] = fl2fp(s);
				a[i * 2 + 1] = b[i * 2 + 1] = fl2fp(-s * 0.5f);
			}
			eq.ProcessStereo(0, a, FRAMES);
			master.Process(b, FRAMES);
			for (int i = 0; i < FRAMES * 2; i++) if (a[i] != b[i]) diffs++;
		}
		expect(diffs == 0, "instrument EQ == master EQ, same knobs (diffs)", diffs, 0);
	}

	// Back to flat: skipped again
	setKnobs(vars, ThreeBandEQ::DefaultParams);
	expect(!eq.Prepare(), "back to flat: skipped again", eq.Prepare(), 0);

	printf("%s\n", failures ? "FAILED" : "ALL OK");
	fflush(stdout);
	_exit(failures ? 1 : 0);
}
