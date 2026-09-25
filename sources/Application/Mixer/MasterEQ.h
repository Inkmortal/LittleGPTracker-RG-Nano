#ifndef _MASTER_EQ_H_
#define _MASTER_EQ_H_

#include "Foundation/T_Singleton.h"
#include "Services/Audio/AudioInsert.h"

class Project;

// Three-band EQ on the whole mix (low shelf, mid peak, high shelf), set on
// the EQ screen. Flat settings are skipped entirely.
class MasterEQ : public AudioInsert, public T_Singleton<MasterEQ> {
public:
	MasterEQ();
	void SetProject(Project *project);
	// Without a project (the device checks): use these knob values
	void SetParams(const int params[6]);
	virtual void Process(fixed *buffer, int frames);

	// Knob curves, shared with the EQ screen
	static float GainDbFromParam(int value);       // 80 = 0 dB, +-12 dB
	static float LowFreqFromParam(int value);      // 30 .. 400 Hz
	static float MidFreqFromParam(int value);      // 150 .. 6000 Hz
	static float HighFreqFromParam(int value);     // 1.5 .. 16 kHz

	// The three filters for these knob values, as biquad coefficients
	// (b0 b1 b2 a1 a2, a0 normalized), for the audio and for the drawing
	static void Design(const int params[6], float sampleRate, float coeffs[3][5]);
	// Response of that design at freq, in dB
	static float ResponseDb(const float coeffs[3][5], float freq, float sampleRate);

private:
	void update();

	Project *project_;
	int params_[6];      // the knob values the coefficients were made from
	int fixed_[6];       // used when there is no project
	bool flat_;
	float coeffs_[3][5];
	double state_[3][2][2]; // [band][channel][z1,z2] (transposed direct form II,
	                        // double: float state loses low-frequency precision)
};

#endif
