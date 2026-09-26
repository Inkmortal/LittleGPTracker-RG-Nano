// HyperSynth's saws on the device build (qemu-arm): the four-at-a-time
// NEON saw on a 32-bit integer phase against the saw it replaced (a float
// phase, one sample at a time), both run here side by side:
//  - same waveform: started at the same phase, the two agree to float
//    rounding (the PolyBLEP correction is the same polynomial)
//  - same aliasing: at 1 kHz every harmonic lands on an exact frequency
//    and every folded (aliased) one between them; the energy of the folded
//    ones against the harmonics is measured for both
//  - pitch at least as exact: after 20 s each is compared with an ideal
//    double-precision phase
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/SynthEngines.h"
#include <math.h>
#include <stdio.h>
#include <vector>
#include <unistd.h>

static const double RATE = 44100.0;
static int failures = 0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-52s got=%12.6f want=%12.6f %s\n", what, got, want, ok ? "ok" : "WRONG");
	fflush(stdout);
	if (!ok) failures++;
}

// The saw as it was: float phase, one sample at a time
static float oldBlep(float t, float dt) {
	if (dt <= 0.0f) return 0.0f;
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.0f;
	} else if (t > 1.0f - dt) {
		t = (t - 1.0f) / dt;
		return t * t + t + t + 1.0f;
	}
	return 0.0f;
}

static float oldSaw(float &ph, float inc) {
	float t = ph;
	float s = 2.0f * t - 1.0f - oldBlep(t, inc);
	t += inc;
	if (t >= 1.0f) t -= 1.0f;
	ph = t;
	return s;
}

// The saw now, through the engine's block renderer (16-sample blocks)
static void newSaw(unsigned int &ph, float inc, float *out, int n) {
	for (int k = 0; k < n; k += 16) {
		int m = n - k < 16 ? n - k : 16;
		hyperSawRun(ph, inc, 1.0f, out + k, m, true);
	}
}

// Amplitude at hz (Hann window, a full sine = 1)
static double amplitudeAt(const std::vector<float> &x, double hz) {
	size_t n = x.size();
	double w = 2.0 * M_PI * hz / RATE;
	double re = 0, im = 0, winSum = 0;
	for (size_t i = 0; i < n; i++) {
		double win = 0.5 - 0.5 * cos(2.0 * M_PI * i / (n - 1));
		winSum += win;
		re += x[i] * win * cos(w * i);
		im += x[i] * win * sin(w * i);
	}
	return 2.0 * sqrt(re * re + im * im) / winSum;
}

// Folded (aliased) energy against harmonic energy, dB, of a 1 kHz saw
static double aliasDb(const std::vector<float> &x) {
	double harm = 0, alias = 0;
	for (int m = 1; m <= 22; m++) {
		double a = amplitudeAt(x, 1000.0 * m);
		harm += a * a;
	}
	for (int m = 0; m <= 21; m++) {
		double a = amplitudeAt(x, 1000.0 * m + 100.0);
		alias += a * a;
	}
	return 10.0 * log10(alias / harm);
}

int main() {
	SynthEnginesInit();
	const float inc = (float)(1000.0 / RATE);

	// Same waveform from the same start
	{
		const int n = 4410;
		float ph = 0.25f;
		unsigned int u = 0x40000000u;
		std::vector<float> a(n), b(n);
		for (int i = 0; i < n; i++) a[i] = oldSaw(ph, inc);
		newSaw(u, inc, &b[0], n);
		double worst = 0, sum = 0;
		for (int i = 0; i < n; i++) {
			double d = fabs(a[i] - b[i]);
			if (d > worst) worst = d;
			sum += d * d;
		}
		// the float phase drifts by a few 1e-7 in 0.1 s; near a jump that
		// moves the correction a little
		double rmsDb = 20.0 * log10(sqrt(sum / n) + 1e-20);
		printf("difference over 0.1 s: RMS %.1f dB (of a full-scale saw), largest %.5f\n", rmsDb,
		       worst);
		expect(rmsDb < -60.0, "same saw over 0.1 s (RMS difference, dB)", rmsDb, -60.0);
	}

	// Same aliasing
	{
		const int n = 44100;
		float ph = 0.0f;
		unsigned int u = 0;
		std::vector<float> a(n), b(n);
		for (int i = 0; i < n; i++) a[i] = oldSaw(ph, inc);
		newSaw(u, inc, &b[0], n);
		double oldAlias = aliasDb(a);
		double newAlias = aliasDb(b);
		printf("folded energy against harmonics: before %.2f dB, now %.2f dB\n", oldAlias, newAlias);
		expect(newAlias <= oldAlias + 0.1, "aliasing not worse (dB, now - before)", newAlias - oldAlias, 0.0);
	}

	// Pitch: phase after 20 s against an ideal double-precision phase
	for (int f = 0; f < 3; f++) {
		static const double freqs[3] = {55.0, 440.0, 3520.0};
		float inc2 = (float)(freqs[f] / RATE);
		const int n = (int)(20 * RATE);
		float ph = 0.0f;
		unsigned int u = 0;
		std::vector<float> buf(16);
		for (int i = 0; i < n; i++) oldSaw(ph, inc2);
		for (int i = 0; i < n; i += 16) newSaw(u, inc2, &buf[0], 16);
		double ideal = fmod((double)inc2 * n, 1.0);   // the step itself is the float value
		double oldErr = fabs(ph - ideal);
		if (oldErr > 0.5) oldErr = 1.0 - oldErr;
		double newErr = fabs(u / 4294967296.0 - ideal);
		if (newErr > 0.5) newErr = 1.0 - newErr;
		char what[80];
		snprintf(what, sizeof(what), "%g Hz: phase error after 20 s (cycles), before %.2e", freqs[f], oldErr);
		expect(newErr <= oldErr + 1e-6, what, newErr, oldErr);
	}

	printf("%s\n", failures ? "FAILED" : "ALL OK");
	fflush(stdout);
	// no global destructors: the app objects linked in are not set up here
	_exit(failures ? 1 : 0);
}
