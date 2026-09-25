// The synth engines (FM4, HYPER, WAV) built for the device and run under
// qemu-arm, measured from their real output:
//  - FM4 carriers play the note's pitch at ratio 1 (and 2, 0.5), counted
//    from zero crossings
//  - one modulator gives the sidebands phase modulation predicts (Bessel)
//  - the twelve algorithms all sound different; additive mode (0B) has only
//    its four partials; feedback turns a sine into a saw-like wave
//  - HYPER's two saws per note are detuned by exactly the swarm spread and
//    split left/right by width (peak frequency of each channel)
//  - WAV's mirror sets the pulse width; its pitch is right
//  - a project round trip keeps every engine knob; old songs stay "synth"
//  - render cost of 8 voices per engine (time per 1199-frame buffer)
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/SynthInstrument.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "System/FileSystem/FileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <string>
#include <vector>

extern fixed panlaw[] ;

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

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-46s got=%10.4f want=%10.4f %s\n",what,got,want,ok?"ok":"WRONG");
	fflush(stdout);
	if (!ok) failures++;
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
	// steady level: no envelope movement while we measure
	set(s,"attack",0);
	set(s,"sustain",255);
	set(s,"volume",0x80);
	set(s,"pan",0x7F);
	set(s,"reverb",0);
	set(s,"delay",0);
	set(s,"chorus",0);
	return s;
}

// Renders 'seconds' of one note after 0.2 s of settling; left/right out
static void play(SynthInstrument *s, int note, double seconds,
                 std::vector<float> &left, std::vector<float> &right, int channel=0) {
	s->Start(channel,(unsigned char)note,true);
	int skip=(int)(0.2*RATE/FRAMES)+1;
	for (int b=0;b<skip;b++) s->Render(channel,buffer,FRAMES,b==0);
	int total=(int)(seconds*RATE);
	left.clear();
	right.clear();
	while ((int)left.size()<total) {
		s->Render(channel,buffer,FRAMES,false);
		for (int i=0;i<FRAMES;i++) {
			left.push_back(fp2fl(buffer[i*2])/32767.0f);
			right.push_back(fp2fl(buffer[i*2+1])/32767.0f);
		}
	}
	left.resize(total);
	right.resize(total);
	s->StopQuickly(channel);
	for (int b=0;b<40;b++) s->Render(channel,buffer,FRAMES,false);
}

// Frequency from positive-going zero crossings (interpolated)
static double crossingHz(const std::vector<float> &x) {
	double first=-1,last=-1;
	int count=0;
	for (size_t i=1;i<x.size();i++) {
		if (x[i-1]<0.0f && x[i]>=0.0f) {
			double t=(i-1)+x[i-1]/(x[i-1]-x[i]);
			if (first<0) first=t;
			last=t;
			count++;
		}
	}
	if (count<2) return 0;
	return (count-1)*RATE/(last-first);
}

// Amplitude of a sine at hz (Hann window, normalised so a full sine = 1).
// The window is cached per length; the phasor turns by multiplication.
static double amplitudeAt(const std::vector<float> &x, double hz) {
	static std::vector<double> win;
	static double winSum=0;
	size_t n=x.size();
	if (win.size()!=n) {
		win.resize(n);
		winSum=0;
		for (size_t i=0;i<n;i++) {
			win[i]=0.5-0.5*cos(2.0*M_PI*i/(n-1));
			winSum+=win[i];
		}
	}
	double w=2.0*M_PI*hz/RATE;
	double c=cos(w),s=sin(w);
	double pr=1.0,pi=0.0;
	double re=0,im=0;
	for (size_t i=0;i<n;i++) {
		double v=x[i]*win[i];
		re+=v*pr;
		im+=v*pi;
		double t=pr*c-pi*s;
		pi=pr*s+pi*c;
		pr=t;
		if ((i&1023)==1023) {
			// keep the phasor on the unit circle
			double m=sqrt(pr*pr+pi*pi);
			pr/=m;
			pi/=m;
		}
	}
	return 2.0*sqrt(re*re+im*im)/winSum;
}

// Loudest frequency between lo and hi (fine scan, then parabola)
static double peakHz(const std::vector<float> &x, double lo, double hi, double step) {
	double best=lo,bestA=-1;
	int steps=(int)((hi-lo)/step);
	std::vector<double> a(steps+1);
	int bi=0;
	for (int k=0;k<=steps;k++) {
		a[k]=amplitudeAt(x,lo+k*step);
		if (a[k]>bestA) { bestA=a[k]; bi=k; }
	}
	best=lo+bi*step;
	if (bi>0 && bi<steps) {
		double y0=a[bi-1],y1=a[bi],y2=a[bi+1];
		double d=0.5*(y0-y2)/(y0-2*y1+y2);
		best+=d*step;
	}
	return best;
}

static double besselJ(int n, double x) {
	// power series, plenty for x < 4
	double sum=0,term=1;
	for (int k=1;k<=n;k++) term*=x/2.0/k;
	for (int k=0;k<40;k++) {
		sum+=term;
		term*=-(x*x/4.0)/((k+1)*(double)(k+1+n));
	}
	return sum;
}

static double harmonicsDistance(const double *a, const double *b, int n) {
	double d=0;
	for (int k=0;k<n;k++) d+=fabs(a[k]-b[k]);
	return d;
}

static double nowSeconds() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return ts.tv_sec+ts.tv_nsec*1e-9;
}

// 8 voices of one preset (a chord spread over the channels), time per buffer
static double timeEngine(const char *preset, const char *chrd, double &peak) {
	SynthInstrument *s=new SynthInstrument();
	s->Init();
	setString(s,"preset",preset);
	static const int notes[8]={48,52,55,59,60,64,67,71};
	for (int ch=0;ch<8;ch++) {
		s->Start(ch,(unsigned char)notes[ch],true);
		if (chrd) s->ProcessCommand(ch,MAKE_FOURCC('C','H','R','D'),(ushort)strtol(chrd,0,16));
	}
	const int buffers=120;
	peak=0;
	// warm up
	for (int ch=0;ch<8;ch++) s->Render(ch,buffer,FRAMES,true);
	double start=nowSeconds();
	for (int b=0;b<buffers;b++) {
		for (int ch=0;ch<8;ch++) {
			s->Render(ch,buffer,FRAMES,(b%4)==0);
			if (b==buffers-1) {
				for (int i=0;i<FRAMES*2;i++) {
					double v=fabs(fp2fl(buffer[i])/32767.0f);
					if (v>peak) peak=v;
				}
			}
		}
	}
	double perBuffer=(nowSeconds()-start)/buffers;
	delete s;
	return perBuffer;
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
	fprintf(stderr,"setup ok\n");
	std::vector<float> L,R;

	printf("--- FM4 carrier pitch (D alone, zero crossings)\n");
	{
		SynthInstrument *s=make("fm init");
		set(s,"fm c level",0);
		play(s,69,1.0,L,R);
		double hz=crossingHz(L);
		expect(fabs(hz-440.0)<0.05,"A4, ratio 1.00 -> 440 Hz",hz,440.0);
		play(s,45,1.0,L,R);
		hz=crossingHz(L);
		expect(fabs(hz-110.0)<0.02,"A2, ratio 1.00 -> 110 Hz",hz,110.0);
		set(s,"fm d ratio",200);
		play(s,69,1.0,L,R);
		hz=crossingHz(L);
		expect(fabs(hz-880.0)<0.1,"A4, ratio 2.00 -> 880 Hz",hz,880.0);
		set(s,"fm d ratio",50);
		play(s,69,1.0,L,R);
		hz=crossingHz(L);
		expect(fabs(hz-220.0)<0.03,"A4, ratio 0.50 -> 220 Hz",hz,220.0);
		// Another operator as the carrier: additive, only B heard
		set(s,"fm d level",0);
		set(s,"fm b level",255);
		setString(s,"fm algo","0B");
		play(s,69,1.0,L,R);
		hz=crossingHz(L);
		expect(fabs(hz-440.0)<0.05,"algo 0B, B alone at ratio 1 -> 440 Hz",hz,440.0);
		delete s;
	}

	printf("--- FM4 sidebands: C (ratio 3) modulating D, vs Bessel\n");
	{
		SynthInstrument *s=make("fm init");
		set(s,"fm c ratio",300);
		set(s,"fm c level",0x60);
		play(s,45,1.0,L,R);
		double f=110.0;
		double index=FM4_MOD_DEPTH*2.0*M_PI*fm4LevelAmp(0x60);
		double j0=fabs(besselJ(0,index)),j1=fabs(besselJ(1,index)),j2=fabs(besselJ(2,index));
		double h[9];
		for (int k=1;k<=8;k++) h[k]=amplitudeAt(L,k*f);
		printf("index %.3f rad: J0 %.3f J1 %.3f J2 %.3f\n",index,j0,j1,j2);
		expect(fabs(h[2]/h[1]-j1/j0)<0.03*j1/j0,"harmonic 2 / 1 = J1/J0",h[2]/h[1],j1/j0);
		expect(fabs(h[4]/h[1]-j1/j0)<0.03*j1/j0,"harmonic 4 / 1 = J1/J0",h[4]/h[1],j1/j0);
		expect(fabs(h[5]/h[1]-j2/j0)<0.05*j2/j0,"harmonic 5 / 1 = J2/J0",h[5]/h[1],j2/j0);
		expect(fabs(h[7]/h[1]-j2/j0)<0.05*j2/j0,"harmonic 7 / 1 = J2/J0",h[7]/h[1],j2/j0);
		expect(h[3]/h[1]<0.002,"harmonic 3 absent",h[3]/h[1],0.0);
		expect(h[6]/h[1]<0.002,"harmonic 6 absent",h[6]/h[1],0.0);
		// A MOD slot aimed at "fm" deepens every modulator (+127 = twice)
		setString(s,"mod1 type","ahd");
		setString(s,"mod1 dest","fm amt");
		set(s,"mod1 amount",127);
		set(s,"mod1 p2",0xFF);   // hold
		set(s,"mod1 p3",0xFF);   // decay
		printf("mod1: %s -> %s %s, p1 %s p2 %s p3 %s\n",s->FindVariable("mod1 type")->GetString(),
		       s->FindVariable("mod1 dest")->GetString(),s->FindVariable("mod1 amount")->GetString(),
		       s->FindVariable("mod1 p1")->GetString(),s->FindVariable("mod1 p2")->GetString(),
		       s->FindVariable("mod1 p3")->GetString());
		play(s,45,0.5,L,R);
		double index2=2.0*index;
		double m0=amplitudeAt(L,f),m1=amplitudeAt(L,2*f);
		double want=fabs(besselJ(1,index2)/besselJ(0,index2));
		expect(fabs(m1/m0-want)<0.05*want,"MOD fm +127: index doubled (J1/J0)",m1/m0,want);
		delete s;
	}

	printf("--- FM4 algorithms\n");
	{
		SynthInstrument *s=make("fm init");
		const int ratios[4]={100,200,300,400};
		const char *ops="abcd";
		char name[32];
		for (int op=0;op<4;op++) {
			sprintf(name,"fm %c ratio",ops[op]);
			set(s,name,ratios[op]);
			sprintf(name,"fm %c level",ops[op]);
			set(s,name,0x70);
		}
		static const char *algoNames[12]={"00","01","02","03","04","05","06","07","08","09","0A","0B"};
		double spectra[12][16];
		for (int a=0;a<12;a++) {
			setString(s,"fm algo",algoNames[a]);
			play(s,45,0.5,L,R);
			double total=0;
			for (int k=0;k<16;k++) {
				spectra[a][k]=amplitudeAt(L,(k+1)*110.0);
				total+=spectra[a][k];
			}
			for (int k=0;k<16;k++) spectra[a][k]/=total;
			double above4=0;
			for (int k=4;k<16;k++) above4+=spectra[a][k];
			printf("algo %s: partials above the 4th %.3f of the sound\n",algoNames[a],above4);
		}
		double closest=1e9;
		int ca=0,cb=0;
		for (int a=0;a<12;a++) {
			for (int b=a+1;b<12;b++) {
				double d=harmonicsDistance(spectra[a],spectra[b],16);
				if (d<closest) { closest=d; ca=a; cb=b; }
			}
		}
		printf("closest pair: %s / %s\n",algoNames[ca],algoNames[cb]);
		expect(closest>0.02,"every algorithm has its own spectrum",closest,0.02);
		// Additive: 0B with the four partials at level FF
		for (int op=0;op<4;op++) {
			sprintf(name,"fm %c level",ops[op]);
			set(s,name,255);
		}
		setString(s,"fm algo","0B");
		play(s,45,0.5,L,R);
		double h1=amplitudeAt(L,110.0);
		double worst=0,over=0;
		for (int k=2;k<=4;k++) {
			double r=amplitudeAt(L,k*110.0)/h1;
			if (fabs(r-1.0)>worst) worst=fabs(r-1.0);
		}
		for (int k=5;k<=12;k++) {
			double r=amplitudeAt(L,k*110.0)/h1;
			if (r>over) over=r;
		}
		expect(worst<0.02,"0B: partials 1-4 equally loud",worst,0.0);
		expect(over<0.001,"0B: nothing above partial 4",over,0.0);
		// Each of the four carriers is a quarter of full scale (x volume 0x80
		// and the centre of the pan law)
		double vol=0x80/255.0*fp2fl(panlaw[0x7F]);
		expect(fabs(h1-0.25*vol)<0.01*vol,"0B: each carrier 1/4",h1/vol,0.25);
		// Chain 00: modulation reaches above the 4th partial
		setString(s,"fm algo","00");
		play(s,45,0.5,L,R);
		double hi=0;
		for (int k=5;k<=12;k++) hi+=amplitudeAt(L,k*110.0);
		expect(hi/amplitudeAt(L,110.0)>0.05,"00: A>B>C>D makes high sidebands",hi/amplitudeAt(L,110.0),0.05);
		delete s;
	}

	printf("--- FM4 feedback\n");
	{
		SynthInstrument *s=make("fm init");
		set(s,"fm c level",0);
		play(s,45,0.5,L,R);
		double clean=amplitudeAt(L,220.0)/amplitudeAt(L,110.0);
		set(s,"fm d feedback",255);
		play(s,45,0.5,L,R);
		double fed=amplitudeAt(L,220.0)/amplitudeAt(L,110.0);
		double fed3=amplitudeAt(L,330.0)/amplitudeAt(L,110.0);
		expect(clean<0.001,"no feedback: pure sine (2nd/1st)",clean,0.0);
		expect(fed>0.25 && fed3>0.1,"feedback FF: saw-like (2nd/1st)",fed,0.5);
		double fedHz=peakHz(L,105.0,115.0,0.05);
		expect(fabs(fedHz-110.0)<0.1,"feedback keeps the pitch (peak)",fedHz,110.0);
		delete s;
	}

	printf("--- HYPER detune and width\n");
	{
		SynthInstrument *s=make("hyper init");
		setString(s,"filter","off");
		set(s,"hyper note 1",0);
		set(s,"hyper note 2",12);
		set(s,"hyper note 3",24);
		set(s,"hyper shift",0);
		set(s,"hyper width",255);
		const int swarms[2]={255,0x80};
		for (int k=0;k<2;k++) {
			set(s,"hyper swarm",swarms[k]);
			play(s,57,2.0,L,R);
			double fl=peakHz(L,213.0,227.0,0.05);
			double fr=peakHz(R,213.0,227.0,0.05);
			double spread=1200.0*log(fr/fl)/log(2.0);
			double want=HyperDetuneCents(swarms[k],0);
			char what[64];
			sprintf(what,"swarm %02X: left/right saws apart (cents)",swarms[k]);
			expect(fabs(spread-want)<1.0,what,spread,want);
			sprintf(what,"swarm %02X: centred on the note (Hz)",swarms[k]);
			double centre=sqrt(fl*fr);
			expect(fabs(centre-220.0)<0.15,what,centre,220.0);
		}
		set(s,"hyper swarm",0);
		play(s,57,1.0,L,R);
		double hz=crossingHz(L);
		expect(fabs(peakHz(L,213.0,227.0,0.05)-220.0)<0.1,"swarm 00: in tune",peakHz(L,213.0,227.0,0.05),220.0);
		set(s,"hyper swarm",0xC0);
		set(s,"hyper width",0);
		play(s,57,0.5,L,R);
		double diff=0,power=0;
		for (size_t i=0;i<L.size();i++) {
			diff+=fabs(L[i]-R[i]);
			power+=fabs(L[i]);
		}
		expect(diff/power<1e-4,"width 00: mono (left = right)",diff/power,0.0);
		set(s,"hyper width",255);
		play(s,57,0.5,L,R);
		diff=0;power=0;
		for (size_t i=0;i<L.size();i++) {
			diff+=fabs(L[i]-R[i]);
			power+=fabs(L[i]);
		}
		expect(diff/power>0.3,"width FF: left and right differ",diff/power,0.3);
		(void)hz;
		delete s;
	}

	printf("--- WAV\n");
	{
		SynthInstrument *s=make("wav init");
		setString(s,"wav shape","pulse50");
		set(s,"wav mirror",0x40);
		play(s,57,1.0,L,R);
		int high=0;
		for (size_t i=0;i<L.size();i++) if (L[i]>0.0f) high++;
		double duty=(double)high/L.size();
		double want=0.5+(0x40-128)/127.0*0.48;
		expect(fabs(duty-want)<0.01,"pulse50 + mirror 40: pulse width",duty,want);
		expect(fabs(crossingHz(L)-220.0)<0.3,"pulse pitch A3",crossingHz(L),220.0);
		set(s,"wav mirror",0x80);
		play(s,57,1.0,L,R);
		high=0;
		for (size_t i=0;i<L.size();i++) if (L[i]>0.0f) high++;
		expect(fabs((double)high/L.size()-0.5)<0.01,"mirror 80: square",(double)high/L.size(),0.5);
		setString(s,"wav shape","sine");
		set(s,"wav size",0xFF);
		play(s,57,1.0,L,R);
		expect(fabs(crossingHz(L)-220.0)<0.1,"sine pitch A3",crossingHz(L),220.0);
		delete s;
	}

	printf("--- Save and restore (by name, in order, like a project file)\n");
	{
		SynthInstrument *a=make("epiano");
		set(a,"fm b ratio",201);
		setString(a,"fm algo","03");
		set(a,"hyper note 4",7);
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
		// A song saved before the engines: preset and synth knobs only
		SynthInstrument *old=new SynthInstrument();
		old->Init();
		setString(old,"preset","pad");
		setString(old,"cutoff","100");
		expect(old->GetEngine()==SE_SYNTH,"old song: engine synth",old->GetEngine(),SE_SYNTH);
		expect(old->FindVariable("cutoff")->GetInt()==100,"old song: its own cutoff",old->FindVariable("cutoff")->GetInt(),100);
		delete a;
		delete b;
		delete old;
	}

	printf("--- Render cost, 8 voices (buffer of %d frames = %.1f ms of audio)\n",FRAMES,FRAMES*1000.0/RATE);
	{
		struct { const char *preset; const char *chrd; const char *label; } runs[]={
			{"pad",0,"synth pad (supersaw)"},
			{"epiano",0,"fm4 epiano"},
			{"brass",0,"fm4 brass"},
			{"fm bass",0,"fm4 fm bass"},
			{"epiano","047B","fm4 epiano + CHRD 047B (16 ops)"},
			{"hyper pad",0,"hyper pad (12 saws + sub)"},
			{"hoover",0,"hyper hoover"},
			{"pwm pad",0,"wav pwm pad"},
			{"sync lead",0,"wav sync lead"},
		};
		double budget=FRAMES/RATE;
		for (unsigned int k=0;k<sizeof(runs)/sizeof(runs[0]);k++) {
			double peak;
			double t=timeEngine(runs[k].preset,runs[k].chrd,peak);
			printf("%-36s %8.3f ms/buffer  %5.1f%% of real time  peak %.2f\n",
			       runs[k].label,t*1000.0,100.0*t/budget,peak);
			if (!(peak>0.01 && peak<=2.0)) {
				printf("  output out of range\n");
				failures++;
			}
		}
	}

	if (failures) printf("FAILED: %d\n",failures);
	else printf("all engine checks passed\n");
	return failures?1:0;
}
