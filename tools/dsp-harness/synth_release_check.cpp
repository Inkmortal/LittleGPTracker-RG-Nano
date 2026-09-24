// Runs the real SynthInstrument, built for the device, through a play /
// KILL / stop sequence and checks every released voice actually dies.
// Build + run under qemu-arm: wsl bash tools/dsp-harness/run.sh <this file>
#include "Application/Instruments/SynthInstrument.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "System/FileSystem/FileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <stdio.h>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static fixed buffer[2048*2];
static const int FRAMES=1199; // what the device mixes per buffer

static void render(SynthInstrument &s,int ch,int buffers,bool tick) {
	for (int i=0;i<buffers;i++) s.Render(ch,buffer,FRAMES,tick);
}

static bool check(SynthInstrument &s,int ch,const char *what) {
	int stage; float level;
	s.GetVoiceDebug(ch,stage,level);
	bool ok=(stage<=0);
	printf("%-40s stage=%d level=%.5f releasing=%d %s\n",what,stage,level,
	       s.IsReleasing(ch)?1:0,ok?"ok":"STUCK");
	fflush(stdout);
	return ok;
}

static void set(SynthInstrument *s,const char *name,int value) {
	Variable *v=s->FindVariable(name);
	if (!v) { fprintf(stderr,"no variable %s\n",name); return; }
	v->SetInt(value);
}

static SynthInstrument *make(const char *preset,int volume,bool pad) {
	SynthInstrument *s=new SynthInstrument();
	s->Init();
	Variable *p=s->FindVariable("preset");
	if (p) p->SetString(preset);
	set(s,"volume",volume);
	if (pad) {
		set(s,"shape",64);
		set(s,"attack",160);
		set(s,"sustain",176);
		set(s,"release",156);
		set(s,"cutoff",104);
		set(s,"glide",4);
		set(s,"reverb",160);
		Variable *d=s->FindVariable("lfo dest");
		if (d) d->SetString("pitch");
		set(s,"lfo rate",152);
		set(s,"lfo amount",6);
	}
	return s;
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

	int failures=0;
	for (int k=0;k<2;k++) {
		bool pad=(k==0);
		SynthInstrument *s=pad?make("pad",56,true):make("subbass",80,false);
		int ch=pad?4:3;
		printf("--- %s\n",pad?"pad (07)":"subbass (06)");

		s->Start(ch,48,true); render(*s,ch,20,true);
		s->Stop(ch); render(*s,ch,200,false);
		failures+=!check(*s,ch,"note, KILL, tail rendered");

		s->Start(ch,48,true); render(*s,ch,20,true);
		s->StopQuickly(ch); render(*s,ch,200,false);
		failures+=!check(*s,ch,"note, transport stop");

		s->Start(ch,48,true); render(*s,ch,5,true);
		s->Stop(ch); render(*s,ch,2,false);
		s->StopQuickly(ch); render(*s,ch,200,false);
		failures+=!check(*s,ch,"note, KILL, then transport stop");

		s->Start(ch,48,true); render(*s,ch,5,true);
		s->Start(ch,55,true); render(*s,ch,1,true);
		s->StopQuickly(ch); render(*s,ch,200,false);
		failures+=!check(*s,ch,"note stolen mid-fade, transport stop");
	}
	if (failures) printf("FAILED: %d stuck\n",failures);
	else printf("all voices released\n");
	return failures?1:0;
}
