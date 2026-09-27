// harness: a7cost
// The synth's DRUM and PHYS engines (Plaits models, SynthPlaits.cpp) built
// for the device and run under qemu-arm with the a7cost plugin:
//  - every DRUM and PHYS preset sounds, stays finite and its voice ends by
//    itself once the model has died away
//  - pitch: a plain string (material 40) and a modal resonator at material
//    40 play the note (A3 = 220 Hz); the 808 kick rings at its tuned pitch
//  - decay: a longer decay knob rings longer; snap adds snare rattle
//  - a project round trip keeps every DRUM and PHYS knob
//  - cost of 8 voices of every preset next to the other engines, as a share
//    of the Cortex-A7 at 1.2 GHz (a7cost units: weighted instructions, the
//    model tools/dsp-harness/engine_room_check.cpp is calibrated with).
//    Drums are hit on every channel 8 times a second, struck models 4 times
//    a second. Each preset must stay under 25% of the CPU for 8 voices: the
//    Engine Room demo (heavy engines on all 8 tracks, sends, master EQ and
//    limiter) measures ~20% average / 27% peak on the device and plays
//    without dropouts, so 8 voices of one engine plus the master chain
//    stay in that range.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/SynthPlaits.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "System/FileSystem/FileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static const int FRAMES=1199;   // what the device mixes per buffer
static const double RATE=44100.0;
static const double DEVICE_HZ=1.2e9;
static const double BUDGET_PERCENT=25.0;
static fixed buffer[FRAMES*2];
static int failures=0;

#define SYS_MARK 0x7AB0
#define SYS_RESET 0x7AB2
#define SYS_SLOTS 0x7AB3

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-48s got=%10.4f want=%10.4f %s\n",what,got,want,ok?"ok":"WRONG");
	fflush(stdout);
	if (!ok) failures++;
}

// Cost charged to slot 1 since the last reset (a7cost units), -1 without
// the plugin
static double a7SlotCost() {
	const char *path=getenv("A7COST_OUT");
	if (!path) return -1.0;
	unlink(path);
	syscall(SYS_SLOTS);
	int fd=open(path,O_RDONLY);
	if (fd<0) return -1.0;
	std::string text;
	char chunk[4096];
	int got;
	while ((got=read(fd,chunk,sizeof(chunk)))>0) text.append(chunk,got);
	close(fd);
	size_t pos=0;
	while (pos<text.size()) {
		size_t nl=text.find('\n',pos);
		if (nl==std::string::npos) nl=text.size();
		int slot;
		unsigned long long cost,insn;
		if (sscanf(text.substr(pos,nl-pos).c_str(),"slot %d %llu %llu",&slot,&cost,&insn)==3 && slot==1) {
			return (double)cost;
		}
		pos=nl+1;
	}
	return 0.0;
}

static void set(SynthInstrument *s, const char *name, int value) {
	Variable *v=s->FindVariable(name);
	if (!v) { printf("no variable %s\n",name); failures++; return; }
	v->SetInt(value);
}

static void setString(SynthInstrument *s, const char *name, const char *value) {
	Variable *v=s->FindVariable(name);
	if (!v) { printf("no variable %s\n",name); failures++; return; }
	v->SetString(value);
}

static SynthInstrument *make(const char *preset) {
	SynthInstrument *s=new SynthInstrument();
	s->Init();
	setString(s,"preset",preset);
	if (strcmp(s->FindVariable("preset")->GetString(),preset)) {
		printf("preset %s missing\n",preset);
		failures++;
	}
	set(s,"reverb",0);
	set(s,"delay",0);
	set(s,"chorus",0);
	set(s,"pan",0x7F);
	return s;
}

static bool voiceActive(SynthInstrument *s, int channel) {
	int stage;
	float level;
	s->GetVoiceDebug(channel,stage,level);
	return stage>=0;
}

// One hit: 'seconds' of the left channel; 'endedAt' = when the voice
// stopped by itself (-1 if it still plays at the end)
static void hit(SynthInstrument *s, int note, double seconds, std::vector<float> &out,
                double &endedAt) {
	s->Start(0,(unsigned char)note,true);
	int total=(int)(seconds*RATE);
	out.clear();
	endedAt=-1.0;
	while ((int)out.size()<total) {
		s->Render(0,buffer,FRAMES,out.empty());
		for (int i=0;i<FRAMES;i++) out.push_back(fp2fl(buffer[i*2])/32767.0f);
		if (endedAt<0.0 && !voiceActive(s,0)) endedAt=out.size()/RATE;
	}
	out.resize(total);
}

// Amplitude of a sine at hz over a stretch (Hann window)
static double amplitudeAt(const std::vector<float> &x, size_t from, size_t n, double hz) {
	double re=0,im=0,wsum=0;
	for (size_t i=0;i<n;i++) {
		double w=0.5-0.5*cos(2.0*M_PI*i/(n-1));
		double ph=2.0*M_PI*hz*i/RATE;
		re+=x[from+i]*w*cos(ph);
		im+=x[from+i]*w*sin(ph);
		wsum+=w;
	}
	return 2.0*sqrt(re*re+im*im)/wsum;
}

// Loudest frequency between lo and hi (scan, then parabola)
static double peakHz(const std::vector<float> &x, size_t from, size_t n, double lo, double hi,
                     double step) {
	int steps=(int)((hi-lo)/step);
	std::vector<double> a(steps+1);
	int best=0;
	for (int k=0;k<=steps;k++) {
		a[k]=amplitudeAt(x,from,n,lo+k*step);
		if (a[k]>a[best]) best=k;
	}
	double hz=lo+best*step;
	if (best>0 && best<steps) {
		double y0=a[best-1],y1=a[best],y2=a[best+1];
		hz+=0.5*(y0-y2)/(y0-2*y1+y2)*step;
	}
	return hz;
}

static double peakOf(const std::vector<float> &x) {
	double p=0;
	for (size_t i=0;i<x.size();i++) {
		double a=fabs(x[i]);
		if (a>p) p=a;
	}
	return p;
}

// Seconds until the level (10 ms RMS windows) stays 40 dB under its peak
static double ringSeconds(const std::vector<float> &x) {
	const int w=441;
	std::vector<double> rms;
	for (size_t i=0;i+w<=x.size();i+=w) {
		double s=0;
		for (int k=0;k<w;k++) s+=x[i+k]*x[i+k];
		rms.push_back(sqrt(s/w));
	}
	double top=0;
	for (size_t i=0;i<rms.size();i++) if (rms[i]>top) top=rms[i];
	size_t last=0;
	for (size_t i=0;i<rms.size();i++) if (rms[i]>top*0.01) last=i;
	return (last+1)*w/RATE;
}

// Energy above 'hz' (one-pole high-pass), relative to the total
static double highShare(const std::vector<float> &x, double hz) {
	double a=exp(-2.0*M_PI*hz/RATE);
	double lp=0,hi=0,all=0;
	for (size_t i=0;i<x.size();i++) {
		lp=a*lp+(1.0-a)*x[i];
		double h=x[i]-lp;
		hi+=h*h;
		all+=x[i]*x[i];
	}
	return all>0?hi/all:0;
}

struct CostRun {
	const char *preset;
	double hitsPerSecond;   // 0 = one sustained note per channel
} ;

// 8 voices of one preset for 'seconds': share of the A7, and the peak
static double costOf(const CostRun &run, double seconds, double &peak) {
	SynthInstrument *s=make(run.preset);
	static const int notes[8]={48,52,55,59,60,64,67,71};
	for (int ch=0;ch<8;ch++) s->Start(ch,(unsigned char)notes[ch],true);
	int buffers=(int)(seconds*RATE/FRAMES);
	double nextHit=run.hitsPerSecond>0?1.0/run.hitsPerSecond:1e9;
	peak=0;
	syscall(SYS_RESET);
	for (int b=0;b<buffers;b++) {
		double t=(double)b*FRAMES/RATE;
		syscall(SYS_MARK,1);
		if (t>=nextHit) {
			for (int ch=0;ch<8;ch++) s->Start(ch,(unsigned char)notes[ch],true);
			nextHit+=1.0/run.hitsPerSecond;
		}
		for (int ch=0;ch<8;ch++) {
			s->Render(ch,buffer,FRAMES,(b%4)==0);
			syscall(SYS_MARK,0);
			for (int i=0;i<FRAMES*2;i++) {
				double v=fabs(fp2fl(buffer[i])/32767.0f);
				if (v>peak) peak=v;
			}
			syscall(SYS_MARK,1);
		}
		syscall(SYS_MARK,0);
	}
	double cost=a7SlotCost();
	delete s;
	if (cost<0) return -1.0;
	return 100.0*cost/(buffers*FRAMES/RATE*DEVICE_HZ);
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
	std::vector<float> x;
	double ended;

	static const char *drumPresets[]={
		"drum init","808 kick","909 kick","boom kick","808 snare","909 snare","rim",
		"808 hat","808 open","ring hat","808 tom"
	};
	static const char *physPresets[]={
		"phys init","wood bar","vibes","temple bell","kalimba","hand drum",
		"guzheng","nylon","sitar","steel","pluck bass"
	};

	printf("--- Every preset sounds and ends by itself\n");
	for (int k=0;k<(int)(sizeof(drumPresets)/sizeof(char*));k++) {
		SynthInstrument *s=make(drumPresets[k]);
		expect(s->GetEngine()==SE_DRUM,drumPresets[k],s->GetEngine(),SE_DRUM);
		hit(s,60,8.0,x,ended);
		char what[64];
		sprintf(what,"%s: peak",drumPresets[k]);
		double p=peakOf(x);
		expect(p>0.05 && p<=2.0,what,p,0.5);
		sprintf(what,"%s: voice ends (s)",drumPresets[k]);
		expect(ended>0.0 && ended<8.0,what,ended,1.0);
		delete s;
	}
	for (int k=0;k<(int)(sizeof(physPresets)/sizeof(char*));k++) {
		SynthInstrument *s=make(physPresets[k]);
		expect(s->GetEngine()==SE_PHYS,physPresets[k],s->GetEngine(),SE_PHYS);
		hit(s,60,20.0,x,ended);
		char what[64];
		sprintf(what,"%s: peak",physPresets[k]);
		double p=peakOf(x);
		expect(p>0.05 && p<=2.0,what,p,0.5);
		sprintf(what,"%s: voice ends (s)",physPresets[k]);
		expect(ended>0.0 && ended<20.0,what,ended,5.0);
		delete s;
	}

	printf("--- Pitch\n");
	{
		SynthInstrument *s=make("nylon");
		hit(s,57,1.0,x,ended);
		double hz=peakHz(x,4410,16384,180.0,260.0,0.25);
		expect(fabs(hz/220.0-1.0)<0.005,"string, material 40: A3 = 220 Hz",hz,220.0);
		delete s;
		s=make("phys init");
		set(s,"phys material",0x40);
		hit(s,57,1.0,x,ended);
		hz=peakHz(x,2205,16384,180.0,260.0,0.25);
		expect(fabs(hz/220.0-1.0)<0.005,"modal, material 40: A3 = 220 Hz",hz,220.0);
		delete s;
		s=make("808 kick");
		hit(s,60,1.0,x,ended);
		double want=440.0*pow(2.0,(60-26-69)/12.0);
		hz=peakHz(x,2205,16384,30.0,100.0,0.1);
		expect(fabs(hz/want-1.0)<0.03,"808 kick, C3 tune -26: rings at A#1",hz,want);
		delete s;
	}

	printf("--- Knobs\n");
	{
		SynthInstrument *s=make("808 kick");
		set(s,"drum decay",0x40);
		hit(s,60,4.0,x,ended);
		double shortRing=ringSeconds(x);
		set(s,"drum decay",0xC0);
		hit(s,60,4.0,x,ended);
		double longRing=ringSeconds(x);
		expect(longRing>2.0*shortRing,"kick decay C0 rings > 2x decay 40 (s)",longRing,2.0*shortRing);
		delete s;
		s=make("808 snare");
		set(s,"drum snap",0);
		hit(s,60,0.5,x,ended);
		double dull=highShare(x,3000.0);
		set(s,"drum snap",0xFF);
		hit(s,60,0.5,x,ended);
		double crisp=highShare(x,3000.0);
		expect(crisp>2.0*dull,"snare snap FF: more energy above 3 kHz",crisp,2.0*dull);
		delete s;
		s=make("wood bar");
		set(s,"phys decay",0x30);
		hit(s,60,8.0,x,ended);
		shortRing=ringSeconds(x);
		set(s,"phys decay",0xC0);
		hit(s,60,8.0,x,ended);
		longRing=ringSeconds(x);
		expect(longRing>2.0*shortRing,"modal decay C0 rings > 2x decay 30 (s)",longRing,2.0*shortRing);
		delete s;
	}

	printf("--- Save and restore (by name, in order, like a project file)\n");
	{
		SynthInstrument *a=make("909 snare");
		set(a,"drum snap",0x33);
		set(a,"drum accent",0x44);
		setString(a,"phys model","string");
		set(a,"phys material",0x12);
		std::vector<std::string> names,values;
		IteratorPtr<Variable> it(a->GetIterator());
		for (it->Begin();!it->IsDone();it->Next()) {
			names.push_back(it->CurrentItem().GetName());
			values.push_back(it->CurrentItem().GetString());
		}
		SynthInstrument *b=new SynthInstrument();
		b->Init();
		for (size_t k=0;k<names.size();k++) {
			Variable *v=b->FindVariable(names[k].c_str());
			if (v) v->SetString(values[k].c_str());
		}
		int wrong=0;
		IteratorPtr<Variable> it2(b->GetIterator());
		size_t k=0;
		for (it2->Begin();!it2->IsDone();it2->Next(),k++) {
			if (values[k]!=it2->CurrentItem().GetString()) {
				printf("  %s: %s != %s\n",names[k].c_str(),it2->CurrentItem().GetString(),values[k].c_str());
				wrong++;
			}
		}
		expect(wrong==0,"every knob comes back",wrong,0);
		expect(b->GetEngine()==SE_DRUM,"engine drum",b->GetEngine(),SE_DRUM);
		delete a;
		delete b;
	}

	printf("--- Cost of 8 voices, %% of the Cortex-A7 at 1.2 GHz (a7cost)\n");
	{
		CostRun runs[]={
			{"pad",0},{"epiano",0},{"hyper pad",0},{"pwm pad",0},{"kick",8},
			{"808 kick",8},{"909 kick",8},{"808 snare",8},{"909 snare",8},
			{"808 hat",8},{"ring hat",8},{"808 open",8},
			{"phys init",4},{"wood bar",4},{"temple bell",4},{"hand drum",4},
			{"guzheng",4},{"sitar",4},{"steel",4},{"pluck bass",4},
		};
		for (unsigned int k=0;k<sizeof(runs)/sizeof(runs[0]);k++) {
			double peak;
			double pct=costOf(runs[k],2.0,peak);
			if (pct<0) {
				printf("a7cost plugin not loaded: the cost guard cannot run\n");
				failures++;
				break;
			}
			SynthInstrument *probe=make(runs[k].preset);
			int engine=probe->GetEngine();
			delete probe;
			printf("%-14s %-5s %6.2f%%  peak %.2f\n",runs[k].preset,
			       SynthInstrument::GetEngineName(engine),pct,peak);
			if (engine==SE_DRUM || engine==SE_PHYS) {
				char what[64];
				sprintf(what,"%s: 8 voices under %.0f%%",runs[k].preset,BUDGET_PERCENT);
				expect(pct<BUDGET_PERCENT,what,pct,BUDGET_PERCENT);
			}
		}
	}

	if (failures) printf("FAILED: %d\n",failures);
	else printf("all plaits engine checks passed\n");
	return failures?1:0;
}
