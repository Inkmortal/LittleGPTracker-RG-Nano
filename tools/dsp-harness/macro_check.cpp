// Macro synth (MacroInstrument + the ported macro oscillator), built for the
// device and run under qemu-arm:
//  - tuning: A4 (and A2/A6) on several shapes, from zero crossings of the
//    real output, within 3 cents
//  - every shape renders finite, audible output with the default knobs
//  - every preset is audible and stays below full scale
//  - degrade/redux change the sound and stay finite; KILL ends the voice
//  - CPU: 8 voices of each shape per mixer buffer, next to 8 synth voices
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static int failures=0;
static const int FRAMES=1199;  // what the device mixes per buffer
static fixed buffer[2048*2];

static void set(I_Instrument *s,const char *name,int value) {
	Variable *v=s->FindVariable(name);
	if (!v) { fprintf(stderr,"no variable %s\n",name); failures++; return; }
	v->SetInt(value);
}

static MacroInstrument *make(int shape,int timbre,int color) {
	MacroInstrument *m=new MacroInstrument();
	m->Init();
	set(m,"shape",shape);
	set(m,"timbre",timbre);
	set(m,"color",color);
	set(m,"sustain",255);
	set(m,"volume",255);
	return m;
}

// Renders 'seconds' of channel 0 into out (left channel, -1..1)
static int renderNote(I_Instrument *s,int note,float seconds,float *out,int maxOut) {
	s->Start(0,note,true);
	int total=(int)(seconds*44100.0f);
	if (total>maxOut) total=maxOut;
	int n=0;
	while (n<total) {
		int count=FRAMES;
		if (count>total-n) count=total-n;
		s->Render(0,buffer,count,true);
		for (int i=0;i<count;i++) out[n+i]=fp2fl(buffer[i*2])/32767.0f;
		n+=count;
	}
	return n;
}

static float wave[44100*3];

// Frequency from rising zero crossings (after a DC-removing highpass and a
// gentle lowpass so overtones don't add crossings), interpolated in time
static double measureFreq(const float *x,int n,double lowpassHz) {
	double a=exp(-2.0*M_PI*lowpassHz/44100.0);
	double lp=0.0,lp2=0.0,dc=0.0,prev=0.0;
	double first=-1.0,last=-1.0;
	int crossings=0;
	bool armed=false;
	double peak=0.0;
	int start=n/4;  // skip the attack
	for (int i=0;i<n;i++) {
		dc+=(x[i]-dc)*0.001;
		lp=lp*a+(x[i]-dc)*(1.0-a);
		lp2=lp2*a+lp*(1.0-a);
		if (i>=start && fabs(lp2)>peak) peak=fabs(lp2);
	}
	lp=lp2=0.0;dc=0.0;
	double hyst=peak*0.2;
	for (int i=0;i<n;i++) {
		dc+=(x[i]-dc)*0.001;
		lp=lp*a+(x[i]-dc)*(1.0-a);
		lp2=lp2*a+lp*(1.0-a);
		if (i>=start) {
			if (lp2<-hyst) armed=true;
			if (armed && prev<0.0 && lp2>=0.0) {
				double t=(i-1)+prev/(prev-lp2);
				if (first<0.0) first=t; else crossings++;
				last=t;
				armed=false;
			}
		}
		prev=lp2;
	}
	if (crossings<2) return 0.0;
	return 44100.0*crossings/(last-first);
}

static double rms(const float *x,int n,int from) {
	double s=0.0;
	for (int i=from;i<n;i++) s+=x[i]*x[i];
	return sqrt(s/(n-from>0?n-from:1));
}

static bool allFinite(const float *x,int n) {
	for (int i=0;i<n;i++) if (x[i]!=x[i] || x[i]>4.0f || x[i]<-4.0f) return false;
	return true;
}

static double nowUs() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return ts.tv_sec*1e6+ts.tv_nsec/1e3;
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

	// --- Tuning
	printf("--- tuning (target 440 Hz at A4 = note 69)\n");
	struct { int shape; int timbre; int color; int note; double lowpass; } tunes[]={
		{MS_MORPH,0,0,69,2000},{MS_CSAW,0,0,69,1000},{MS_SAWSQUARE,0,0,69,1000},
		{MS_FOLD,0,0,69,2000},{MS_FM,0,0,69,2000},{MS_WTBL,0,0,69,500},
		{MS_BUZZ,0,0,69,1000},{MS_PLUCK,0x20,0x80,69,1000},
		{MS_MORPH,0,0,45,500},{MS_MORPH,0,0,93,6000},
	};
	for (unsigned t=0;t<sizeof(tunes)/sizeof(tunes[0]);t++) {
		MacroInstrument *m=make(tunes[t].shape,tunes[t].timbre,tunes[t].color);
		int n=renderNote(m,tunes[t].note,1.5f,wave,44100*3);
		double want=440.0*pow(2.0,(tunes[t].note-69)/12.0);
		double got=measureFreq(wave,n,tunes[t].lowpass);
		double cents=got>0.0?1200.0*log(got/want)/log(2.0):9999.0;
		// Oscillators: 3 cents. The plucked string keeps the module's own
		// delay-line tuning: 25
		bool ok=fabs(cents)<(tunes[t].shape==MS_PLUCK?25.0:3.0);
		printf("%-8s note %3d  %9.3f Hz  want %9.3f  %+6.2f cents %s\n",
		       MacroInstrument::GetShapeName(tunes[t].shape),tunes[t].note,got,want,cents,ok?"ok":"WRONG");
		if (!ok) failures++;
		delete m;
	}

	// --- Every shape: finite, audible
	printf("--- every shape at C4, timbre/color 80\n");
	int broken0=MacroInstrument::BrokenVoiceCount();
	for (int shape=0;shape<MACRO_SHAPE_COUNT;shape++) {
		MacroInstrument *m=make(shape,0x80,0x80);
		int n=renderNote(m,60,0.6f,wave,44100*3);
		double level=rms(wave,n,0);
		double db=20.0*log10(level+1e-9);
		bool finite=allFinite(wave,n);
		bool ok=finite && db>-45.0;
		printf("%-8s rms %6.1f dBFS %s%s\n",MacroInstrument::GetShapeName(shape),db,
		       finite?"":"NOT FINITE ",ok?"ok":"WRONG");
		if (!ok) failures++;
		delete m;
	}
	if (MacroInstrument::BrokenVoiceCount()!=broken0) {
		printf("voices shut down as broken: %d WRONG\n",MacroInstrument::BrokenVoiceCount()-broken0);
		failures++;
	}

	// --- Presets: audible, under full scale
	printf("--- presets at C3 (note 48), 1.5 s\n");
	for (int p=0;p<MacroInstrument::GetPresetCount();p++) {
		MacroInstrument *m=new MacroInstrument();
		m->Init();
		m->LoadPreset(MacroInstrument::GetPresetName(p));
		int n=renderNote(m,48,1.5f,wave,44100*3);
		double level=rms(wave,n,0);
		double peak=0.0;
		for (int i=0;i<n;i++) if (fabs(wave[i])>peak) peak=fabs(wave[i]);
		bool ok=allFinite(wave,n) && level>0.003 && peak<1.0;
		printf("%-10s rms %6.1f dBFS peak %6.1f dBFS %s\n",MacroInstrument::GetPresetName(p),
		       20.0*log10(level+1e-9),20.0*log10(peak+1e-9),ok?"ok":"WRONG");
		if (!ok) failures++;
		delete m;
	}

	// --- Degrade / redux
	printf("--- degrade / redux\n");
	{
		static float clean[44100];
		MacroInstrument *m=make(MS_MORPH,0x40,0x40);
		int n=renderNote(m,57,0.5f,clean,44100);
		delete m;
		const int knobs[3][2]={{0xC0,0},{0,0xC0},{0xFF,0xFF}};
		for (int k=0;k<3;k++) {
			m=make(MS_MORPH,0x40,0x40);
			set(m,"degrade",knobs[k][0]);
			set(m,"redux",knobs[k][1]);
			renderNote(m,57,0.5f,wave,44100);
			double diff=0.0;
			for (int i=0;i<n;i++) diff+=(wave[i]-clean[i])*(wave[i]-clean[i]);
			diff=sqrt(diff/n);
			bool ok=allFinite(wave,n) && diff>0.01;
			printf("degrade %02X redux %02X: differs by %.4f rms %s\n",knobs[k][0],knobs[k][1],diff,ok?"ok":"WRONG");
			if (!ok) failures++;
			delete m;
		}
	}

	// --- KILL (release) and transport stop end the voice
	printf("--- release\n");
	{
		const int shapes[3]={MS_SWARM,MS_BOWED,MS_CLOUD};
		for (int k=0;k<3;k++) {
			MacroInstrument *m=make(shapes[k],0x80,0x80);
			set(m,"release",0x90);
			m->Start(2,60,true);
			for (int b=0;b<10;b++) m->Render(2,buffer,FRAMES,true);
			if (k==2) m->StopQuickly(2); else m->Stop(2);
			for (int b=0;b<200;b++) m->Render(2,buffer,FRAMES,false);
			int stage; float level;
			m->GetVoiceDebug(2,stage,level);
			bool ok=stage<0;
			printf("%-8s %s: stage=%d level=%.5f %s\n",MacroInstrument::GetShapeName(shapes[k]),
			       k==2?"transport stop":"KILL",stage,level,ok?"ok":"STUCK");
			if (!ok) failures++;
			delete m;
		}
	}

	// --- CPU: 8 voices, one mixer buffer each round
	printf("--- CPU, 8 voices, %d frames per buffer (%.1f ms of audio)\n",FRAMES,FRAMES*1000.0/44100.0);
	{
		const int rounds=40;
		SynthInstrument *s=new SynthInstrument();
		s->Init();
		Variable *p=s->FindVariable("preset");
		p->SetString("pad");
		set(s,"sustain",255);
		for (int c=0;c<8;c++) s->Start(c,48+c,true);
		for (int c=0;c<8;c++) s->Render(c,buffer,FRAMES,true);
		double t0=nowUs();
		for (int r=0;r<rounds;r++) {
			for (int c=0;c<8;c++) s->Render(c,buffer,FRAMES,false);
		}
		double synthUs=(nowUs()-t0)/rounds;
		printf("synth pad (reference)   %8.0f us per buffer\n",synthUs);
		delete s;

		double worst=0.0,sum=0.0;
		int worstShape=0;
		for (int shape=0;shape<MACRO_SHAPE_COUNT;shape++) {
			MacroInstrument *m=make(shape,0x80,0x80);
			for (int c=0;c<8;c++) m->Start(c,48+c,true);
			for (int c=0;c<8;c++) m->Render(c,buffer,FRAMES,true);
			t0=nowUs();
			for (int r=0;r<rounds;r++) {
				for (int c=0;c<8;c++) m->Render(c,buffer,FRAMES,false);
			}
			double us=(nowUs()-t0)/rounds;
			printf("%-8s %8.0f us per buffer  %4.1fx synth\n",MacroInstrument::GetShapeName(shape),us,us/synthUs);
			sum+=us;
			if (us>worst) { worst=us; worstShape=shape; }
			delete m;
		}
		printf("macro average %8.0f us, worst %s %8.0f us (%.1fx / %.1fx the synth)\n",
		       sum/MACRO_SHAPE_COUNT,MacroInstrument::GetShapeName(worstShape),worst,
		       sum/MACRO_SHAPE_COUNT/synthUs,worst/synthUs);
		printf("buffer lasts %8.0f us of audio (qemu timing: compare the ratios)\n",FRAMES*1e6/44100.0);
	}

	if (failures) printf("FAILED: %d\n",failures);
	else printf("macro synth ok\n");
	return failures?1:0;
}
