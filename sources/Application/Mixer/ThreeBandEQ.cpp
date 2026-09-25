#include "ThreeBandEQ.h"
#include <math.h>

const int ThreeBandEQ::DefaultParams[6] = {0x80, 0x70, 0x80, 0x80, 0x80, 0x90};

float ThreeBandEQ::GainDbFromParam(int value) {
	if (value < 0) value = 0;
	if (value > 255) value = 255;
	return (value - 128) * 12.0f / 128.0f;
}

static float logSweep(int value, float low, float high) {
	if (value < 0) value = 0;
	if (value > 255) value = 255;
	return low * (float)pow(high / low, value / 255.0);
}

float ThreeBandEQ::LowFreqFromParam(int value) { return logSweep(value, 30.0f, 400.0f); }
float ThreeBandEQ::MidFreqFromParam(int value) { return logSweep(value, 150.0f, 6000.0f); }
float ThreeBandEQ::HighFreqFromParam(int value) { return logSweep(value, 1500.0f, 16000.0f); }

// RBJ audio EQ cookbook: low shelf / peaking / high shelf
static void shelf(bool high, float gainDb, float freq, float rate, float *c) {
	float A = (float)pow(10.0, gainDb / 40.0);
	float w = 2.0f * 3.14159265f * freq / rate;
	float cw = (float)cos(w), sw = (float)sin(w);
	float alpha = sw / 2.0f * (float)sqrt(2.0);  // slope 1
	float sa = 2.0f * (float)sqrt(A) * alpha;
	float b0, b1, b2, a0, a1, a2;
	if (!high) {
		b0 = A * ((A + 1) - (A - 1) * cw + sa);
		b1 = 2 * A * ((A - 1) - (A + 1) * cw);
		b2 = A * ((A + 1) - (A - 1) * cw - sa);
		a0 = (A + 1) + (A - 1) * cw + sa;
		a1 = -2 * ((A - 1) + (A + 1) * cw);
		a2 = (A + 1) + (A - 1) * cw - sa;
	} else {
		b0 = A * ((A + 1) + (A - 1) * cw + sa);
		b1 = -2 * A * ((A - 1) + (A + 1) * cw);
		b2 = A * ((A + 1) + (A - 1) * cw - sa);
		a0 = (A + 1) - (A - 1) * cw + sa;
		a1 = 2 * ((A - 1) - (A + 1) * cw);
		a2 = (A + 1) - (A - 1) * cw - sa;
	}
	c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b2 / a0; c[3] = a1 / a0; c[4] = a2 / a0;
}

static void peak(float gainDb, float freq, float rate, float *c) {
	float A = (float)pow(10.0, gainDb / 40.0);
	float w = 2.0f * 3.14159265f * freq / rate;
	float alpha = (float)sin(w) / (2.0f * 0.9f);  // Q 0.9: a broad, musical bell
	float cw = (float)cos(w);
	float a0 = 1 + alpha / A;
	c[0] = (1 + alpha * A) / a0;
	c[1] = (-2 * cw) / a0;
	c[2] = (1 - alpha * A) / a0;
	c[3] = (-2 * cw) / a0;
	c[4] = (1 - alpha / A) / a0;
}

void ThreeBandEQ::Design(const int params[6], float rate, float coeffs[3][5]) {
	float nyquistSafe = rate * 0.45f;
	float lowF = LowFreqFromParam(params[1]);
	float midF = MidFreqFromParam(params[3]);
	float highF = HighFreqFromParam(params[5]);
	if (highF > nyquistSafe) highF = nyquistSafe;
	shelf(false, GainDbFromParam(params[0]), lowF, rate, coeffs[0]);
	peak(GainDbFromParam(params[2]), midF, rate, coeffs[1]);
	shelf(true, GainDbFromParam(params[4]), highF, rate, coeffs[2]);
}

float ThreeBandEQ::ResponseDb(const float coeffs[3][5], float freq, float rate) {
	float w = 2.0f * 3.14159265f * freq / rate;
	float total = 0.0f;
	for (int b = 0; b < 3; b++) {
		const float *c = coeffs[b];
		// |H(e^jw)| of b0 + b1 z^-1 + b2 z^-2 over 1 + a1 z^-1 + a2 z^-2
		float nr = c[0] + c[1] * (float)cos(w) + c[2] * (float)cos(2 * w);
		float ni = -c[1] * (float)sin(w) - c[2] * (float)sin(2 * w);
		float dr = 1 + c[3] * (float)cos(w) + c[4] * (float)cos(2 * w);
		float di = -c[3] * (float)sin(w) - c[4] * (float)sin(2 * w);
		float mag = (float)sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
		total += 20.0f * (float)log10(mag > 1e-6f ? mag : 1e-6f);
	}
	return total;
}
