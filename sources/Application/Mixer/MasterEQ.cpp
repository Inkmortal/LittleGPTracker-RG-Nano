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
	// A band at 0 dB is the identity: only the others run (like the
	// instrument EQ)
	int active[3];
	int activeCount = 0;
	for (int b = 0; b < 3; b++) {
		if (ThreeBandEQ::BandFlat(params_, b)) {
			state_[b][0][0] = state_[b][0][1] = state_[b][1][0] = state_[b][1][1] = 0.0;
		} else {
			active[activeCount++] = b;
		}
	}
	// Each channel through each band in turn over a chunk, the filter
	// state and coefficients held in registers (the same operations per
	// sample as one sample through all bands, in the same order)
	const int CHUNK = 128;
	double x[CHUNK];
	for (int start = 0; start < frames; start += CHUNK) {
		int n = frames - start;
		if (n > CHUNK) n = CHUNK;
		for (int ch = 0; ch < 2; ch++) {
			fixed *io = buffer + start * 2 + ch;
			for (int i = 0; i < n; i++) x[i] = fp2fl(io[i * 2]);
			for (int k = 0; k < activeCount; k++) {
				int b = active[k];
				const float *c = coeffs_[b];
				double z[2] = {state_[b][ch][0], state_[b][ch][1]};
				for (int i = 0; i < n; i++) x[i] = ThreeBandEQ::Step(c, z, x[i]);
				state_[b][ch][0] = z[0];
				state_[b][ch][1] = z[1];
			}
			for (int i = 0; i < n; i++) {
				double v = x[i];
				// A boost on a loud mix must not wrap the fixed-point sample;
				// the master clipper after this handles the rest
				if (v > 65000.0) v = 65000.0;
				if (v < -65000.0) v = -65000.0;
				io[i * 2] = fl2fp((float)v);
			}
		}
	}
}
