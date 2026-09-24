// MOD page of the Instrument screen, shared by synth and sample instruments:
// two slots, each an envelope or an LFO aimed at one sound parameter.

#include "Application/AppWindow.h"
#include "InstrumentView.h"
#include "Application/Instruments/ModSources.h"
#include "BaseClasses/UIIntVarField.h"
#include "Foundation/Variables/Variable.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

static const FourCC modIds[MOD_SLOT_COUNT][4]={
	{MOD1_TYPE,MOD1_DEST,MOD1_AMOUNT,MOD1_TIME},
	{MOD2_TYPE,MOD2_DEST,MOD2_AMOUNT,MOD2_TIME},
};

static int modInt(I_Instrument *instr, FourCC id) {
	Variable *v=instr?instr->FindVariable(id):0;
	return v?v->GetInt():0;
}

// Which slot and which of its four settings an id belongs to (-1 if none)
static int modSlotOf(FourCC id, int &which) {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		for (int k=0;k<4;k++) {
			if (modIds[s][k]==id) {
				which=k;
				return s;
			}
		}
	}
	return -1;
}

bool InstrumentView::isModField(FourCC id) {
	int which;
	return modSlotOf(id,which)>=0;
}

void InstrumentView::fillModPage(I_Instrument *instr, GUIPoint position) {
	static const char *labels[MOD_SLOT_COUNT][4]={
		{"mod1   %s","dest1  %s","amt1   %+d","rate1  %2.2X"},
		{"mod2   %s","dest2  %s","amt2   %+d","rate2  %2.2X"},
	};
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		Variable *v=instr->FindVariable(modIds[s][0]);
		UIIntVarField *f=new UIIntVarField(position,*v,labels[s][0],0,MT_LAST-1,1,1);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
		v=instr->FindVariable(modIds[s][1]);
		f=new UIIntVarField(position,*v,labels[s][1],0,MD_LAST-1,1,1);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
		v=instr->FindVariable(modIds[s][2]);
		f=new UIIntVarField(position,*v,labels[s][2],-MOD_AMOUNT_MAX,MOD_AMOUNT_MAX,1,0x10);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
		v=instr->FindVariable(modIds[s][3]);
		f=new UIIntVarField(position,*v,labels[s][3],0,0xFF,1,0x10);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
	}
}

static void formatModTime(char *out, float seconds) {
	if (seconds<1.0f) {
		sprintf(out,"%d ms",(int)(seconds*1000.0f+0.5f));
	} else {
		sprintf(out,"%.1f s",seconds);
	}
}

void InstrumentView::getModFieldHelp(FourCC id, I_Instrument *instr, char *line1,
                                     char *line2, char *value) {
	line1[0]=line2[0]=value[0]=0;
	int which=0;
	int slot=modSlotOf(id,which);
	if (slot<0) {
		return;
	}
	int type=modInt(instr,modIds[slot][0]);
	int x=modInt(instr,id);
	switch(which) {
		case 0:
			strcpy(line1,"decay/swell: envelope");
			strcpy(line2,"sine..random: LFO wobble");
			break;
		case 1:
			strcpy(line1,"what this slot moves");
			strcpy(line2,"cutoff = filter sweep");
			break;
		case 2:
			strcpy(line1,"how far it moves");
			strcpy(line2,"minus = the other way");
			if (modInt(instr,modIds[slot][1])==MD_PITCH) {
				sprintf(value,"%+.1f semitones",x*MOD_PITCH_RANGE/MOD_AMOUNT_MAX);
			} else {
				sprintf(value,"%+d%%",(x*100)/MOD_AMOUNT_MAX);
			}
			break;
		default:
			strcpy(line1,"higher = faster");
			if (ModSource::IsLfo(type)) {
				strcpy(line2,"wobbles per second");
				sprintf(value,"%.2f Hz",ModSource::RateFromParam(x));
			} else {
				strcpy(line2,"envelope length");
				formatModTime(value,ModSource::EnvTimeFromRate(x));
			}
			break;
	}
}

// Both slots over two seconds, each at full height so its shape reads even
// at small amounts (flipped when the amount is negative; the fields show
// how far). Slot 1 in the highlight colour, slot 2 in the play colour.
void InstrumentView::drawModPlot(I_Instrument *instr, int bx, int by, int bw, int bh) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	imp->SetColor(frame);
	GUIRect outer(bx,by,bx+bw,by+bh);
	imp->DrawRect(outer);
	imp->SetColor(panel);
	GUIRect inner(bx+1,by+1,bx+bw-1,by+bh-1);
	imp->DrawRect(inner);

	const int top=by+3, bottom=by+bh-4;
	const int mid=(top+bottom)/2;
	const int plotW=bw-4;
	const float window=2.0f;

	// Centre line, dotted
	imp->SetColor(frame);
	for (int x=0;x<plotW;x+=6) {
		GUIRect dot(bx+2+x,mid,bx+4+x,mid+1);
		imp->DrawRect(dot);
	}

	for (int s=MOD_SLOT_COUNT-1;s>=0;s--) {
		int type=modInt(instr,modIds[s][0]);
		int amount=modInt(instr,modIds[s][2]);
		if (type==MT_OFF || amount==0) {
			continue;
		}
		int rate=modInt(instr,modIds[s][3]);
		float a=(amount<0)?-0.9f:0.9f;
		GUIColor color=AppWindow::ThemeColor(s==0?CD_HILITE2:CD_PLAY);
		imp->SetColor(color);
		unsigned int seed=12345u+s;
		float held=0.0f;
		int lastCycle=-1;
		int prev=mid;
		for (int x=0;x<plotW;x++) {
			float t=window*x/(plotW-1);
			float level;
			if (ModSource::IsLfo(type)) {
				float cycles=t*ModSource::RateFromParam(rate);
				int cycle=(int)cycles;
				if (cycle!=lastCycle) {
					seed=seed*1664525u+1013904223u;
					held=((seed>>8)&0xFFFF)/32767.5f-1.0f;
					lastCycle=cycle;
				}
				level=ModSource::LfoShape(type,cycles-cycle,held);
			} else {
				float len=ModSource::EnvTimeFromRate(rate);
				if (type==MT_DECAY) {
					level=(float)exp(-4.6f*t/len);
				} else {
					level=t>=len?1.0f:t/len;
				}
			}
			int y=mid-(int)(level*a*(bottom-top)/2);
			if (y<top) y=top;
			if (y>bottom) y=bottom;
			int y0=y<prev?y:prev;
			int y1=y>prev?y:prev;
			if (x==0) {
				y0=y1=y;
			}
			GUIRect r(bx+2+x,y0,bx+3+x,y1+1);
			imp->DrawRect(r);
			prev=y;
		}
	}
#endif
}
