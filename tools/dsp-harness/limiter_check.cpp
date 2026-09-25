// Master limiter built for the device, run under qemu-arm: hot sines and
// bursts through the real Process() path. Checks that off is untouched,
// the ceiling holds (also on the first peak of a sudden burst: the
// look-ahead), the gain-reduction meter reads what was taken off, the
// look-ahead delay, and the release time.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Mixer/MasterLimiter.h"
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
	printf("%-46s got=%9.3f want=%9.3f %s\n", what, got, want, ok ? "ok" : "WRONG");
	if (!ok) failures++;
}

static const int RATE = 44100;
static const int BLOCK = 512;
#define TWO_PI 6.283185307

// A sine generator with a wrapped phase (sin() of thousands of radians is
// not reliable on the device's libm)
struct Sine {
	double phase, step;
	Sine(double freq) : phase(0.0), step(TWO_PI * freq / RATE) {}
	float next(float amp) {
		float s = amp * (float)sin(phase);
		phase += step;
		if (phase > TWO_PI) phase -= TWO_PI;
		return s;
	}
};

static float peakOf(const fixed *buf, int frames) {
	float p = 0.0f;
	for (int i = 0; i < frames * 2; i++) {
		float a = fabsf(fp2fl(buf[i]));
		if (a > p) p = a;
	}
	return p;
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	AudioSettings settings;
	settings.bufferSize_ = 1024;
	settings.preBufferCount_ = 0;
	Audio::Install(new HarnessAudio(settings));

	fixed buf[BLOCK * 2];
	fixed in[BLOCK * 2];

	// 1. Off (drive 00): every sample passes untouched, no delay
	{
		MasterLimiter lim;
		const int p[4] = {0x00, 0x00, 0x80, 0x00};  // even a -25.5 dB ceiling
		lim.SetParams(p);
		Sine s(1000.0);
		int diffs = 0;
		for (int b = 0; b < 20; b++) {
			for (int i = 0; i < BLOCK; i++) {
				float v = s.next(32000.0f);
				in[i * 2] = buf[i * 2] = fl2fp(v);
				in[i * 2 + 1] = buf[i * 2 + 1] = fl2fp(-v);
			}
			lim.Process(buf, BLOCK);
			for (int i = 0; i < BLOCK * 2; i++) if (buf[i] != in[i]) diffs++;
		}
		expect(diffs == 0, "off: samples changed", diffs, 0);
		expect(lim.GetLatency() == 0, "off: delay (samples)", lim.GetLatency(), 0);
	}

	// 2. On, nothing to limit: the mix comes out exactly, one look-ahead late
	{
		MasterLimiter lim;
		const int p[4] = {0x01, 0xFF, 0x80, 0x00};  // 0 dB drive, 0 dB ceiling, 1 ms
		lim.SetParams(p);
		Sine s(440.0);
		static fixed all[BLOCK * 2 * 8], out[BLOCK * 2 * 8];
		for (int b = 0; b < 8; b++) {
			for (int i = 0; i < BLOCK; i++) {
				float v = s.next(20000.0f);
				buf[i * 2] = buf[i * 2 + 1] = fl2fp(v);
			}
			for (int i = 0; i < BLOCK * 2; i++) all[b * BLOCK * 2 + i] = buf[i];
			lim.Process(buf, BLOCK);
			for (int i = 0; i < BLOCK * 2; i++) out[b * BLOCK * 2 + i] = buf[i];
		}
		int d = lim.GetLatency();
		int wantD = (int)(MasterLimiter::AttackMsFromParam(0x80) * 0.001f * RATE + 0.5f) - 1;
		expect(d == wantD && d >= 43 && d <= 44, "on: look-ahead delay at 1 ms (samples)", d, wantD);
		double worst = 0.0;
		for (int n = d; n < BLOCK * 8; n++) {
			double e = fabs(fp2fl(out[n * 2]) - fp2fl(all[(n - d) * 2]));
			if (e > worst) worst = e;
		}
		expect(worst < 0.01, "on, below ceiling: error vs delayed input", worst, 0.0);
		expect(lim.GetGainReductionDb() < 0.001, "on, below ceiling: GR meter (dB)",
		       lim.GetGainReductionDb(), 0.0);
	}

	// 3. A hot sine driven 12 dB into a -6 dB ceiling
	{
		MasterLimiter lim;
		const int p[4] = {0x79, 0xC3, 0x80, 0x00};  // +12.0 dB, -6.0 dB, 1 ms, auto
		lim.SetParams(p);
		expect(fabs(MasterLimiter::DriveDbFromParam(0x79) - 12.0) < 0.001, "drive 79 (dB)",
		       MasterLimiter::DriveDbFromParam(0x79), 12.0);
		expect(fabs(MasterLimiter::CeilingDbFromParam(0xC3) + 6.0) < 0.001, "ceil C3 (dB)",
		       MasterLimiter::CeilingDbFromParam(0xC3), -6.0);
		const float ceiling = 32767.0f * (float)pow(10.0, -6.0 / 20.0);
		Sine s(1000.0);
		float worst = 0.0f, settled = 0.0f;
		for (int b = 0; b < 60; b++) {
			for (int i = 0; i < BLOCK; i++) {
				float v = s.next(30000.0f);
				buf[i * 2] = buf[i * 2 + 1] = fl2fp(v);
			}
			lim.Process(buf, BLOCK);
			float pk = peakOf(buf, BLOCK);
			if (pk > worst) worst = pk;
			if (b >= 40) settled = pk;
		}
		expect(worst <= ceiling * 1.0001f, "hot sine: loudest sample vs ceiling", worst, ceiling);
		expect(settled > ceiling * 0.93f, "hot sine: settled peak near ceiling", settled, ceiling);
		// 30000 * 4 (12 dB) squeezed to the ceiling
		double want = 20.0 * log10(30000.0 * pow(10.0, 12.0 / 20.0) / ceiling);
		double gr = lim.GetGainReductionDb();
		expect(fabs(gr - want) < 0.6, "hot sine: GR meter (dB)", gr, want);
		double in = lim.GetInputPeakDb();
		double wantIn = 20.0 * log10(30000.0 * pow(10.0, 12.0 / 20.0) / 32767.0);
		expect(fabs(in - wantIn) < 0.2, "hot sine: input meter after drive (dBFS)", in, wantIn);
	}

	// 4. Silence, then a sudden burst ~7 dB over the ceiling: the look-ahead
	// has the gain down before the very first peak comes out
	for (int attack = 0; attack < 3; attack++) {
		static const int attacks[3] = {0x00, 0x80, 0xFF};  // 0.1, 1, 10 ms
		MasterLimiter lim;
		const int p[4] = {0x01, 0xDB, attacks[attack], 0x00};  // 0 dB, -3.6 dB
		lim.SetParams(p);
		const float ceiling = 32767.0f * (float)pow(10.0, MasterLimiter::CeilingDbFromParam(0xDB) / 20.0);
		Sine s(3000.0);
		float worst = 0.0f;
		for (int b = 0; b < 12; b++) {
			for (int i = 0; i < BLOCK; i++) {
				// Full-scale-plus square-ish burst from block 4 (fixed allows
				// up to 2x full scale on the mix bus)
				float v = b < 4 ? 0.0f : s.next(60000.0f * (float)pow(10.0, -3.6 / 20.0) * 0.99f);
				buf[i * 2] = fl2fp(v);
				buf[i * 2 + 1] = fl2fp(v * 0.5f);
			}
			lim.Process(buf, BLOCK);
			float pk = peakOf(buf, BLOCK);
			if (pk > worst) worst = pk;
		}
		char what[64];
		sprintf(what, "burst, attack %.1f ms: loudest vs ceiling",
		        MasterLimiter::AttackMsFromParam(attacks[attack]));
		expect(worst <= ceiling * 1.0001f, what, worst, ceiling);
	}

	// 5. Release: heavy limiting, then a quiet tone. The gain climbs back
	// with the release time (63 ms at 80): measured where 63% of the way
	// back up, on the quiet tone's level
	for (int mode = 0; mode < 2; mode++) {
		MasterLimiter lim;
		const int rel = mode == 0 ? 0x80 : 0x00;
		Sine s(1000.0);
		const float quiet = 2000.0f;
		// 0.5 s of a full-scale tone driven +6.9 dB into a 0 dB ceiling,
		// 0.3 ms look-ahead
		int p2[4] = {0x46, 0xFF, 0x40, rel};
		lim.SetParams(p2);
		for (int b = 0; b < 44; b++) {
			for (int i = 0; i < BLOCK; i++) {
				float v = s.next(32000.0f);
				buf[i * 2] = buf[i * 2 + 1] = fl2fp(v);
			}
			lim.Process(buf, BLOCK);
		}
		double grStart = lim.GetGainReductionDb();
		// Then the quiet tone (well under the ceiling even with the drive)
		const double drive = pow(10.0, MasterLimiter::DriveDbFromParam(0x46) / 20.0);
		double g0 = pow(10.0, -grStart / 20.0);
		double target = 1.0 - (1.0 - g0) / 2.718281828;  // 63% of the way back
		int reached = -1;
		int n = 0;
		for (int b = 0; b < 200 && reached < 0; b++) {
			for (int i = 0; i < BLOCK; i++) {
				float v = s.next(quiet);
				buf[i * 2] = buf[i * 2 + 1] = fl2fp(v);
			}
			lim.Process(buf, BLOCK);
			// level per 1 kHz cycle (44 samples)
			for (int c = 0; c + 44 <= BLOCK && reached < 0; c += 44) {
				float pk = 0.0f;
				for (int i = c; i < c + 44; i++) {
					float a = fabsf(fp2fl(buf[i * 2]));
					if (a > pk) pk = a;
				}
				double g = pk / (quiet * drive);
				// (the first 2 ms still carry the loud tone, look-ahead late)
				if (g >= target && n + c >= 88) reached = n + c;
			}
			n += BLOCK;
		}
		double ms = reached < 0 ? 99999.0 : reached * 1000.0 / RATE;
		printf("  release %s: GR before %.1f dB, 63%% back after %.1f ms\n",
		       mode == 0 ? "80" : "auto", grStart, ms);
		if (mode == 0) {
			expect(grStart > 5.0, "release: heavy limiting before (dB)", grStart, 6.9);
			expect(fabs(ms - 63.2) < 8.0, "release 80: 63% recovery time (ms)", ms, 63.2);
		} else {
			// Auto: 100 ms when barely limiting .. 900 ms when pressing hard;
			// from ~7 dB it starts slow and speeds up as it recovers
			expect(ms > 100.0 && ms < 900.0, "release auto: 63% recovery time (ms)", ms, 400.0);
		}
	}

	printf("%s\n", failures ? "FAILED" : "ALL OK");
	fflush(stdout);
	_exit(failures ? 1 : 0);
}
