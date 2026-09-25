// Master EQ built for the device, run under qemu-arm: sines through the
// real Process() path, level measured after the filters settle.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Mixer/MasterEQ.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static int failures = 0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-44s got=%9.3f want=%9.3f %s\n", what, got, want, ok ? "ok" : "WRONG");
	if (!ok) failures++;
}

// Level change in dB of a sine at freq through the EQ with these knobs
static double gainAt(const int knobs[6], double freq) {
	MasterEQ eq;
	eq.SetParams(knobs);
	const int rate = Audio::GetInstance()->GetSampleRate();
	const int frames = 512;
	fixed buffer[frames * 2];
	double in = 0, out = 0;
	long n = 0;
	// Wrapped phase: sin() of thousands of radians is not reliable on the
	// device's libm (the app's oscillators wrap their phase too)
	double phase = 0.0, step = 6.283185307 * freq / rate;
	for (int block = 0; block < 80; block++) {
		for (int i = 0; i < frames; i++, n++) {
			float s = 8000.0f * (float)sin(phase);
			phase += step;
			if (phase > 6.283185307) phase -= 6.283185307;
			buffer[i * 2] = buffer[i * 2 + 1] = fl2fp(s);
		}
		double blockIn = 0;
		for (int i = 0; i < frames; i++) blockIn += fp2fl(buffer[i * 2]) * fp2fl(buffer[i * 2]);
		eq.Process(buffer, frames);
		if (block >= 20) {  // after the filters settle
			in += blockIn;
			for (int i = 0; i < frames; i++) out += fp2fl(buffer[i * 2]) * fp2fl(buffer[i * 2]);
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

	const int flat[6] = {0x80, 0x70, 0x80, 0x80, 0x80, 0x90};
	double g = gainAt(flat, 1000.0);
	expect(fabs(g) < 0.01, "flat EQ leaves the sound alone (dB)", g, 0.0);

	const int bass[6] = {0xFF, 0x70, 0x80, 0x80, 0x80, 0x90};  // low +12 dB at ~94 Hz
	g = gainAt(bass, 40.0);
	expect(g > 10.5 && g < 12.5, "low shelf +12: 40 Hz boosted (dB)", g, 12.0);
	g = gainAt(bass, 5000.0);
	expect(fabs(g) < 0.7, "low shelf +12: 5 kHz untouched (dB)", g, 0.0);

	const int mid[6] = {0x80, 0x70, 0x00, 0x80, 0x80, 0x90};  // mid -12 dB at ~950 Hz
	{
		// The designed curve itself (no processing): checks the maths libm
		float c[3][5];
		MasterEQ::Design(mid, 44100.0f, c);
		double d = MasterEQ::ResponseDb(c, MasterEQ::MidFreqFromParam(0x80), 44100.0f);
		expect(d < -11.5 && d > -12.5, "mid bell -12: designed response (dB)", d, -12.0);
		const double sweep[6] = {300, 600, 949, 1500, 3000, 6000};
		for (int i = 0; i < 6; i++) {
			printf("  %5.0f Hz: designed %6.2f dB, measured %6.2f dB\n", sweep[i],
			       MasterEQ::ResponseDb(c, (float)sweep[i], 44100.0f), gainAt(mid, sweep[i]));
		}
	}
	g = gainAt(mid, MasterEQ::MidFreqFromParam(0x80));
	expect(g < -10.5 && g > -12.5, "mid bell -12 at its centre (dB)", g, -12.0);
	g = gainAt(mid, 60.0);
	expect(fabs(g) < 1.0, "mid bell -12: 60 Hz untouched (dB)", g, 0.0);

	const int treble[6] = {0x80, 0x70, 0x80, 0x80, 0xFF, 0x90};  // high +12 dB above ~5.7 kHz
	g = gainAt(treble, 14000.0);
	expect(g > 10.0 && g < 12.5, "high shelf +12: 14 kHz boosted (dB)", g, 12.0);
	g = gainAt(treble, 200.0);
	expect(fabs(g) < 0.7, "high shelf +12: 200 Hz untouched (dB)", g, 0.0);

	printf("%s\n", failures ? "FAILED" : "ALL OK");
	fflush(stdout);
	_exit(failures ? 1 : 0);
}
