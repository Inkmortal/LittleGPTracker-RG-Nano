#ifndef _THREE_BAND_EQ_H_
#define _THREE_BAND_EQ_H_

// The three-band EQ shared by the master EQ and every instrument's EQ:
// a low shelf, a mid bell and a high shelf, each with a gain and a
// frequency knob (params: low gain, low freq, mid gain, mid freq, high
// gain, high freq; 0..FF, gain 80 = flat). Knob curves, biquad design
// (RBJ audio EQ cookbook) and the response used to draw the curve.
class ThreeBandEQ {
public:
	static const int DefaultParams[6];

	// Knob curves
	static float GainDbFromParam(int value);       // 80 = 0 dB, +-12 dB
	static float LowFreqFromParam(int value);      // 30 .. 400 Hz
	static float MidFreqFromParam(int value);      // 150 .. 6000 Hz
	static float HighFreqFromParam(int value);     // 1.5 .. 16 kHz

	// A band does nothing at 0 dB (its filter is the identity)
	static bool BandFlat(const int params[6], int band) { return params[band * 2] == 0x80; }
	static bool IsFlat(const int params[6]) {
		return BandFlat(params, 0) && BandFlat(params, 1) && BandFlat(params, 2);
	}

	// The three filters for these knob values, as biquad coefficients
	// (b0 b1 b2 a1 a2, a0 normalized), for the audio and for the drawing
	static void Design(const int params[6], float sampleRate, float coeffs[3][5]);
	// Response of that design at freq, in dB
	static float ResponseDb(const float coeffs[3][5], float freq, float sampleRate);

	// One sample through one band: transposed direct form II, double state
	// (float state loses low-frequency precision)
	static inline double Step(const float *c, double *z, double x) {
		double y = c[0] * x + z[0];
		z[0] = c[1] * x - c[3] * y + z[1];
		z[1] = c[2] * x - c[4] * y;
		return y;
	}
};

#endif
