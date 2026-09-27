// How loud every SYNTH and MACRO preset is, built for the device and run
// under qemu-arm. Each preset plays C 4 (note 60, the note the instrument
// screen auditions) exactly as it comes: its own envelope, filter, volume
// and pan; sends are separate buses, so only the dry sound is measured.
// Loudness is BS.1770 momentary loudness (K-weighted, 400 ms windows, the
// loudest window), in LUFS relative to a full-scale stereo sine at 0 LU;
// peak is the loudest sample in dBFS.
// Every preset must land in its role's window (drums, bass, keys/leads,
// pads) and never clip, so none is "barely audible" next to the others.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "System/FileSystem/FileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <stdlib.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static const int FRAMES=1199;   // what the device mixes per buffer
static const double RATE=44100.0;
static fixed buffer[FRAMES*2];
static int failures=0;

// K-weighting (BS.1770): a +4 dB shelf above ~1.5 kHz, then a high-pass at
// ~38 Hz; coefficients designed for the sample rate (as pyloudnorm does)
struct Biquad {
	double b0,b1,b2,a1,a2,z1,z2;
	void reset() { z1=z2=0; }
	double run(double x) {
		double y=b0*x+z1;
		z1=b1*x-a1*y+z2;
		z2=b2*x-a2*y;
		return y;
	}
};

static void kWeighting(Biquad &shelf, Biquad &highpass) {
	double G=3.999843853973347,Q=0.7071752369554196,fc=1681.974450955533;
	double A=pow(10.0,G/40.0);
	double w0=2.0*M_PI*fc/RATE;
	double alpha=sin(w0)/(2.0*Q);
	double c=cos(w0),sA=2.0*sqrt(A)*alpha;
	double a0=(A+1)-(A-1)*c+sA;
	shelf.b0=A*((A+1)+(A-1)*c+sA)/a0;
	shelf.b1=-2*A*((A-1)+(A+1)*c)/a0;
	shelf.b2=A*((A+1)+(A-1)*c-sA)/a0;
	shelf.a1=2*((A-1)-(A+1)*c)/a0;
	shelf.a2=((A+1)-(A-1)*c-sA)/a0;
	shelf.reset();
	fc=38.13547087602444;
	Q=0.5003270373238773;
	w0=2.0*M_PI*fc/RATE;
	alpha=sin(w0)/(2.0*Q);
	c=cos(w0);
	a0=1+alpha;
	highpass.b0=(1+c)/2/a0;
	highpass.b1=-(1+c)/a0;
	highpass.b2=(1+c)/2/a0;
	highpass.a1=-2*c/a0;
	highpass.a2=(1-alpha)/a0;
	highpass.reset();
}

struct Level {
	double lufs;   // loudest 400 ms window
	double hit;    // loudest 100 ms window: how loud a short hit sounds
	double peakDb;
};

// Momentary loudness of a stereo signal: -0.691 + 10 log10(sum of the
// channels' mean squares) over 400 ms windows every 100 ms
static Level measure(const std::vector<float> &L, const std::vector<float> &R) {
	Biquad s[2],h[2];
	kWeighting(s[0],h[0]);
	kWeighting(s[1],h[1]);
	size_t n=L.size();
	std::vector<double> sq(n);
	double peak=0;
	for (size_t i=0;i<n;i++) {
		double l=h[0].run(s[0].run(L[i]));
		double r=h[1].run(s[1].run(R[i]));
		sq[i]=l*l+r*r;
		double p=fabs(L[i])>fabs(R[i])?fabs(L[i]):fabs(R[i]);
		if (p>peak) peak=p;
	}
	Level lv;
	for (int w=0;w<2;w++) {
		size_t win=(size_t)((w?0.1:0.4)*RATE),hop=(size_t)((w?0.025:0.1)*RATE);
		double best=0;
		for (size_t start=0;start+win<=n;start+=hop) {
			double sum=0;
			for (size_t i=start;i<start+win;i++) sum+=sq[i];
			double ms=sum/win;
			if (ms>best) best=ms;
		}
		double v=best>0?-0.691+10.0*log10(best):-120.0;
		if (w) lv.hit=v; else lv.lufs=v;
	}
	lv.peakDb=peak>0?20.0*log10(peak):-120.0;
	return lv;
}

// Holds the note 1.2 s (a phrase row or four), then releases for 0.6 s
static void play(I_Instrument *s, std::vector<float> &L, std::vector<float> &R) {
	L.clear();
	R.clear();
	s->Start(0,60,true);
	int held=(int)(1.2*RATE/FRAMES)+1;
	int tail=(int)(0.6*RATE/FRAMES)+1;
	for (int b=0;b<held+tail;b++) {
		if (b==held) s->Stop(0);
		memset(buffer,0,sizeof(buffer));
		bool playing=s->Render(0,buffer,FRAMES,true);
		for (int i=0;i<FRAMES;i++) {
			L.push_back(playing?fp2fl(buffer[i*2])/32767.0f:0.0f);
			R.push_back(playing?fp2fl(buffer[i*2+1])/32767.0f:0.0f);
		}
	}
	s->StopQuickly(0);
	for (int b=0;b<20;b++) s->Render(0,buffer,FRAMES,false);
}

// Loudness windows by role, LUFS over the loudest 100 ms (how loud a hit
// or the start of a note sounds; a 400 ms window reads a 40 ms hat 6 dB
// quieter than it sounds). Kicks, snares and toms carry the groove; hats,
// rims and small percussion sit about 5 dB under them, as in a mix; basses
// and keys/leads/plucks sit with the drums; pads may sit lower since they
// hold and stack. Each window is 9-12 LU wide: presets differ on purpose,
// but none drops out of the mix.
enum Role { R_DRUM, R_SMALL, R_BASS, R_KEYS, R_PAD };
static const char *roleNames[]={"drum","small","bass","keys","pad"};
static const double roleLow[]={-18.0,-22.0,-19.0,-19.0,-22.0};
static const double roleHigh[]={-8.0,-11.0,-9.0,-8.0,-9.0};

static const char *drumNames[]={"kick","snare","clap","tom","808 kick","909 kick",
	"boom kick","808 snare","909 snare","808 tom","drum init","hand drum","metaltom",
	"dagu","tanggu",0};
static const char *smallNames[]={"hat","openhat","perc","rim","808 hat","808 open",
	"ring hat","noise hat","woodblock","bangzi","bo cymbal",0};
static const char *bassNames[]={"bass","subbass","acid","fm bass","slap bass",
	"hyper bass","chip bass","fold bass","pluck bass","fmbass","reese",0};
static const char *padNames[]={"pad","hyper pad","strings","dream","pwm pad",
	"hyper init","choir","chords","cloud","wind",0};

static bool inList(const char *name, const char **list) {
	for (int i=0;list[i];i++) {
		if (!strcmp(name,list[i])) return true;
	}
	return false;
}

static Role roleOf(const char *name) {
	if (inList(name,drumNames)) return R_DRUM;
	if (inList(name,smallNames)) return R_SMALL;
	if (inList(name,bassNames)) return R_BASS;
	if (inList(name,padNames)) return R_PAD;
	return R_KEYS;
}

static void check(const char *kind, const char *name, const Level &lv, int volume,
                  const Level &raw) {
	Role role=roleOf(name);
	bool ok=lv.hit>=roleLow[role] && lv.hit<=roleHigh[role] && lv.peakDb<-3.0;
	printf("%-6s %-12s %-4s vol %02X M %6.1f hit %6.1f pk %6.1f | at 80: M %6.1f hit %6.1f [%3.0f,%3.0f] %s\n",
	       kind,name,roleNames[role],volume,lv.lufs,lv.hit,lv.peakDb,raw.lufs,raw.hit,
	       roleLow[role],roleHigh[role],ok?"ok":"OUT");
	if (!ok) {
		// The volume that would put it in the middle of its window
		double target=0.5*(roleLow[role]+roleHigh[role]);
		double v=volume*pow(10.0,(target-lv.hit)/20.0);
		printf("       -> volume %02X would read %.1f\n",v>255?255:(int)(v+0.5),
		       v>255?lv.hit+20.0*log10(255.0/volume):target);
	}
	fflush(stdout);
	if (!ok) failures++;
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
	std::vector<float> L,R;

	// Reference: a full-scale sine on both channels reads about +0.1 LUFS
	{
		std::vector<float> a,b;
		for (int i=0;i<(int)RATE;i++) {
			float v=(float)sin(2.0*M_PI*997.0*i/RATE);
			a.push_back(v);
			b.push_back(v);
		}
		Level ref=measure(a,b);
		printf("reference full-scale 997 Hz stereo sine: %.2f LUFS\n",ref.lufs);
		if (fabs(ref.lufs)>1.0) failures++;
	}

	if (getenv("MODEL_SWEEP")) {
		// Every Braids shape from INIT (sustain full), volume 80
		for (int shape=0;shape<MACRO_SHAPE_COUNT;shape++) {
			MacroInstrument *m=new MacroInstrument();
			m->Init();
			m->LoadPreset("init");
			m->FindVariable("shape")->SetInt(shape);
			m->FindVariable("sustain")->SetInt(0xFF);
			play(m,L,R);
			Level lv=measure(L,R);
			printf("shape %2d %-8s M %6.1f hit %6.1f pk %6.1f\n",shape,
			       m->FindVariable("shape")->GetString(),lv.lufs,lv.hit,lv.peakDb);
			delete m;
		}
		return 0;
	}
	int count=SynthInstrument::GetPresetCount();
	for (int p=0;p<count;p++) {
		const char *name=SynthInstrument::GetPresetName(p);
		SynthInstrument *s=new SynthInstrument();
		s->Init();
		s->LoadPreset(name);
		play(s,L,R);
		Level lv=measure(L,R);
		int vol=s->FindVariable("volume")->GetInt();
		s->FindVariable("volume")->SetInt(0x80);
		play(s,L,R);
		check(SynthInstrument::GetEngineName(SynthInstrument::GetPresetEngine(p)),name,lv,vol,measure(L,R));
		delete s;
	}
	int macros=MacroInstrument::GetPresetCount();
	for (int p=0;p<macros;p++) {
		const char *name=MacroInstrument::GetPresetName(p);
		MacroInstrument *m=new MacroInstrument();
		m->Init();
		m->LoadPreset(name);
		play(m,L,R);
		Level lv=measure(L,R);
		int vol=m->FindVariable("volume")->GetInt();
		m->FindVariable("volume")->SetInt(0x80);
		play(m,L,R);
		check("macro",name,lv,vol,measure(L,R));
		delete m;
	}
	printf("%s: %d preset(s) out of their loudness window\n",failures?"FAILED":"ALL OK",failures);
	return failures?1:0;
}
