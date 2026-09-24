// Chorus send built for the device, run under qemu-arm: its knob curves,
// then a sine through the chorus alone. The wet signal must come back at
// about the input level, delayed (not instant), wobbling in pitch, and
// different on the left and right.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Mixer/SendFX.h"
#include "Application/Model/Mixer.h"
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
	Mixer::GetInstance()->Clear();

	expect(fabs(SendFX::ChorusRateFromParam(0)-0.1f)<0.001f,"rate 00 = 0.1 Hz",SendFX::ChorusRateFromParam(0),0.1);
	expect(fabs(SendFX::ChorusRateFromParam(255)-5.0f)<0.01f,"rate FF = 5 Hz",SendFX::ChorusRateFromParam(255),5.0);
	expect(fabs(SendFX::ChorusDepthMsFromParam(0)-0.5f)<0.001f,"depth 00 = 0.5 ms",SendFX::ChorusDepthMsFromParam(0),0.5);
	expect(fabs(SendFX::ChorusDepthMsFromParam(255)-7.5f)<0.01f,"depth FF = 7.5 ms",SendFX::ChorusDepthMsFromParam(255),7.5);

	const int rate=Audio::GetInstance()->GetSampleRate();
	const int frames=512;
	const int blocks=(rate*4)/frames;   // four seconds, a full LFO cycle
	SendFX *fx=SendFX::GetInstance();
	float in[frames*2];
	fixed out[frames*2];
	double inPower=0,outPower=0,diffPower=0;
	int firstSound=-1;
	// The left delay, measured every 10 ms from the phase of the 440 Hz sine
	const double w=6.28318530718*440.0/rate;
	const int window=rate/100;
	double I=0,Q=0,lastPhase=0,unwrap=0,lagMin=1e9,lagMax=-1e9;
	bool havePhase=false;
	bool finite=true;
	long n=0;
	for (int b=0;b<blocks;b++) {
		for (int i=0;i<frames;i++) {
			float s=0.5f*(float)sin(6.28318530718*440.0*(n+i)/rate);
			in[i*2]=in[i*2+1]=s;
		}
		fx->AddSend(0,in,frames,0.0f,0.0f,1.0f);
		fx->Render(out,frames);
		for (int i=0;i<frames;i++,n++) {
			float l=fp2fl(out[i*2])/32767.0f;
			float r=fp2fl(out[i*2+1])/32767.0f;
			if (l!=l || r!=r) finite=false;
			if (firstSound<0 && fabs(l)>0.01f) firstSound=(int)n;
			if (n>rate/10) {
				inPower+=in[i*2]*in[i*2];
				outPower+=l*l;
				diffPower+=(l-r)*(l-r);
			}
			I+=l*sin(w*n);
			Q+=l*cos(w*n);
			if ((n+1)%window==0) {
				double phase=atan2(Q,I);
				if (havePhase) {
					double d=phase-lastPhase;
					while (d>3.14159265) d-=6.28318530718;
					while (d<-3.14159265) d+=6.28318530718;
					unwrap+=d;
				}
				lastPhase=phase;
				havePhase=true;
				if (n>rate/10) {
					double lagMs=-unwrap/w*1000.0/rate;
					if (lagMs<lagMin) lagMin=lagMs;
					if (lagMs>lagMax) lagMax=lagMs;
				}
				I=Q=0;
			}
		}
	}
	double level=sqrt(outPower/inPower);
	double width=sqrt(diffPower/outPower);
	double firstMs=firstSound*1000.0/rate;
	expect(finite,"output is finite",finite?1:0,1);
	expect(firstMs>11.5 && firstMs<21.0,"first wet sound after the base delay (ms)",firstMs,12.0);
	expect(level>0.8 && level<1.2,"wet level ~ input level",level,1.0);
	expect(width>0.05,"left and right differ",width,0.05);
	// Default depth 70 sweeps 0.5+7*0x70/255 = 3.6 ms; four seconds at the
	// default speed (0.27 Hz) cover a full cycle
	expect(lagMax-lagMin>2.0 && lagMax-lagMin<4.0,"delay sweeps (ms, peak to peak)",lagMax-lagMin,3.6);

	printf("%s\n",failures?"FAILED":"ALL OK");
	return failures?1:0;
}
