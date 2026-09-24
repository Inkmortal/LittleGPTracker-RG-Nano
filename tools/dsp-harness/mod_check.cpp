// Mod slots (envelopes/LFOs) built for the device, run under qemu-arm:
// the envelope and LFO maths, then a real synth voice with a volume LFO.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/ModSources.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include "System/FileSystem/FileSystem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static int failures=0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-44s got=%9.4f want=%9.4f %s\n",what,got,want,ok?"ok":"WRONG");
	if (!ok) failures++;
}

static RUParams freshParams() {
	RUParams r;
	r.volumeOffset_=r.cutOffset_=r.resOffset_=r.panOffset_=0;
	r.fbMixOffset_=r.fbTunOffset_=0;
	r.speedOffset_=FP_ONE;
	return r;
}

static void runSteps(ModSource &m, int steps) {
	for (int i=0;i<steps;i++) m.Trigger(false);
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	Path::SetAlias("bin",".");
	Path::SetAlias("root",".");
	AudioSettings settings;
	settings.bufferSize_=1024;
	settings.preBufferCount_=0;
	Audio::Install(new HarnessAudio(settings));

	const float krate=441.0f;

	// Decay envelope on cutoff: full amount at the start, -40 dB after its time
	ModSource m;
	m.Start(MT_DECAY,MD_CUTOFF,MOD_AMOUNT_MAX,0x80,krate,1);
	RUParams r=freshParams();
	m.UpdateSRP(r);
	expect(fabs(fp2fl(r.cutOffset_)-1.0f)<0.01f,"decay starts at full",fp2fl(r.cutOffset_),1.0);
	float len=ModSource::EnvTimeFromRate(0x80);
	runSteps(m,(int)(len*krate));
	r=freshParams(); m.UpdateSRP(r);
	expect(fabs(fp2fl(r.cutOffset_)-0.01f)<0.004f,"decay is -40 dB after its time",fp2fl(r.cutOffset_),0.01);

	// Swell envelope on volume, negative amount: 0 -> -255 and holds
	m.Start(MT_SWELL,MD_VOLUME,-MOD_AMOUNT_MAX,0xC0,krate,1);
	runSteps(m,(int)(ModSource::EnvTimeFromRate(0xC0)*krate)+5);
	r=freshParams(); m.UpdateSRP(r);
	expect(fabs(fp2fl(r.volumeOffset_)+255.0f)<1.0f,"swell reaches full (negative)",fp2fl(r.volumeOffset_),-255.0);

	// Pitch at full amount = +24 semitones = x4 speed
	m.Start(MT_SWELL,MD_PITCH,MOD_AMOUNT_MAX,0xFF,krate,1);
	runSteps(m,50);
	r=freshParams(); m.UpdateSRP(r);
	expect(fabs(fp2fl(r.speedOffset_)-4.0f)<0.01f,"pitch full amount = 2 octaves up",fp2fl(r.speedOffset_),4.0);

	// Sine LFO: one cycle at its rate, peak = amount
	m.Start(MT_SINE,MD_PAN,MOD_AMOUNT_MAX,0x80,krate,1);
	float hz=ModSource::RateFromParam(0x80);
	int quarter=(int)(krate/hz/4.0f+0.5f);
	runSteps(m,quarter);
	r=freshParams(); m.UpdateSRP(r);
	expect(fabs(fp2fl(r.panOffset_)-127.0f)<3.0f,"sine LFO peaks a quarter cycle in",fp2fl(r.panOffset_),127.0);

	// Random LFO stays in range
	m.Start(MT_RANDOM,MD_RESO,MOD_AMOUNT_MAX,0xFF,krate,7);
	float lo=0,hi=0;
	for (int i=0;i<2000;i++) {
		m.Trigger(false);
		r=freshParams(); m.UpdateSRP(r);
		float v=fp2fl(r.resOffset_);
		if (v<lo) lo=v;
		if (v>hi) hi=v;
	}
	expect(lo>=-1.001f && hi<=1.001f && hi-lo>1.0f,"random LFO spans -1..1",hi-lo,2.0);

	// A synth voice with a square LFO on volume: loud and silent halves
	SynthInstrument *s=new SynthInstrument();
	s->Init();
	s->FindVariable("preset")->SetString("pad");
	s->FindVariable("attack")->SetInt(0);
	s->FindVariable("mod1 type")->SetString("square");
	s->FindVariable("mod1 dest")->SetString("volume");
	s->FindVariable("mod1 amount")->SetInt(-MOD_AMOUNT_MAX);
	s->FindVariable("mod1 rate")->SetInt(0xA0);   // ~3.5 Hz
	static fixed buffer[2048*2];
	s->Start(0,60,true);
	float loudest=0.0f,quietest=1e9f;
	for (int b=0;b<60;b++) {
		s->Render(0,buffer,441,true);
		float peak=0.0f;
		for (int i=100;i<441*2;i++) {
			float v=fabsf(fp2fl(buffer[i]));
			if (v>peak) peak=v;
		}
		if (b>4) {
			if (peak>loudest) loudest=peak;
			if (peak<quietest) quietest=peak;
		}
	}
	expect(loudest>1000.0f,"volume LFO: loud half has sound",loudest,1000.0);
	expect(quietest<loudest*0.05f,"volume LFO: quiet half is silent",quietest,0.0);
	s->StopQuickly(0);
	for (int b=0;b<100;b++) s->Render(0,buffer,441,false);
	int stage; float level;
	s->GetVoiceDebug(0,stage,level);
	expect(stage<=0,"voice with mods still releases",stage,-1);

	if (failures) printf("FAILED: %d\n",failures);
	else printf("mods ok\n");
	return failures?1:0;
}
