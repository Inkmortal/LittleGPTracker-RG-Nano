#include "MasterLimiter.h"
#include "Application/Model/Project.h"
#include "Services/Audio/Audio.h"
#include <math.h>
#include <string.h>

#define MASK (LIMITER_MAX_WINDOW - 1)
#define FULL_SCALE 32767.0f
#define METER_FLOOR_DB -60.0f

static const FourCC limVars[4] = {VAR_LIM_DRIVE, VAR_LIM_CEILING, VAR_LIM_ATTACK,
                                  VAR_LIM_RELEASE};
static const int limDefaults[4] = {0x00, 0xFC, 0x80, 0x00};

MasterLimiter::MasterLimiter()
    : project_(0), enabled_(false), idle_(false), rate_(0.0f), window_(1), box1_(1),
      box2_(1), drive_(1.0f), ceiling_(FULL_SCALE), releaseMs_(-1.0f), n_(0),
      holdHead_(0), holdTail_(0), released_(1.0f), sum1_(1.0), sum2_(1.0), grDb_(0.0f),
      inDb_(METER_FLOOR_DB), outDb_(METER_FLOOR_DB), historyPos_(0) {
	for (int i = 0; i < 4; i++) {
		fixed_[i] = limDefaults[i];
		params_[i] = -1;
	}
	for (int i = 0; i < LIMITER_HISTORY; i++) {
		historyGr_[i] = 0.0f;
		historyIn_[i] = METER_FLOOR_DB;
	}
	reset();
}

void MasterLimiter::SetProject(Project *project) {
	project_ = project;
	for (int i = 0; i < 4; i++) params_[i] = -1;
	reset();
}

void MasterLimiter::SetParams(const int params[4]) {
	memcpy(fixed_, params, sizeof(fixed_));
}

float MasterLimiter::DriveDbFromParam(int value) {
	if (value < 1) value = 1;
	if (value > 255) value = 255;
	return (value - 1) * 0.1f;
}

float MasterLimiter::CeilingDbFromParam(int value) {
	if (value < 0) value = 0;
	if (value > 255) value = 255;
	return (value - 255) * 0.1f;
}

float MasterLimiter::AttackMsFromParam(int value) {
	if (value < 0) value = 0;
	if (value > 255) value = 255;
	return 0.1f * (float)pow(100.0, value / 255.0);
}

float MasterLimiter::ReleaseMsFromParam(int value) {
	if (value <= 0) return -1.0f;  // auto
	if (value > 255) value = 255;
	return 4.0f * (float)pow(250.0, (value - 1) / 254.0);
}

bool MasterLimiter::Enabled() {
	int p[4];
	readParams(p);
	return p[0] != 0;
}

float MasterLimiter::GetHistoryGrDb(int i) {
	if (i < 0 || i >= LIMITER_HISTORY) return 0.0f;
	return historyGr_[(historyPos_ + i) % LIMITER_HISTORY];
}

float MasterLimiter::GetHistoryInDb(int i) {
	if (i < 0 || i >= LIMITER_HISTORY) return METER_FLOOR_DB;
	return historyIn_[(historyPos_ + i) % LIMITER_HISTORY];
}

void MasterLimiter::readParams(int params[4]) {
	for (int i = 0; i < 4; i++) {
		Variable *v = project_ ? project_->FindVariable(limVars[i]) : 0;
		params[i] = v ? v->GetInt() : fixed_[i];
	}
}

// Everything back to "no gain reduction", audio delay silent
void MasterLimiter::reset() {
	memset(delay_, 0, sizeof(delay_));
	holdHead_ = holdTail_ = 0;
	released_ = 1.0f;
	for (int i = 0; i < LIMITER_MAX_WINDOW; i++) {
		boxRing1_[i] = boxRing2_[i] = 1.0f;
	}
	sum1_ = box1_;
	sum2_ = box2_;
	idle_ = true;
}

// A new look-ahead length: the averages restart at the current gain (no
// jump), the hold forgets; the audio delay just reads from a new place
void MasterLimiter::setWindow(int window) {
	if (window < 1) window = 1;
	if (window > LIMITER_MAX_WINDOW - 1) window = LIMITER_MAX_WINDOW - 1;
	window_ = window;
	// 58/42 split of the two averages: a smooth S-shaped attack
	box1_ = (int)(window * 0.58f + 0.999f);
	if (box1_ < 1) box1_ = 1;
	box2_ = window - box1_ + 1;
	if (box2_ < 1) box2_ = 1;
	for (int i = 0; i < LIMITER_MAX_WINDOW; i++) {
		boxRing1_[i] = boxRing2_[i] = released_;
	}
	sum1_ = (double)released_ * box1_;
	sum2_ = (double)released_ * box2_;
	holdHead_ = holdTail_ = 0;
}

void MasterLimiter::configure(const int params[4]) {
	float rate = (float)Audio::GetInstance()->GetSampleRate();
	if (rate < 8000.0f) rate = 44100.0f;
	drive_ = (float)pow(10.0, DriveDbFromParam(params[0]) / 20.0);
	ceiling_ = FULL_SCALE * (float)pow(10.0, CeilingDbFromParam(params[1]) / 20.0);
	releaseMs_ = ReleaseMsFromParam(params[3]);
	int window = (int)(AttackMsFromParam(params[2]) * 0.001f * rate + 0.5f);
	if (window < 1) window = 1;
	if (window > LIMITER_MAX_WINDOW - 1) window = LIMITER_MAX_WINDOW - 1;
	if (window != window_ || rate != rate_) {
		rate_ = rate;
		setWindow(window);
	}
	memcpy(params_, params, sizeof(params_));
}

void MasterLimiter::Silence() {
	if (!idle_) reset();
	grDb_ = 0.0f;
	inDb_ = outDb_ = METER_FLOOR_DB;
	historyGr_[historyPos_] = 0.0f;
	historyIn_[historyPos_] = METER_FLOOR_DB;
	historyPos_ = (historyPos_ + 1) % LIMITER_HISTORY;
}

static float toDb(float linear) {
	if (linear <= 0.001f) return METER_FLOOR_DB;
	float db = 20.0f * (float)log10(linear);
	return db < METER_FLOOR_DB ? METER_FLOOR_DB : db;
}

void MasterLimiter::Process(fixed *buffer, int frames) {
	int p[4];
	readParams(p);
	float peakIn = 0.0f;
	if (p[0] == 0) {
		// Off: untouched, no delay. The screen still shows the mix level
		if (!idle_) reset();
		enabled_ = false;
		for (int i = 0; i < frames * 2; i++) {
			float a = fabsf(fp2fl(buffer[i]));
			if (a > peakIn) peakIn = a;
		}
		grDb_ = 0.0f;
		inDb_ = outDb_ = toDb(peakIn / FULL_SCALE);
		historyGr_[historyPos_] = 0.0f;
		historyIn_[historyPos_] = inDb_;
		historyPos_ = (historyPos_ + 1) % LIMITER_HISTORY;
		return;
	}
	if (memcmp(p, params_, sizeof(p)) || !enabled_) {
		configure(p);
	}
	enabled_ = true;
	idle_ = false;

	const float ceiling = ceiling_;
	const float drive = drive_;
	const int window = window_;
	const float inv1 = 1.0f / box1_, inv2 = 1.0f / box2_;
	float relCoef = 0.0f;
	if (releaseMs_ > 0.0f) {
		relCoef = 1.0f - (float)exp(-1000.0 / (releaseMs_ * rate_));
	}
	float minGain = 1.0f;
	float peakOut = 0.0f;

	for (int i = 0; i < frames; i++) {
		if (releaseMs_ <= 0.0f && (i & 31) == 0) {
			// Auto release: 100 ms when barely limiting, up to 900 ms when
			// pressing hard, so heavy limiting does not pump
			float gr = -20.0f * (float)log10(released_ > 1e-4f ? released_ : 1e-4f);
			float amount = gr / 12.0f;
			if (amount > 1.0f) amount = 1.0f;
			float ms = 100.0f + 800.0f * amount;
			relCoef = 1.0f - (float)exp(-1000.0 / (ms * rate_));
		}
		float l = fp2fl(buffer[i * 2]) * drive;
		float r = fp2fl(buffer[i * 2 + 1]) * drive;
		float al = fabsf(l), ar = fabsf(r);
		float a = al > ar ? al : ar;
		if (a > peakIn) peakIn = a;
		float need = a > ceiling ? ceiling / a : 1.0f;

		// Peak hold: the lowest gain needed over the look-ahead window
		while (holdTail_ != holdHead_ && holdValue_[(holdTail_ - 1) & MASK] >= need) {
			holdTail_--;
		}
		holdValue_[holdTail_ & MASK] = need;
		holdIndex_[holdTail_ & MASK] = n_;
		holdTail_++;
		while (n_ - holdIndex_[holdHead_ & MASK] >= (unsigned int)window) {
			holdHead_++;
		}
		float held = holdValue_[holdHead_ & MASK];

		// Release: down at once (the averages below shape the attack), back
		// up exponentially
		if (held < released_) {
			released_ = held;
		} else {
			released_ += (held - released_) * relCoef;
		}

		// Two moving averages, window long in total
		float old1 = boxRing1_[(n_ - box1_) & MASK];
		boxRing1_[n_ & MASK] = released_;
		sum1_ += released_ - old1;
		float s1 = (float)sum1_ * inv1;
		float old2 = boxRing2_[(n_ - box2_) & MASK];
		boxRing2_[n_ & MASK] = s1;
		sum2_ += s1 - old2;
		float gain = (float)sum2_ * inv2;
		if (gain > 1.0f) gain = 1.0f;
		if (gain < minGain) minGain = gain;

		// The audio, window-1 samples late: the gain is down by the time
		// the peak comes out
		delay_[(n_ & MASK) * 2] = l;
		delay_[(n_ & MASK) * 2 + 1] = r;
		unsigned int d = (n_ - (unsigned int)(window - 1)) & MASK;
		float ol = delay_[d * 2] * gain;
		float orr = delay_[d * 2 + 1] * gain;
		// Rounding in the running sums can leave a hair over the ceiling
		if (ol > ceiling) ol = ceiling;
		if (ol < -ceiling) ol = -ceiling;
		if (orr > ceiling) orr = ceiling;
		if (orr < -ceiling) orr = -ceiling;
		float ao = fabsf(ol) > fabsf(orr) ? fabsf(ol) : fabsf(orr);
		if (ao > peakOut) peakOut = ao;
		buffer[i * 2] = fl2fp(ol);
		buffer[i * 2 + 1] = fl2fp(orr);
		n_++;
	}

	grDb_ = -toDb(minGain) < 0.0f ? 0.0f : -toDb(minGain);
	inDb_ = toDb(peakIn / FULL_SCALE);
	outDb_ = toDb(peakOut / FULL_SCALE);
	historyGr_[historyPos_] = grDb_;
	historyIn_[historyPos_] = inDb_;
	historyPos_ = (historyPos_ + 1) % LIMITER_HISTORY;
}
