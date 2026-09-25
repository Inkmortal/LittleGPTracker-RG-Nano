#ifndef _MASTER_EQ_H_
#define _MASTER_EQ_H_

#include "Foundation/T_Singleton.h"
#include "Services/Audio/AudioInsert.h"
#include "ThreeBandEQ.h"

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

	// Knob curves and design live in ThreeBandEQ (shared with the
	// instruments' EQ); kept here for the EQ screen and the checks
	static float GainDbFromParam(int value) { return ThreeBandEQ::GainDbFromParam(value); }
	static float LowFreqFromParam(int value) { return ThreeBandEQ::LowFreqFromParam(value); }
	static float MidFreqFromParam(int value) { return ThreeBandEQ::MidFreqFromParam(value); }
	static float HighFreqFromParam(int value) { return ThreeBandEQ::HighFreqFromParam(value); }
	static void Design(const int params[6], float sampleRate, float coeffs[3][5]) {
		ThreeBandEQ::Design(params, sampleRate, coeffs);
	}
	static float ResponseDb(const float coeffs[3][5], float freq, float sampleRate) {
		return ThreeBandEQ::ResponseDb(coeffs, freq, sampleRate);
	}

private:
	void update();

	Project *project_;
	int params_[6];      // the knob values the coefficients were made from
	int fixed_[6];       // used when there is no project
	bool flat_;
	float coeffs_[3][5];
	double state_[3][2][2]; // [band][channel][z1,z2]
};

#endif
