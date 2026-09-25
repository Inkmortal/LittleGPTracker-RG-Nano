// Synth pages of the Instrument screen: SOUND, ENV, FILTER, LFO, MOD, MIX, EQ.
// Each page draws a live picture of what its knobs do, plus a plain-English
// explanation of the focused knob with real units (ms, Hz, semitones).

#include "Application/AppWindow.h"
#include "InstrumentView.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Mixer/SendFX.h"
#include "Application/Player/Player.h"
#include "BaseClasses/UIIntVarField.h"
#include "BaseClasses/UIIntVarOffField.h"
#include "Foundation/Variables/Variable.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

static int synthInt(I_Instrument *instr, FourCC id) {
	Variable *v=instr?instr->FindVariable(id):0;
	return v?v->GetInt():0;
}

static void formatSeconds(char *out, float seconds) {
	if (seconds<1.0f) {
		sprintf(out,"%d ms",(int)(seconds*1000.0f+0.5f));
	} else {
		sprintf(out,"%.1f s",seconds);
	}
}

static void formatHz(char *out, float hz) {
	if (hz<1000.0f) {
		sprintf(out,"%d Hz",(int)(hz+0.5f));
	} else {
		sprintf(out,"%.1f kHz",hz/1000.0f);
	}
}

const char *InstrumentView::getSynthPageName() {
	switch(labPage_) {
		case 0: return "SOUND";
		case 1: return "ENV";
		case 2: return "FILTER";
		case 3: return getInstrumentType()==IT_MACRO?"MOTION":"LFO";
		case INSTRUMENT_MOD_PAGE: return "MOD";
		case INSTRUMENT_EQ_PAGE: return "EQ";
		default: return "MIX";
	}
}

void InstrumentView::addTypeField(GUIPoint &position) {
	int i=viewData_->currentInstrument_;
	if (i>=MAX_SAMPLEINSTRUMENT_COUNT) {
		return;
	}
	InstrumentType type=getInstrumentType();
	typeVar_->SetInt(type==IT_MACRO?2:(type==IT_SYNTH?1:0),false);
	UIIntVarField *f=new UIIntVarField(position,*typeVar_,"type   %s",0,2,1,1);
	T_SimpleList<UIField>::Insert(f);
	position._y+=1;
}

// Applies a change of the view-owned "type" field once field processing is
// over (the instrument object is replaced, so every field gets rebuilt).
bool InstrumentView::applyTypeChange() {
	int i=viewData_->currentInstrument_;
	if (i>=MAX_SAMPLEINSTRUMENT_COUNT) {
		return false;
	}
	static const InstrumentType types[3]={IT_SAMPLE,IT_SYNTH,IT_MACRO};
	int index=typeVar_->GetInt();
	InstrumentType wanted=types[(index<0 || index>2)?0:index];
	if (wanted==getInstrumentType()) {
		return false;
	}
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
	if (current_) {
		current_->RemoveObserver(*this);
	}
	current_=0;
	ClearFocus();
	T_SimpleList<UIField>::Empty();
	bank->SetInstrumentType(i,wanted);
	if (wanted==IT_SYNTH) {
		((SynthInstrument *)bank->GetInstrument(i))->LoadPreset("init");
	} else if (wanted==IT_MACRO) {
		((MacroInstrument *)bank->GetInstrument(i))->LoadPreset("init");
	}
	labPage_=0;
	lastFocusID_=INSTRUMENT_TYPE_FIELD;
	onInstrumentChange();
	isDirty_=true;
	return true;
}

void InstrumentView::fillSynthParameters() {
	int i=viewData_->currentInstrument_;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
	I_Instrument *s=bank->GetInstrument(i);
	GUIPoint position=GetAnchor();
	position._x=1;
	position._y=14;
	if (isMacroPage()) {
		fillMacroPage(s,position);
		return;
	}
	UIIntVarField *f;
	Variable *v;

#define SYNTH_FIELD(id,fmt,mn,mx,x,y) \
	v=s->FindVariable(id); \
	f=new UIIntVarField(position,*v,fmt,mn,mx,x,y); \
	T_SimpleList<UIField>::Insert(f); \
	position._y+=1;

	// The macro synth shares these pages but has no engine knob
	bool synth=(getInstrumentType()==IT_SYNTH);
	int engine=synth?((SynthInstrument *)s)->GetEngine():SE_SYNTH;
	int firstPreset,lastPreset;
	SynthInstrument::GetPresetRange(engine,firstPreset,lastPreset);
	shownEngine_=synth?engine:-1;

	switch(labPage_) {
		case 0:
			addTypeField(position);
			SYNTH_FIELD(SYP_ENGINE,"engine %s",0,SE_LAST-1,1,1);
			// A+Left/Right browses this engine's presets
			SYNTH_FIELD(SYP_PRESET,"preset %s",firstPreset,lastPreset,1,1);
			if (engine!=SE_SYNTH) {
				fillEngineSoundPage((SynthInstrument *)s,position,engine);
				break;
			}
			SYNTH_FIELD(SYP_WAVE,"wave   %s",0,SW_LAST-1,1,1);
			SYNTH_FIELD(SYP_SHAPE,"shape  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_SUB,"sub    %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_NOISE,"noise  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_FMAMT,"fm     %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_FMRATIO,"ratio  %s",0,SYNTH_FM_RATIO_COUNT-1,1,1);
			SYNTH_FIELD(SYP_CHORD,"chord  %s",0,SYNTH_CHORD_COUNT-1,1,1);
			SYNTH_FIELD(SYP_TUNE,"tune   %+d",-48,48,1,12);
			break;
		case 1:
			SYNTH_FIELD(SYP_ATTACK,"attack %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_DECAY,"decay  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_SUSTAIN,"sustn  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_RELEASE,"releas %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_PITCHENV,"p.env  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_PITCHDEC,"p.dec  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_GLIDE,"glide  %2.2X",0,0xFF,1,0x10);
			break;
		case 2:
			SYNTH_FIELD(SYP_FILTTYPE,"filter %s",0,SFT_LAST-1,1,1);
			SYNTH_FIELD(SYP_CUTOFF,"cutoff %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_RESO,"reso   %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_ENVAMT,"env    %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_ENVDEC,"envdec %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_DRIVE,"drive  %2.2X",0,0xFF,1,0x10);
			break;
		case 3:
			SYNTH_FIELD(SYP_LFODEST,"lfo    %s",0,SLD_LAST-1,1,1);
			SYNTH_FIELD(SYP_LFORATE,"rate   %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_LFOAMT,"depth  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_FINE,"fine   %+d",-50,50,1,10);
			SYNTH_FIELD(SYP_TABLEAUTO,"auto   %s",0,1,1,1);
			v=s->FindVariable(SYP_TABLE);
			f=new UIIntVarOffField(position,*v,"table  %2.2X",0x00,0x7F,1,0x10);
			T_SimpleList<UIField>::Insert(f);
			position._y+=1;
			break;
		case INSTRUMENT_MOD_PAGE:
			fillModPage(s,position);
			break;
		case INSTRUMENT_EQ_PAGE:
			fillEQPage(s,position);
			break;
		default:
			SYNTH_FIELD(SYP_VOLUME,"volume %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_PAN,"pan    %2.2X",0,0xFE,1,0x10);
			SYNTH_FIELD(SYP_REVERB,"reverb %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_DELAY,"delay  %2.2X",0,0xFF,1,0x10);
			SYNTH_FIELD(SYP_CHORUS,"chorus %2.2X",0,0xFF,1,0x10);
			break;
	}
#undef SYNTH_FIELD
}

// Two short lines describing the focused knob, plus its value in real units.
void InstrumentView::getSynthFieldHelp(FourCC id, I_Instrument *s, char *line1,
                                       char *line2, char *value) {
	int x=synthInt(s,id);
	char buf[32];
	line1[0]=line2[0]=value[0]=0;
	if (getInstrumentType()==IT_MACRO && getMacroFieldHelp(id,s,line1,line2,value)) {
		return;
	}
	switch(id) {
		case INSTRUMENT_TYPE_FIELD:
			strcpy(line1,"synth/macro: own sound");
			strcpy(line2,"sample: plays a WAV file");
			break;
		case SYP_PRESET:
			strcpy(line1,"A+Left/Right loads a");
			strcpy(line2,"ready-made sound");
			break;
		case SYP_ENGINE:
			strcpy(line1,"how the tone is made: synth,");
			strcpy(line2,"fm4, hyper chord or wav");
			switch(x) {
				case SE_FM4: strcpy(value,"4-op FM"); break;
				case SE_HYPER: strcpy(value,"6-note saw chord"); break;
				case SE_WAV: strcpy(value,"8-bit shapes"); break;
				default: strcpy(value,"subtractive + FM"); break;
			}
			break;
		case SYP_WAVE:
			strcpy(line1,"base tone: saw/pulse bright");
			strcpy(line2,"sine/triangle soft");
			break;
		case SYP_SHAPE:
			switch(synthInt(s,SYP_WAVE)) {
				case SW_SINE: strcpy(line1,"feedback: sine to buzzy"); break;
				case SW_TRIANGLE: strcpy(line1,"wavefold: adds overtones"); break;
				case SW_SAW: strcpy(line1,"adds octave brightness"); break;
				case SW_PULSE: strcpy(line1,"pulse width: square to thin"); break;
				case SW_SUPERSAW: strcpy(line1,"detune: wider and lusher"); break;
				case SW_NOISE: strcpy(line1,"00 hiss, higher = crunchy"); break;
				default: strcpy(line1,"spreads the metal tones"); break;
			}
			strcpy(line2,"changes the tone color");
			break;
		case SYP_SUB:
			strcpy(line1,"square one octave lower");
			strcpy(line2,"makes basses fatter");
			break;
		case SYP_NOISE:
			strcpy(line1,"mixes in hiss");
			strcpy(line2,"snares, breath, air");
			break;
		case SYP_FMAMT:
			strcpy(line1,"FM: bells, keys, metal");
			strcpy(line2,"higher = brighter/harsher");
			break;
		case SYP_FMRATIO:
			strcpy(line1,"FM partner pitch: 1,2 warm");
			strcpy(line2,"3.5, 7, 11 = bell-like");
			break;
		case SYP_CHORD:
			strcpy(line1,"notes played together");
			strcpy(line2,"CHRD in a phrase overrides");
			break;
		case SYP_TUNE:
			strcpy(line1,"shift pitch in semitones");
			strcpy(line2,"-12 = one octave down");
			sprintf(value,"%+d semitones",x);
			break;
		case SYP_FINE:
			strcpy(line1,"tiny tuning in cents");
			strcpy(line2,"100 cents = 1 semitone");
			sprintf(value,"%+d cents",x);
			break;
		case SYP_ATTACK:
			strcpy(line1,"fade-in time");
			strcpy(line2,"00 = instant hit");
			formatSeconds(buf,SynthInstrument::TimeFromParam(x));
			sprintf(value,"%s",buf);
			break;
		case SYP_DECAY:
			strcpy(line1,"time to fall to sustain");
			strcpy(line2,"short = punchy");
			formatSeconds(buf,SynthInstrument::TimeFromParam(x));
			sprintf(value,"%s",buf);
			break;
		case SYP_SUSTAIN:
			strcpy(line1,"level while the note holds");
			strcpy(line2,"00 = pluck or drum hit");
			sprintf(value,"%d%%",(x*100)/255);
			break;
		case SYP_RELEASE:
			strcpy(line1,"fade after the note stops");
			strcpy(line2,"KILL or next instrument");
			formatSeconds(buf,SynthInstrument::TimeFromParam(x));
			sprintf(value,"%s",buf);
			break;
		case SYP_PITCHENV:
			strcpy(line1,"pitch drop at note start");
			strcpy(line2,"kicks and toms use this");
			sprintf(value,"+%d semitones",(x*48)/255);
			break;
		case SYP_PITCHDEC:
			strcpy(line1,"how fast the pitch drops");
			strcpy(line2,"");
			formatSeconds(buf,SynthInstrument::TimeFromParam(x));
			sprintf(value,"%s",buf);
			break;
		case SYP_GLIDE:
			strcpy(line1,"slide between notes");
			strcpy(line2,"00 = off, notes retrigger");
			if (x==0) {
				strcpy(value,"off");
			} else {
				formatSeconds(buf,SynthInstrument::TimeFromParam(x));
				sprintf(value,"%s",buf);
			}
			break;
		case SYP_FILTTYPE:
			strcpy(line1,"lowpass = darker");
			strcpy(line2,"highpass = thinner");
			break;
		case SYP_CUTOFF:
			strcpy(line1,"where the filter cuts");
			strcpy(line2,"lower = darker/muffled");
			formatHz(buf,SynthInstrument::CutoffHzFromParam(x/255.0f));
			sprintf(value,"%s",buf);
			break;
		case SYP_RESO:
			strcpy(line1,"peak at the cutoff");
			strcpy(line2,"high = squelchy/acid");
			break;
		case SYP_ENVAMT:
			strcpy(line1,"brightness burst per note");
			strcpy(line2,"also brightens FM");
			break;
		case SYP_ENVDEC:
			strcpy(line1,"how fast that burst fades");
			strcpy(line2,"");
			formatSeconds(buf,SynthInstrument::TimeFromParam(x));
			sprintf(value,"%s",buf);
			break;
		case SYP_DRIVE:
			strcpy(line1,"saturation: warmer, grittier");
			strcpy(line2,"");
			break;
		case SYP_LFODEST:
			strcpy(line1,"what the LFO wobbles");
			strcpy(line2,"pitch = vibrato");
			break;
		case SYP_LFORATE:
			strcpy(line1,"wobble speed");
			strcpy(line2,"");
			sprintf(value,"%.2f Hz",SynthInstrument::LfoRateFromParam(x));
			break;
		case SYP_LFOAMT:
			strcpy(line1,"wobble depth");
			strcpy(line2,"00 = LFO off");
			break;
		case SYP_TABLEAUTO:
		case SYP_TABLE:
			strcpy(line1,"table: command steps that");
			strcpy(line2,"run on every note");
			break;
		case SYP_VOLUME:
			strcpy(line1,"instrument loudness");
			strcpy(line2,"VOLM changes it per note");
			sprintf(value,"%d%%",(x*100)/255);
			break;
		case SYP_PAN:
			strcpy(line1,"left/right position");
			strcpy(line2,"7F = center");
			break;
		case SYP_REVERB:
			strcpy(line1,"send to the shared reverb");
			strcpy(line2,"room size: FX screen");
			break;
		case SYP_DELAY:
			strcpy(line1,"send to the shared echo");
			strcpy(line2,"echo time: FX screen");
			break;
		case SYP_CHORUS:
			strcpy(line1,"send to the shared chorus");
			strcpy(line2,"speed/depth: FX screen");
			break;
		default:
			if (isEQField(id)) {
				getEQFieldHelp(id,s,line1,line2,value);
			} else if (getInstrumentType()!=IT_SYNTH || !getEngineFieldHelp(id,s,line1,line2,value)) {
				getModFieldHelp(id,s,line1,line2,value);
			}
			break;
	}
}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)

GUIColor synthPanel() { return AppWindow::ThemeColor(CD_BACKGROUND); }
GUIColor synthFrame() { return AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45); }
GUIColor synthTrace() { return AppWindow::ThemeColor(CD_HILITE2); }
GUIColor synthHot() { return AppWindow::ThemeColor(CD_NORMAL); }

void synthBox(SDLGUIWindowImp *imp, int x, int y, int w, int h) {
	GUIColor frame=synthFrame();
	GUIColor panel=synthPanel();
	imp->SetColor(frame);
	GUIRect outer(x,y,x+w,y+h);
	imp->DrawRect(outer);
	imp->SetColor(panel);
	GUIRect inner(x+1,y+1,x+w-1,y+h-1);
	imp->DrawRect(inner);
}

// Draws a continuous trace through points (px) inside a box
void synthPlot(SDLGUIWindowImp *imp, const int *ys, int count, int x, int top,
                      int bottom, bool hot) {
	GUIColor color=hot?synthHot():synthTrace();
	imp->SetColor(color);
	int prev=ys[0];
	for (int i=0;i<count;i++) {
		int y=ys[i];
		if (y<top) y=top;
		if (y>bottom) y=bottom;
		int a=y<prev?y:prev;
		int b=y>prev?y:prev;
		GUIRect r(x+i,a,x+i+1,b+1);
		imp->DrawRect(r);
		prev=y;
	}
}
#endif

void InstrumentView::drawSynthVisuals() {
	int i=viewData_->currentInstrument_;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
	I_Instrument *s=bank->GetInstrument(i);
	GUITextProperties props;
	char line[40];

	SetColor(CD_HILITE2);
	sprintf(line,"<LB %d/%d %s LB>",labPage_+1,INSTRUMENT_PAGE_COUNT,getSynthPageName());
	DrawString((30-(int)strlen(line))/2,2,line,props);
	SetColor(CD_NORMAL);

	if (labPage_==INSTRUMENT_MOD_PAGE) {
		// Its own layout: four slots, one curve, the slot's settings
		drawModPage(s);
		return;
	}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor clearColor=AppWindow::ThemeColor(CD_BACKGROUND);
	imp->SetColor(clearColor);
	GUIRect clearPanel(0,34,240,110);
	imp->DrawRect(clearPanel);

	const int bx=8, by=36, bw=224, bh=58;
	const int plotW=bw-4;
	const int top=by+3, bottom=by+bh-4;
	const int mid=(top+bottom)/2;
	int ys[240];
	synthBox(imp,bx,by,bw,bh);

	if (isMacroPage()) {
		drawMacroPicture(s,bx,by,bw,bh);
	} else if (labPage_==0 && ((SynthInstrument *)s)->GetEngine()!=SE_SYNTH) {
		// FM4 routing, HYPER swarm, WAV shape
		drawEngineSoundVisual((SynthInstrument *)s,bx,by,bw,bh);
	} else if (labPage_==0) {
		// One cycle of the oscillator, twice, so the shape reads clearly
		float cycle[110];
		((SynthInstrument *)s)->RenderCycle(cycle,110);
		for (int x=0;x<plotW;x++) {
			float v=cycle[(x*220/plotW)%110];
			ys[x]=mid-(int)(v*(bottom-top)/2*0.9f);
		}
		synthPlot(imp,ys,plotW,bx+2,top,bottom,false);
	} else if (labPage_==1) {
		// ADSR: widths grow with the time values, sustain is a fixed plateau
		float a=synthInt(s,SYP_ATTACK)/255.0f;
		float d=synthInt(s,SYP_DECAY)/255.0f;
		float r=synthInt(s,SYP_RELEASE)/255.0f;
		float sus=synthInt(s,SYP_SUSTAIN)/255.0f;
		int wa=4+(int)(a*52), wd=4+(int)(d*60), ws=sus>0.0f?40:0, wr=4+(int)(r*60);
		int total=wa+wd+ws+wr;
		float scale=total>plotW?(float)plotW/total:1.0f;
		wa=(int)(wa*scale); wd=(int)(wd*scale); ws=(int)(ws*scale); wr=(int)(wr*scale);
		int n=0;
		for (int x=0;x<wa && n<plotW;x++) ys[n++]=bottom-(int)((bottom-top)*(x+1)/(float)wa);
		for (int x=0;x<wd && n<plotW;x++) {
			float lvl=sus+(1.0f-sus)*(float)exp(-4.6f*x/(float)wd);
			ys[n++]=bottom-(int)((bottom-top)*lvl);
		}
		for (int x=0;x<ws && n<plotW;x++) ys[n++]=bottom-(int)((bottom-top)*sus);
		float start=sus>0.0f?sus:(float)exp(-4.6f);
		for (int x=0;x<wr && n<plotW;x++) {
			float lvl=start*(float)exp(-4.6f*x/(float)wr);
			ys[n++]=bottom-(int)((bottom-top)*lvl);
		}
		if (n>0) synthPlot(imp,ys,n,bx+2,top,bottom,false);
		// Pitch envelope as a second, bright trace
		int pe=synthInt(s,SYP_PITCHENV);
		if (pe>0) {
			float pd=synthInt(s,SYP_PITCHDEC)/255.0f;
			int wpd=6+(int)(pd*80);
			int m=0;
			for (int x=0;x<wpd && m<plotW;x++) {
				float lvl=(pe/255.0f)*(float)exp(-4.6f*x/(float)wpd);
				ys[m++]=bottom-(int)((bottom-top)*lvl);
			}
			synthPlot(imp,ys,m,bx+2,top,bottom,true);
		}
	} else if (labPage_==2) {
		// Magnitude response of the 2-pole filter, 20 Hz .. 20 kHz
		int type=synthInt(s,SYP_FILTTYPE);
		float cut=SynthInstrument::CutoffHzFromParam(synthInt(s,SYP_CUTOFF)/255.0f);
		float k=2.0f-1.94f*(synthInt(s,SYP_RESO)/255.0f);
		for (int x=0;x<plotW;x++) {
			float f=20.0f*(float)pow(1000.0,(double)x/(plotW-1));
			float w=f/cut;
			float den=(float)sqrt((1-w*w)*(1-w*w)+(k*w)*(k*w));
			float mag=1.0f;
			if (type==SFT_LOWPASS) mag=1.0f/den;
			else if (type==SFT_HIGHPASS) mag=(w*w)/den;
			else if (type==SFT_BANDPASS) mag=(k*w)/den;
			float db=20.0f*(float)log10(mag+1e-6f);
			if (db<-36.0f) db=-36.0f;
			if (db>18.0f) db=18.0f;
			ys[x]=top+(int)((18.0f-db)/54.0f*(bottom-top));
		}
		synthPlot(imp,ys,plotW,bx+2,top,bottom,false);
		if (type==SFT_OFF) {
			SetColor(CD_HILITE1);
			DrawString(8,6,"filter is off",props);
			SetColor(CD_NORMAL);
		}
		if (type!=SFT_OFF) {
			int cx=bx+2+(int)(log(cut/20.0f)/log(1000.0f)*(plotW-1));
			GUIColor hot=synthHot();
			imp->SetColor(hot);
			GUIRect marker(cx,top,cx+1,bottom);
			imp->DrawRect(marker);
		}
	} else if (labPage_==3) {
		// LFO shape: one second at the real rate (at least two cycles so slow
		// LFOs still read), depth on a square-root scale so small values show
		float rate=SynthInstrument::LfoRateFromParam(synthInt(s,SYP_LFORATE));
		float cycles=rate<2.0f?2.0f:(rate>12.0f?12.0f:rate);
		float depth=(float)sqrt(synthInt(s,SYP_LFOAMT)/255.0f);
		if (depth<=0.0f) {
			SetColor(CD_HILITE1);
			DrawString(7,6,"depth 00 = no LFO",props);
			SetColor(CD_NORMAL);
		}
		for (int x=0;x<plotW;x++) {
			float v=(float)sin(6.2831853f*cycles*x/plotW)*depth;
			ys[x]=mid-(int)(v*(bottom-top)/2);
		}
		synthPlot(imp,ys,plotW,bx+2,top,bottom,false);
	} else if (labPage_==INSTRUMENT_EQ_PAGE) {
		drawEQPlot(s,bx,by,bw,bh);
	} else {
		// Mix: level and pan, then the three effect sends
		const char *names[5]={"VOL","PAN","REV","DLY","CHO"};
		FourCC ids[5]={SYP_VOLUME,SYP_PAN,SYP_REVERB,SYP_DELAY,SYP_CHORUS};
		const int rows[5]={4,5,8,9,10};
		GUIColor clearColor2=AppWindow::ThemeColor(CD_BACKGROUND);
		imp->SetColor(clearColor2);
		GUIRect clearBox(bx,by,bx+bw,by+bh+4);
		imp->DrawRect(clearBox);
		for (int k=0;k<5;k++) {
			drawPixelLabBar(48,rows[k]*8+1,176,6,synthInt(s,ids[k]),ids[k]==SYP_PAN?254:255,ids[k]==SYP_PAN);
		}
		// Labels go on the character grid so the text layer keeps them
		SetColor(CD_NORMAL);
		for (int k=0;k<5;k++) {
			DrawString(1,rows[k],names[k],props);
		}
		SetColor(CD_MUTE);
		DrawString(1,7,"sends",props);
		SetColor(CD_NORMAL);
	}
#endif

	// Preset name and the focused value in real units. Explanations live
	// in the RB+Select helper so the page itself stays quiet.
	SetColor(CD_HILITE1);
	sprintf(line,"%s",s->GetName());
	DrawString(1,12,line,props);
	UIIntVarField *focused=(UIIntVarField *)GetFocus();
	if (focused) {
		char l1[40],l2[40],value[40];
		getSynthFieldHelp(focused->GetVariableID(),s,l1,l2,value);
		if (value[0]) {
			DrawString(29-(int)strlen(value),12,value,props);
		}
	}
	SetColor(CD_NORMAL);
}

void InstrumentView::auditionSynth(int offset) {
	int i=viewData_->currentInstrument_;
	int note=60+offset;
	Player::GetInstance()->AuditionInstrument(i,note);
	isDirty_=true;
}

void InstrumentView::customizeSynthOverlay(const char *&name, const char *&where,
                                           const char *&edit, const char *&field,
                                           const char *&cmd1, const char *&cmd2,
                                           const char *&cmd3, const char *&cmd4,
                                           const char *&cmd5, const char *&cmd6,
                                           const char *&cmd7) {
	where="RB+Left Phrase";
	edit="A+Dpad edit the knob";
	// The focused knob's explanation, shown only when help is asked for
	UIIntVarField *focused=(UIIntVarField *)GetFocus();
	if (focused) {
		static char help1[40],help2[40],value[40];
		int i=viewData_->currentInstrument_;
		I_Instrument *s=viewData_->project_->GetInstrumentBank()->GetInstrument(i);
		getSynthFieldHelp(focused->GetVariableID(),s,help1,help2,value);
		if (help1[0]) {
			where=help1;
			edit=help2;
		}
	}
	cmd1="Dpad choose a knob";
	cmd2="A+Left/Right small step";
	cmd3="A+Up/Down big step";
	cmd4="LB+Left/Right page";
	cmd5="Start play the phrase";
	cmd6="A+Start hear  B+A reset";
	cmd7="B+Left/Right other instr";
	switch(labPage_) {
		case 0: {
			name="SYNTH SOUND";
			field="Tone: preset/wave/FM";
			cmd1="preset: A+LR pick a sound";
			int i=viewData_->currentInstrument_;
			SynthInstrument *s=(SynthInstrument *)viewData_->project_->GetInstrumentBank()->GetInstrument(i);
			if (getInstrumentType()==IT_SYNTH && s->GetEngine()!=SE_SYNTH) {
				customizeEngineOverlay(s->GetEngine(),name,field,cmd1,cmd2,cmd3);
			}
			break;
		}
		case 1:
			name="SYNTH ENV";
			field="Volume shape over time";
			cmd1="attack/decay/sustn/release";
			break;
		case 2:
			name="SYNTH FILTER";
			field="Bright vs dark, squelch";
			cmd1="cutoff = brightness";
			break;
		case 3:
			name="SYNTH LFO";
			field="LFO wobble + table";
			cmd1="lfo target, rate, depth";
			break;
		case INSTRUMENT_MOD_PAGE:
			name="SYNTH MOD";
			customizeModOverlay(field,where,edit,cmd1,cmd2,cmd3,cmd4,cmd5,cmd6,cmd7);
			break;
		case INSTRUMENT_EQ_PAGE:
			name="SYNTH EQ";
			field="Its own low/mid/high EQ";
			cmd1="gain 80 = flat, +-12 dB";
			cmd6="A+Start hear  B+A flat";
			break;
		default:
			name="SYNTH MIX";
			field="Level, pan, reverb, echo";
			cmd1="reverb/delay are sends";
			break;
	}
	if (getInstrumentType()==IT_MACRO) {
		switch(labPage_) {
			case 0:
				name="MACRO SOUND";
				field="Shape, timbre, color";
				cmd1="shape: A+LR pick a model";
				break;
			case 1: name="MACRO ENV"; break;
			case 2: name="MACRO FILTER"; break;
			case 3:
				name="MACRO MOTION";
				field="LFO + timbre/color env";
				cmd1="lfo also moves timbre/color";
				break;
			case INSTRUMENT_MOD_PAGE: name="MACRO MOD"; break;
			case INSTRUMENT_EQ_PAGE: name="MACRO EQ"; break;
			default: name="MACRO MIX"; break;
		}
	}
}
