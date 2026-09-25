#include "MasterEQ.h"
#include "Application/Model/Project.h"
#include "Services/Audio/Audio.h"
#include <math.h>
#include <string.h>

static const FourCC eqVars[6] = {VAR_EQ_LOW_GAIN, VAR_EQ_LOW_FREQ, VAR_EQ_MID_GAIN,
                                 VAR_EQ_MID_FREQ, VAR_EQ_HIGH_GAIN, VAR_EQ_HIGH_FREQ};

MasterEQ::MasterEQ() : project_(0), flat_(true) {
	for (int i = 0; i < 6; i++) {
		params_[i] = -1;
		fixed_[i] = ThreeBandEQ::DefaultParams[i];
	}
	memset(coeffs_, 0, sizeof(coeffs_));
	memset(state_, 0, sizeof(state_));
}

void MasterEQ::SetParams(const int params[6]) {
	memcpy(fixed_, params, sizeof(fixed_));
}

void MasterEQ::SetProject(Project *project) {
	project_ = project;
	memset(state_, 0, sizeof(state_));
	for (int i = 0; i < 6; i++) params_[i] = -1;
}

void MasterEQ::update() {
	int now[6];
	bool changed = false;
	for (int i = 0; i < 6; i++) {
		Variable *v = project_ ? project_->FindVariable(eqVars[i]) : 0;
		now[i] = v ? v->GetInt() : fixed_[i];
		if (now[i] != params_[i]) changed = true;
	}
	if (!changed) return;
	memcpy(params_, now, sizeof(params_));
	flat_ = ThreeBandEQ::IsFlat(now);
	float rate = (float)Audio::GetInstance()->GetSampleRate();
	ThreeBandEQ::Design(params_, rate, coeffs_);
}

void MasterEQ::Process(fixed *buffer, int frames) {
	update();
	if (flat_) {
		// All gains at 0 dB: nothing to do; forget the filter memory so a
		// later boost starts clean
		memset(state_, 0, sizeof(state_));
		return;
	}
	for (int i = 0; i < frames; i++) {
		for (int ch = 0; ch < 2; ch++) {
			double x = fp2fl(buffer[i * 2 + ch]);
			for (int b = 0; b < 3; b++) {
				x = ThreeBandEQ::Step(coeffs_[b], state_[b][ch], x);
			}
			// A boost on a loud mix must not wrap the fixed-point sample;
			// the master clipper after this handles the rest
			if (x > 65000.0) x = 65000.0;
			if (x < -65000.0) x = -65000.0;
			buffer[i * 2 + ch] = fl2fp((float)x);
		}
	}
}
