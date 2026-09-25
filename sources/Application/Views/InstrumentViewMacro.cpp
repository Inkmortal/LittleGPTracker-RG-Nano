// Macro synth pages of the Instrument screen. SOUND (shape, timbre, color,
// degrade, redux) draws the real model's output; MOTION holds the LFO and
// the timbre/color envelope. ENV, FILTER, MOD and MIX are the synth's own
// pages (the macro uses the synth's variable ids for those knobs).

#include "Application/AppWindow.h"
#include "InstrumentView.h"
#include "Application/Instruments/MacroInstrument.h"
#include "BaseClasses/UIIntVarField.h"
#include "BaseClasses/UIIntVarOffField.h"
#include "Foundation/Variables/Variable.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

static int macroInt(I_Instrument *instr, FourCC id) {
	Variable *v=instr?instr->FindVariable(id):0;
	return v?v->GetInt():0;
}

bool InstrumentView::isMacroPage() {
	return getInstrumentType()==IT_MACRO && (labPage_==0 || labPage_==3);
}

void InstrumentView::fillMacroPage(I_Instrument *instr, GUIPoint position) {
	UIIntVarField *f;
	Variable *v;

#define MACRO_FIELD(id,fmt,mn,mx,x,y) \
	v=instr->FindVariable(id); \
	f=new UIIntVarField(position,*v,fmt,mn,mx,x,y); \
	T_SimpleList<UIField>::Insert(f); \
	position._y+=1;

	if (labPage_==0) {
		// Labels are 7 wide here (degrade, redux as on the M8), so the type
		// row is this page's own rather than the shared 6-wide one
		int i=viewData_->currentInstrument_;
		if (i<MAX_SAMPLEINSTRUMENT_COUNT) {
			typeVar_->SetInt(2,false);
			f=new UIIntVarField(position,*typeVar_,"type    %s",0,2,1,1);
			T_SimpleList<UIField>::Insert(f);
			position._y+=1;
		}
		MACRO_FIELD(SYP_PRESET,"preset  %s",0,MacroInstrument::GetPresetCount()-1,1,1);
		MACRO_FIELD(MCP_SHAPE,"shape   %s",0,MACRO_SHAPE_COUNT-1,1,8);
		MACRO_FIELD(MCP_TIMBRE,"timbre  %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(MCP_COLOR,"color   %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(MCP_DEGRADE,"degrade %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(MCP_REDUX,"redux   %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(SYP_TUNE,"tune    %+d",-48,48,1,12);
	} else {
		MACRO_FIELD(MCP_LFODEST,"lfo    %s",0,MLD_LAST-1,1,1);
		MACRO_FIELD(SYP_LFORATE,"rate   %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(SYP_LFOAMT,"depth  %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(MCP_TENV,"t.env  %+d",-MACRO_ENV_MAX,MACRO_ENV_MAX,1,0x10);
		MACRO_FIELD(MCP_CENV,"c.env  %+d",-MACRO_ENV_MAX,MACRO_ENV_MAX,1,0x10);
		MACRO_FIELD(MCP_TCDECAY,"tc.dec %2.2X",0,0xFF,1,0x10);
		MACRO_FIELD(SYP_FINE,"fine   %+d",-50,50,1,10);
		MACRO_FIELD(SYP_TABLEAUTO,"auto   %s",0,1,1,1);
		v=instr->FindVariable(SYP_TABLE);
		f=new UIIntVarOffField(position,*v,"table  %2.2X",0x00,0x7F,1,0x10);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
	}
#undef MACRO_FIELD
}

static void macroSeconds(char *out, float seconds) {
	if (seconds<1.0f) {
		sprintf(out,"%d ms",(int)(seconds*1000.0f+0.5f));
	} else {
		sprintf(out,"%.1f s",seconds);
	}
}

// Explanations for the macro's own knobs; false = a shared (synth) knob
bool InstrumentView::getMacroFieldHelp(FourCC id, I_Instrument *instr, char *line1,
                                       char *line2, char *value) {
	int x=macroInt(instr,id);
	int shape=macroInt(instr,MCP_SHAPE);
	line1[0]=line2[0]=value[0]=0;
	switch(id) {
		case SYP_PRESET:
			strcpy(line1,"A+Left/Right loads a");
			strcpy(line2,"ready-made macro sound");
			return true;
		case MCP_SHAPE:
			sprintf(line1,"timbre: %s",MacroInstrument::GetTimbreHelp(shape));
			sprintf(line2,"color: %s",MacroInstrument::GetColorHelp(shape));
			sprintf(value,"%d/%d",shape+1,MACRO_SHAPE_COUNT);
			return true;
		case MCP_TIMBRE:
			sprintf(line1,"%s",MacroInstrument::GetTimbreHelp(shape));
			strcpy(line2,"TIMB command moves it");
			sprintf(value,"%d%%",(x*100+127)/255);
			return true;
		case MCP_COLOR:
			sprintf(line1,"%s",MacroInstrument::GetColorHelp(shape));
			strcpy(line2,"COLR command moves it");
			sprintf(value,"%d%%",(x*100+127)/255);
			return true;
		case MCP_DEGRADE:
			strcpy(line1,"lower sample rate: alias");
			strcpy(line2,"00 off .. FF = 4 kHz");
			if (x==0) {
				strcpy(value,"off");
			} else {
				sprintf(value,"%.1f kHz",96.0/pow(24.0,x/255.0));
			}
			return true;
		case MCP_REDUX:
			strcpy(line1,"fewer bits: grit, crunch");
			strcpy(line2,"00 off .. FF = 2 bits");
			if (x==0) {
				strcpy(value,"off");
			} else {
				sprintf(value,"%d bit",16-(x*14+127)/255);
			}
			return true;
		case MCP_LFODEST:
			strcpy(line1,"what the LFO wobbles");
			strcpy(line2,"timbre/color: sweeps");
			return true;
		case MCP_TENV:
			strcpy(line1,"timbre jump at each note");
			strcpy(line2,"fades back over tc.dec");
			sprintf(value,"%+d%%",(x*100)/MACRO_ENV_MAX);
			return true;
		case MCP_CENV:
			strcpy(line1,"color jump at each note");
			strcpy(line2,"fades back over tc.dec");
			sprintf(value,"%+d%%",(x*100)/MACRO_ENV_MAX);
			return true;
		case MCP_TCDECAY:
			strcpy(line1,"how fast t.env and c.env");
			strcpy(line2,"fall back to the knobs");
			macroSeconds(value,SynthInstrument::TimeFromParam(x));
			return true;
		default:
			return false;
	}
}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
static void macroBox(SDLGUIWindowImp *imp, int x, int y, int w, int h) {
	GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	imp->SetColor(frame);
	GUIRect outer(x,y,x+w,y+h);
	imp->DrawRect(outer);
	imp->SetColor(panel);
	GUIRect inner(x+1,y+1,x+w-1,y+h-1);
	imp->DrawRect(inner);
}

// A curve through one y per column, joined column to column
static void macroTrace(SDLGUIWindowImp *imp, const int *ys, int count, int x,
                       int top, int bottom) {
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

// The picture is the model itself, rendered offline; kept until a knob that
// changes it moves
struct MacroPictureCache {
	int key[6];
	int columns;
	MacroPreviewKind kind;
	float minv[240];
	float maxv[240];
};

void InstrumentView::drawMacroPicture(I_Instrument *instr, int bx, int by, int bw, int bh) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUITextProperties props;
	MacroInstrument *m=(MacroInstrument *)instr;
	const int top=by+3, bottom=by+bh-4;
	const int mid=(top+bottom)/2;
	const int plotW=bw-4;
	macroBox(imp,bx,by,bw,bh);

	if (labPage_==0) {
		static MacroPictureCache cache;
		static bool cached=false;
		int key[6]={macroInt(m,MCP_SHAPE),macroInt(m,MCP_TIMBRE),macroInt(m,MCP_COLOR),
		            macroInt(m,MCP_DEGRADE),macroInt(m,MCP_REDUX),macroInt(m,SYP_TUNE)};
		if (!cached || cache.columns!=plotW || memcmp(key,cache.key,sizeof(key))) {
			cache.kind=m->RenderPreview(cache.minv,cache.maxv,plotW);
			memcpy(cache.key,key,sizeof(key));
			cache.columns=plotW;
			cached=true;
		}
		// Centre line, then min..max per column so noise and hits read too
		GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
		imp->SetColor(frame);
		for (int x=0;x<plotW;x+=6) {
			GUIRect dot(bx+2+x,mid,bx+4+x,mid+1);
			imp->DrawRect(dot);
		}
		GUIColor trace=AppWindow::ThemeColor(CD_HILITE2);
		imp->SetColor(trace);
		const float half=(bottom-top)/2*0.92f;
		for (int x=0;x<plotW;x++) {
			int y0=mid-(int)(cache.maxv[x]*half);
			int y1=mid-(int)(cache.minv[x]*half);
			if (y0<top) y0=top;
			if (y1>bottom) y1=bottom;
			if (y1<y0) y1=y0;
			GUIRect r(bx+2+x,y0,bx+3+x,y1+1);
			imp->DrawRect(r);
		}
		// What the picture spans, in the box corner
		const char *what=cache.kind==MPK_HIT?"note start":
		                 (cache.kind==MPK_TEXTURE?"20 ms":"2 cycles");
		SetColor(CD_MUTE);
		DrawString(29-(int)strlen(what),5,what,props);
		SetColor(CD_HILITE1);
		DrawString(2,5,MacroInstrument::GetShapeName(key[0]),props);
		SetColor(CD_NORMAL);
		return;
	}

	// MOTION: the LFO (as the synth draws it) and the timbre/color envelope
	// over the same second
	int ys[240];
	float rate=MacroInstrument::LfoRateFromParam(macroInt(m,SYP_LFORATE));
	float cycles=rate<2.0f?2.0f:(rate>12.0f?12.0f:rate);
	float depth=(float)sqrt(macroInt(m,SYP_LFOAMT)/255.0f);
	bool any=false;
	if (depth>0.0f) {
		for (int x=0;x<plotW;x++) {
			float v=(float)sin(6.2831853f*cycles*x/plotW)*depth;
			ys[x]=mid-(int)(v*(bottom-top)/2);
		}
		GUIColor trace=AppWindow::ThemeColor(CD_HILITE2);
		imp->SetColor(trace);
		macroTrace(imp,ys,plotW,bx+2,top,bottom);
		any=true;
	}
	float decay=SynthInstrument::TimeFromParam(macroInt(m,MCP_TCDECAY));
	FourCC envs[2]={MCP_TENV,MCP_CENV};
	ColorDefinition colors[2]={CD_HILITE1,CD_PLAY};
	for (int e=0;e<2;e++) {
		int amount=macroInt(m,envs[e]);
		if (amount==0) continue;
		float a=amount/(float)MACRO_ENV_MAX;
		for (int x=0;x<plotW;x++) {
			float t=1.0f*x/plotW;
			float lvl=a*(float)exp(-4.6f*t/decay);
			ys[x]=mid-(int)(lvl*(bottom-top)/2);
		}
		GUIColor c=AppWindow::ThemeColor(colors[e]);
		imp->SetColor(c);
		macroTrace(imp,ys,plotW,bx+2,top,bottom);
		any=true;
	}
	if (!any) {
		SetColor(CD_HILITE1);
		DrawString(5,6,"depth 00, envs off",props);
		SetColor(CD_NORMAL);
	} else {
		// Legend: which trace is which
		SetColor(CD_HILITE2);
		DrawString(2,5,"lfo",props);
		SetColor(CD_HILITE1);
		DrawString(6,5,"t.env",props);
		SetColor(CD_PLAY);
		DrawString(12,5,"c.env",props);
		SetColor(CD_NORMAL);
	}
#endif
}
