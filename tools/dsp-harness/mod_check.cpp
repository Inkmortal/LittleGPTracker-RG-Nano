// MOD slots (four per instrument: AHD, ADSR, drum, trig envelopes, LFOs,
// key tracking) built for the device, run under qemu-arm: each envelope's
// timing and levels, every LFO shape, rate and trigger mode, key tracking,
// the first release's decay/swell and song conversion, then real synth and
// sample voices (ADSR release after note-off, fine pitch, sample start).
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/ModSources.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include "System/FileSystem/FileSystem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static int failures=0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-48s got=%9.4f want=%9.4f %s\n",what,got,want,ok?"ok":"WRONG");
	if (!ok) failures++;
}

static void near(const char *what, double got, double want, double tol) {
	expect(fabs(got-want)<=tol,what,got,want);
}

static ModSlotSettings slot(int type, int dest, int amount, int p1=0, int p2=0, int p3=0, int p4=0) {
	ModSlotSettings s;
	s.type_=type;
	s.dest_=dest;
	s.amount_=amount;
	s.param_[0]=p1;
	s.param_[1]=p2;
	s.param_[2]=p3;
	s.param_[3]=p4;
	return s;
}

// Steps a source to 't' seconds after its start (k-rate 'krate')
static void runTo(ModSource &m, float krate, int &done, float t) {
	int target=(int)(t*krate+0.5f);
	while (done<target) {
		m.Trigger(false);
		done++;
	}
}

static RUParams outputOf(ModSource &m) {
	RUParams r;
	r.Reset();
	m.UpdateSRP(r);
	return r;
}

// 1 s of 16-bit mono at 44.1 kHz: a 441 Hz sine, silent in the first
// half when 'halfSilent'
static void put(unsigned char *&p, const void *data, int n) {
	memcpy(p,data,n);
	p+=n;
}

static void writeWav(const char *path, bool halfSilent) {
	const int n=44100;
	static unsigned char file[44+n*2];
	unsigned char *p=file;
	int dataBytes=n*2;
	int riff=36+dataBytes;
	int fmtLen=16, rate=44100, bps=88200;
	short tag=1, ch=1, align=2, bits=16;
	put(p,"RIFF",4); put(p,&riff,4); put(p,"WAVEfmt ",8); put(p,&fmtLen,4);
	put(p,&tag,2); put(p,&ch,2); put(p,&rate,4); put(p,&bps,4); put(p,&align,2); put(p,&bits,2);
	put(p,"data",4); put(p,&dataBytes,4);
	for (int i=0;i<n;i++) {
		short v=0;
		if (!halfSilent || i>=n/2) {
			v=(short)(16000.0*sin(6.28318530718*441.0*i/44100.0));
		}
		put(p,&v,2);
	}
	I_File *f=FileSystem::GetInstance()->Open(path,(char *)"wb");
	if (!f) return;
	f->Write(file,1,(int)(p-file));
	f->Close();
	delete f;
}

static float blockPeak(fixed *buffer, int frames) {
	float peak=0.0f;
	for (int i=0;i<frames*2;i++) {
		float v=fabsf(fp2fl(buffer[i]));
		if (v>peak) peak=v;
	}
	return peak;
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	Path::SetAlias("bin",".");
	Path::SetAlias("root",".");
	Path::SetAlias("samples","/tmp");
	AudioSettings settings;
	settings.bufferSize_=1024;
	settings.preBufferCount_=0;
	Audio::Install(new HarnessAudio(settings));

	const float kr=1000.0f;   // 1 ms per k-rate step: times read directly
	float A=ModSource::TimeFromParam(0x80);
	float H=ModSource::TimeFromParam(0x40);
	float D=ModSource::TimeFromParam(0x80);
	float curveHalf=1.0f-(float)((1.0-exp(-2.5))/(1.0-exp(-5.0)));  // decay at half its time
	near("time 00 = 0 ms",ModSource::TimeFromParam(0),0.0,1e-9);
	near("time 80 = 102 ms",A,0.1018,0.0005);
	near("time FF = 10 s",ModSource::TimeFromParam(0xFF),10.0,0.001);

	// --- AHD: linear attack, hold at full, decay lands on zero at A+H+D
	{
		ModSource m; int n=0;
		m.Start(slot(MT_AHD,MD_CUTOFF,MOD_AMOUNT_MAX,0x80,0x40,0x80),kr,60,0,1);
		near("ahd starts at 0",m.GetLevel(),0.0,1e-6);
		runTo(m,kr,n,A/2); near("ahd half the attack = 0.5",m.GetLevel(),0.5,0.01);
		runTo(m,kr,n,A+H*0.5f); near("ahd holds at full",m.GetLevel(),1.0,1e-6);
		runTo(m,kr,n,A+H+D*0.5f); near("ahd decay at half its time",m.GetLevel(),curveHalf,0.01);
		runTo(m,kr,n,A+H+D-0.002f); expect(m.GetLevel()>0.0f && m.GetLevel()<0.01f,"ahd nearly zero just before the end",m.GetLevel(),0.0);
		runTo(m,kr,n,A+H+D+0.002f); near("ahd zero after A+H+D",m.GetLevel(),0.0,1e-6);
		expect(m.IsDone(),"ahd done",m.IsDone(),1);
		// attack 00: full right at the note start
		m.Start(slot(MT_AHD,MD_CUTOFF,MOD_AMOUNT_MAX,0,0,0x80),kr,60,0,1);
		near("ahd attack 00 starts full",m.GetLevel(),1.0,1e-6);
		RUParams r=outputOf(m);
		near("ahd full amount on cutoff = +1",fp2fl(r.cutOffset_),1.0,0.002);
	}

	// --- ADSR: attack, decay to sustain, holds, releases after note-off
	{
		float Aa=ModSource::TimeFromParam(0x40);
		ModSource m; int n=0;
		m.Start(slot(MT_ADSR,MD_CUTOFF,MOD_AMOUNT_MAX,0x40,0x80,0x80,0x80),kr,60,0,1);
		runTo(m,kr,n,Aa); near("adsr full after attack",m.GetLevel(),1.0,0.02);
		float S=0x80/255.0f;
		runTo(m,kr,n,Aa+D*0.5f); near("adsr decay halfway",m.GetLevel(),S+(1.0f-S)*curveHalf,0.01);
		runTo(m,kr,n,0.5f); near("adsr holds sustain 80 = 50%",m.GetLevel(),S,1e-6);
		runTo(m,kr,n,2.0f); near("adsr still at sustain after 2 s",m.GetLevel(),S,1e-6);
		expect(!m.IsDone(),"adsr not done while held",m.IsDone(),0);
		m.NoteOff();
		int r0=n;
		runTo(m,kr,n,(r0+D*0.5f*kr)/kr); near("adsr release halfway",m.GetLevel(),S*curveHalf,0.01);
		runTo(m,kr,n,(r0+(D+0.002f)*kr)/kr); near("adsr zero after its release",m.GetLevel(),0.0,1e-6);
		expect(m.IsDone(),"adsr done after release",m.IsDone(),1);
		// note-off during the attack releases from where it is
		m.Start(slot(MT_ADSR,MD_CUTOFF,MOD_AMOUNT_MAX,0x80,0x80,0xFF,0x80),kr,60,0,1);
		n=0; runTo(m,kr,n,A*0.25f);
		float at=m.GetLevel();
		m.NoteOff();
		m.Trigger(false);
		expect(m.GetLevel()<=at && m.GetLevel()>at*0.8f,"adsr early note-off: no jump",m.GetLevel(),at);
	}

	// --- Drum: sharp peak, dip, body swells back, holds, decays
	{
		float tp=ModSource::PeakTime(0xFF);
		float body=ModSource::TimeFromParam(0x80);
		near("drum peak FF = 40 ms",tp,0.040,1e-4);
		near("drum dip FF = 10%",ModSource::PeakDip(0xFF),0.1,1e-4);
		ModSource m; int n=0;
		m.Start(slot(MT_DRUM,MD_VOLUME,MOD_AMOUNT_MAX,0xFF,0x80,0x80),kr,60,0,1);
		near("drum starts at the peak",m.GetLevel(),1.0,1e-6);
		runTo(m,kr,n,tp*0.5f); near("drum halfway into the dip",m.GetLevel(),0.55,0.02);
		runTo(m,kr,n,tp); near("drum at the bottom of the dip",m.GetLevel(),0.1,0.02);
		runTo(m,kr,n,2*tp); near("drum body back to full",m.GetLevel(),1.0,0.02);
		runTo(m,kr,n,2*tp+body*0.5f); near("drum body holds",m.GetLevel(),1.0,1e-6);
		runTo(m,kr,n,2*tp+body+D*0.5f); near("drum decay halfway",m.GetLevel(),curveHalf,0.01);
		runTo(m,kr,n,2*tp+body+D+0.002f); near("drum zero at the end",m.GetLevel(),0.0,1e-6);
	}

	// --- Trig: idle until a note plays on its source track
	{
		ModSource m; int n=0;
		m.Start(slot(MT_TRIG,MD_CUTOFF,-MOD_AMOUNT_MAX,0,0x40,0x80,2),kr,60,0,1);
		runTo(m,kr,n,0.05f); near("trig waits at 0",m.GetLevel(),0.0,1e-6);
		ModSource::NoteStarted(5);
		runTo(m,kr,n,0.06f); near("trig ignores other tracks",m.GetLevel(),0.0,1e-6);
		ModSource::NoteStarted(2);
		m.Trigger(false); n++;
		near("trig fires on track 3",m.GetLevel(),1.0,1e-6);
		RUParams r=outputOf(m);
		near("trig negative amount pulls cutoff down",fp2fl(r.cutOffset_),-1.0,0.002);
		int t0=n;
		runTo(m,kr,n,(t0+(H+D+0.003f)*kr)/kr); near("trig back to 0 after hold+decay",m.GetLevel(),0.0,1e-6);
	}

	// --- LFO shapes at phases 0.1, 0.35, 0.6, 0.85 (step = 1/100 cycle)
	{
		struct { int shape; float q[4]; const char *name; } shapes[]={
			{MLS_TRI,{-0.6f,0.4f,0.6f,-0.4f},"tri"},
			{MLS_SINE,{0.5878f,0.8090f,-0.5878f,-0.8090f},"sine"},
			{MLS_RAMP_DOWN,{0.8f,0.3f,-0.2f,-0.7f},"ramp dn"},
			{MLS_RAMP_UP,{-0.8f,-0.3f,0.2f,0.7f},"ramp up"},
			{MLS_SQUARE_DOWN,{1.0f,1.0f,-1.0f,-1.0f},"sqr dn"},
			{MLS_SQUARE_UP,{-1.0f,-1.0f,1.0f,1.0f},"sqr up"},
		};
		float hz=ModSource::RateFromParam(0x80);
		for (unsigned k=0;k<sizeof(shapes)/sizeof(shapes[0]);k++) {
			ModSource m;
			m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,shapes[k].shape,MLT_RETRIG),hz*100.0f,60,0,1);
			for (int i=0;i<10;i++) m.Trigger(false);
			for (int q=0;q<4;q++) {
				char what[64];
				sprintf(what,"lfo %s at phase %.2f",shapes[k].name,0.1f+0.25f*q);
				near(what,m.GetLevel(),shapes[k].q[q],0.01);
				for (int i=0;i<25;i++) m.Trigger(false);
			}
		}
		ModSource m;
		m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_EXP_DOWN,MLT_RETRIG),hz*100.0f,60,0,1);
		near("lfo exp dn starts at +1",m.GetLevel(),1.0,0.001);
		for (int i=0;i<50;i++) m.Trigger(false);
		near("lfo exp dn at half cycle",m.GetLevel(),1.0-2.0*(1.0-curveHalf),0.02);
		m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_EXP_UP,MLT_RETRIG),hz*100.0f,60,0,1);
		near("lfo exp up starts at -1",m.GetLevel(),-1.0,0.001);
		RUParams r=outputOf(m);
		near("lfo on pan: -1 = hard left offset",fp2fl(r.panOffset_),-127.0,0.5);
	}
	// rate: rising zero crossings of the sine over 20 s at the device k-rate
	{
		const float krate=441.0f;
		ModSource m;
		m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_SINE,MLT_RETRIG),krate,60,0,1);
		int rises=0; float prev=m.GetLevel();
		for (int i=0;i<(int)(20*krate);i++) {
			m.Trigger(false);
			if (prev<0.0f && m.GetLevel()>=0.0f) rises++;
			prev=m.GetLevel();
		}
		near("lfo rate 80: cycles in 20 s",rises,20*ModSource::RateFromParam(0x80),1.0);
		near("lfo rate 00 = 0.05 Hz",ModSource::RateFromParam(0),0.05,1e-6);
		near("lfo rate FF = 50 Hz",ModSource::RateFromParam(0xFF),50.0,0.01);
	}
	// trigger modes
	{
		float hz=ModSource::RateFromParam(0x80);
		ModSource m;
		m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_RAMP_DOWN,MLT_HOLD),hz*100.0f,60,0,1);
		for (int i=0;i<150;i++) m.Trigger(false);
		near("lfo hold: stays at the cycle's end",m.GetLevel(),-1.0,0.01);
		for (int i=0;i<150;i++) m.Trigger(false);
		near("lfo hold: still there",m.GetLevel(),-1.0,0.01);
		m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_RAMP_DOWN,MLT_ONCE),hz*100.0f,60,0,1);
		for (int i=0;i<150;i++) m.Trigger(false);
		near("lfo once: back to its start",m.GetLevel(),1.0,0.01);
		m.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_SINE,MLT_RETRIG),hz*100.0f,60,0,1);
		near("lfo retrig: new note starts the cycle",m.GetLevel(),0.0,1e-5);
		// free: two notes started at different times run in the same phase
		const float krate=441.0f;
		ModClock::Advance(12345);
		ModSource a;
		a.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_SINE,MLT_FREE),krate,60,0,1);
		for (int i=0;i<300;i++) {
			a.Trigger(false);
			ModClock::Advance(100);   // 100 samples per k-rate step
		}
		ModSource b;
		b.Start(slot(MT_LFO,MD_PAN,MOD_AMOUNT_MAX,0x80,MLS_SINE,MLT_FREE),krate,72,3,99);
		// (needs a working floor(): the device libm's is fixed in RGNANOLibm)
		near("lfo free: later note joins in phase",b.GetLevel(),a.GetLevel(),0.03);
	}
	// random / drunk
	{
		const float krate=441.0f;
		ModSource m;
		m.Start(slot(MT_LFO,MD_RESO,MOD_AMOUNT_MAX,0xFF,MLS_RANDOM,MLT_RETRIG),krate,60,0,7);
		float lo=0,hi=0; int changes=0; float prev=m.GetLevel();
		for (int i=0;i<4410;i++) {
			m.Trigger(false);
			float v=m.GetLevel();
			if (v!=prev) changes++;
			prev=v;
			if (v<lo) lo=v;
			if (v>hi) hi=v;
		}
		expect(lo>=-1.001f && hi<=1.001f && hi-lo>1.0f,"random spans -1..1",hi-lo,2.0);
		near("random: one new level per cycle (10 s at 50 Hz)",changes,500,15);
		m.Start(slot(MT_LFO,MD_RESO,MOD_AMOUNT_MAX,0xC0,MLS_DRUNK,MLT_RETRIG),krate,60,0,7);
		lo=hi=m.GetLevel();
		for (int i=0;i<44100;i++) {
			m.Trigger(false);
			float v=m.GetLevel();
			if (v<lo) lo=v;
			if (v>hi) hi=v;
		}
		expect(lo>=-1.0f && hi<=1.0f && hi-lo>0.5f,"drunk wanders inside -1..1",hi-lo,1.0);
	}

	// --- Key tracking: from C1 (36) = low, C6 (96) = high, in between
	{
		ModSlotSettings s=slot(MT_TRACK,MD_CUTOFF,0,36,96,-64,64);
		near("track at 'from' = low",ModSource::TrackLevel(s,36),-64/127.0,1e-5);
		near("track at 'to' = high",ModSource::TrackLevel(s,96),64/127.0,1e-5);
		near("track halfway = middle",ModSource::TrackLevel(s,66),0.0,1e-5);
		near("track below 'from' stays low",ModSource::TrackLevel(s,12),-64/127.0,1e-5);
		ModSource m;
		m.Start(slot(MT_TRACK,MD_FINE,0,0,127,127,127),441.0f,60,0,1);
		expect(m.Enabled(),"track runs without an amount",m.Enabled(),1);
		RUParams r=outputOf(m);
		near("track full on fine = +1 semitone",fp2fl(r.speedOffset_),pow(2.0,1.0/12.0),0.002);
	}

	// --- Destinations: volume depth, pitch range
	{
		ModSource m;
		m.Start(slot(MT_AHD,MD_VOLUME,MOD_AMOUNT_MAX,0,0,0x40),kr,60,0,1);
		near("env on volume +100%: full at the peak",outputOf(m).volumeScale_,1.0,1e-5);
		for (int i=0;i<40;i++) m.Trigger(false);
		near("env on volume +100%: silent at the end",outputOf(m).volumeScale_,0.0,1e-5);
		m.Start(slot(MT_AHD,MD_VOLUME,-64,0,0,0x40),kr,60,0,1);
		near("env on volume -50%: ducks by half",outputOf(m).volumeScale_,1.0-64/127.0,1e-4);
		m.Start(slot(MT_SWELL,MD_PITCH,MOD_AMOUNT_MAX,0xFF),441.0f,60,0,1);
		for (int i=0;i<50;i++) m.Trigger(false);
		near("pitch full amount = two octaves up",fp2fl(outputOf(m).speedOffset_),4.0,0.01);
		m.Start(slot(MT_AHD,MD_REVERB,MOD_AMOUNT_MAX,0,0,0x80),kr,60,0,1);
		near("env on reverb send: +1",outputOf(m).extra_[RUX_REVERB],1.0,1e-5);
		m.Start(slot(MT_AHD,MD_CRUSH,MOD_AMOUNT_MAX,0,0,0x80),kr,60,0,1);
		near("env on crush: 15 bits",outputOf(m).extra_[RUX_CRUSH],15.0,1e-4);
	}

	// --- First release: decay / swell, and songs saved with it
	{
		const float krate=441.0f;
		ModSource m;
		m.Start(slot(MT_DECAY,MD_CUTOFF,MOD_AMOUNT_MAX,0x80),krate,60,0,1);
		near("decay starts at full",fp2fl(outputOf(m).cutOffset_),1.0,0.01);
		int steps=(int)(ModSource::LegacyEnvTime(0x80)*krate);
		for (int i=0;i<steps;i++) m.Trigger(false);
		near("decay is -40 dB after its rate",fp2fl(outputOf(m).cutOffset_),0.01,0.004);
		m.Start(slot(MT_SWELL,MD_VOLUME,-MOD_AMOUNT_MAX,0xC0),krate,60,0,1);
		steps=(int)(ModSource::LegacyEnvTime(0xC0)*krate)+5;
		for (int i=0;i<steps;i++) m.Trigger(false);
		near("swell reaches full (volume offset)",fp2fl(outputOf(m).volumeOffset_),-255.0,1.0);

		SynthInstrument *s=new SynthInstrument();
		InstrumentMods *mods=s->GetMods();
		expect(mods->RestoreLegacy("mod1 type","square"),"old 'square' slot is converted",1,1);
		mods->RestoreLegacy("mod1 rate","200");
		expect(!mods->RestoreLegacy("mod2 type","decay"),"old 'decay' loads as it is",0,0);
		s->FindVariable("mod2 type")->SetString("decay");
		mods->FinishRestore();
		ModSlotSettings a,b;
		mods->GetSlot(0,a);
		mods->GetSlot(1,b);
		expect(a.type_==MT_LFO && a.param_[0]==200 && a.param_[1]==MLS_SQUARE_DOWN && a.param_[2]==MLT_RETRIG,
		       "old square LFO = lfo sqr dn retrig, rate kept",a.param_[0],200);
		expect(b.type_==MT_DECAY && b.param_[0]==0x80,"old decay without a rate: default 80",b.param_[0],0x80);
		delete s;
	}

	static fixed buffer[2048*2];

	// --- A synth voice: ADSR on volume releases after note-off (KILL/OFF)
	{
		for (int withMod=0;withMod<2;withMod++) {
			SynthInstrument *s=new SynthInstrument();
			s->Init();
			s->FindVariable("preset")->SetString("pad");
			s->FindVariable("attack")->SetInt(0);
			s->FindVariable("sustain")->SetInt(0xFF);
			s->FindVariable("release")->SetInt(0xFF);    // 10 s: the voice keeps sounding
			if (withMod) {
				s->FindVariable("mod1 type")->SetString("adsr");
				s->FindVariable("mod1 dest")->SetString("volume");
				s->FindVariable("mod1 amount")->SetInt(MOD_AMOUNT_MAX);
				s->FindVariable("mod1 p1")->SetInt(0);
				s->FindVariable("mod1 p2")->SetInt(0);
				s->FindVariable("mod1 p3")->SetInt(0xFF);
				s->FindVariable("mod1 p4")->SetInt(0x80);  // 102 ms
			}
			s->Start(0,60,true);
			float held=0.0f;
			for (int b=0;b<30;b++) {
				s->Render(0,buffer,441,false);
				held=blockPeak(buffer,441);
			}
			s->Stop(0);
			float at45=0.0f,after=0.0f;
			for (int b=0;b<20;b++) {
				s->Render(0,buffer,441,false);
				float p=blockPeak(buffer,441);
				if (b==4) at45=p;          // 40..50 ms after note-off
				if (b>=12) after=p>after?p:after;  // 120..200 ms
			}
			if (withMod) {
				expect(held>1000.0f,"synth adsr: sounds while held",held,1000.0);
				expect(at45<held*0.25f && at45>held*0.01f,"synth adsr: fading 45 ms after note-off",at45/held,0.1);
				expect(after<held*0.002f,"synth adsr: silent after its release",after/held,0.0);
			} else {
				expect(after>held*0.8f,"synth without mod: 10 s release still loud",after/held,1.0);
			}
			delete s;
		}
	}

	// --- A synth voice: key tracking on fine pitch moves A4 up a semitone
	{
		SynthInstrument *s=new SynthInstrument();
		s->Init();
		s->FindVariable("preset")->SetString("init");
		s->FindVariable("wave")->SetString("sine");
		s->FindVariable("filter")->SetString("off");
		s->FindVariable("sustain")->SetInt(0xFF);
		s->FindVariable("mod1 type")->SetString("track");
		s->FindVariable("mod1 dest")->SetString("fine");
		s->FindVariable("mod1 p1")->SetInt(0);
		s->FindVariable("mod1 p2")->SetInt(127);
		s->FindVariable("mod1 p3")->SetInt(127);
		s->FindVariable("mod1 p4")->SetInt(127);
		s->Start(0,69,true);
		int rises=0; float prev=0.0f;
		for (int b=0;b<100;b++) {
			s->Render(0,buffer,441,false);
			for (int i=0;i<441;i++) {
				float v=fp2fl(buffer[i*2]);
				if (b>=10 && prev<0.0f && v>=0.0f) rises++;
				prev=v;
			}
		}
		near("synth fine +1 semitone: A4 at 466 Hz",rises/0.9,466.16,3.0);
		delete s;
	}

	// --- A sample voice: ADSR on volume keeps the note as a release tail
	writeWav("/tmp/modtone.wav",false);
	writeWav("/tmp/modhalf.wav",true);
	int tone=SamplePool::GetInstance()->AddProjectSample("modtone.wav");
	int half=SamplePool::GetInstance()->AddProjectSample("modhalf.wav");
	expect(tone>=0 && half>=0,"test samples loaded",tone,0);
	if (tone>=0 && half>=0) {
		SampleInstrument *s=new SampleInstrument();
		s->FindVariable("sample")->SetInt(tone);
		s->Init();
		s->FindVariable("loopmode")->SetString("loop");
		s->FindVariable("mod1 type")->SetString("adsr");
		s->FindVariable("mod1 dest")->SetString("volume");
		s->FindVariable("mod1 amount")->SetInt(MOD_AMOUNT_MAX);
		s->FindVariable("mod1 p1")->SetInt(0);
		s->FindVariable("mod1 p2")->SetInt(0);
		s->FindVariable("mod1 p3")->SetInt(0xFF);
		s->FindVariable("mod1 p4")->SetInt(0x80);
		s->Start(0,60,true);
		float held=0.0f;
		for (int b=0;b<30;b++) {
			s->Render(0,buffer,441,false);
			held=blockPeak(buffer,441);
		}
		s->Stop(0);
		expect(s->IsReleasing(0),"sample adsr: note-off starts a release tail",s->IsReleasing(0),1);
		float at45=0.0f; bool ended=false; int endBlock=-1;
		for (int b=0;b<40;b++) {
			bool sound=s->Render(0,buffer,441,false);
			if (b==4) at45=blockPeak(buffer,441);
			if (!sound && !ended) { ended=true; endBlock=b; }
		}
		expect(held>1000.0f,"sample adsr: sounds while held",held,1000.0);
		expect(at45<held*0.25f && at45>held*0.01f,"sample adsr: fading 45 ms after note-off",at45/held,0.1);
		expect(ended && endBlock>=9 && endBlock<=13,"sample adsr: tail ends with its release (~100 ms)",endBlock*10,110);
		expect(!s->IsReleasing(0),"sample adsr: tail over",s->IsReleasing(0),0);
		// no ADSR: a note-off ends a sample right away, as it always did
		s->FindVariable("mod1 type")->SetString("off");
		s->Start(0,60,true);
		s->Render(0,buffer,441,false);
		s->Stop(0);
		expect(!s->IsReleasing(0),"sample without adsr: no tail",s->IsReleasing(0),0);
		delete s;

		// Key tracking on 'start': high notes start in the loud second half
		s=new SampleInstrument();
		s->FindVariable("sample")->SetInt(half);
		s->Init();
		s->FindVariable("mod1 type")->SetString("track");
		s->FindVariable("mod1 dest")->SetString("start");
		s->FindVariable("mod1 p1")->SetInt(60);
		s->FindVariable("mod1 p2")->SetInt(61);
		s->FindVariable("mod1 p3")->SetInt(0);
		s->FindVariable("mod1 p4")->SetInt(64);   // +50 % of the sample
		s->Start(0,60,true);
		s->Render(0,buffer,441,false);
		float low=blockPeak(buffer,441);
		s->Start(0,61,true);
		s->Render(0,buffer,441,false);
		float high=blockPeak(buffer,441);
		expect(low<10.0f,"start tracking: note 60 starts in the silence",low,0.0);
		expect(high>1000.0f,"start tracking: note 61 starts half way (sound)",high,1000.0);
		delete s;
	}

	if (failures) printf("FAILED: %d\n",failures);
	else printf("mods ok\n");
	return failures?1:0;
}
