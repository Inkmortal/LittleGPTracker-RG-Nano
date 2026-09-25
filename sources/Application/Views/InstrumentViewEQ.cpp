// EQ page of the Instrument screen, shared by synth and sample instruments:
// the instrument's own low shelf / mid bell / high shelf (like the M8's
// instrument EQ), drawn as the resulting curve.

#include "Application/AppWindow.h"
#include "InstrumentView.h"
#include "Application/Instruments/InstrumentEQ.h"
#include "BaseClasses/UIIntVarField.h"
#include "Foundation/Variables/Variable.h"
#include "Services/Audio/Audio.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

#define EQ_PLOT_DB 12.0f

static int eqIndexOf(FourCC id) {
	for (int i=0;i<6;i++) {
		if (InstrumentEQ::Ids[i]==id) return i;
	}
	return -1;
}

static void eqParams(I_Instrument *instr, int out[6]) {
	for (int i=0;i<6;i++) {
		Variable *v=instr?instr->FindVariable(InstrumentEQ::Ids[i]):0;
		out[i]=v?v->GetInt():ThreeBandEQ::DefaultParams[i];
	}
}

static void formatEqHz(char *out, float hz) {
	if (hz>=1000.0f) sprintf(out,"%.1f kHz",hz/1000.0f);
	else sprintf(out,"%d Hz",(int)(hz+0.5f));
}

bool InstrumentView::isEQField(FourCC id) {
	return eqIndexOf(id)>=0;
}

void InstrumentView::fillEQPage(I_Instrument *instr, GUIPoint position) {
	static const char *labels[6]={"l.gain %2.2X","l.freq %2.2X","m.gain %2.2X",
	                              "m.freq %2.2X","h.gain %2.2X","h.freq %2.2X"};
	for (int i=0;i<6;i++) {
		Variable *v=instr->FindVariable(InstrumentEQ::Ids[i]);
		if (!v) continue;
		UIIntVarField *f=new UIIntVarField(position,*v,labels[i],0,0xFF,1,0x10);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
	}
}

void InstrumentView::getEQFieldHelp(FourCC id, I_Instrument *instr, char *line1,
                                    char *line2, char *value) {
	line1[0]=line2[0]=value[0]=0;
	int index=eqIndexOf(id);
	if (index<0) return;
	int p[6];
	eqParams(instr,p);
	int x=p[index];
	switch(index) {
		case 0:
			strcpy(line1,"boost or cut the bass");
			strcpy(line2,"80 = flat, +-12 dB");
			sprintf(value,"%+.1f dB",ThreeBandEQ::GainDbFromParam(x));
			break;
		case 1:
			strcpy(line1,"where the bass shelf starts");
			strcpy(line2,"30-400 Hz");
			formatEqHz(value,ThreeBandEQ::LowFreqFromParam(x));
			break;
		case 2:
			strcpy(line1,"boost or cut the middle");
			strcpy(line2,"80 = flat, +-12 dB");
			sprintf(value,"%+.1f dB",ThreeBandEQ::GainDbFromParam(x));
			break;
		case 3:
			strcpy(line1,"centre of the middle band");
			strcpy(line2,"150 Hz - 6 kHz");
			formatEqHz(value,ThreeBandEQ::MidFreqFromParam(x));
			break;
		case 4:
			strcpy(line1,"boost or cut the treble");
			strcpy(line2,"80 = flat, +-12 dB");
			sprintf(value,"%+.1f dB",ThreeBandEQ::GainDbFromParam(x));
			break;
		default:
			strcpy(line1,"where the treble shelf starts");
			strcpy(line2,"1.5-16 kHz");
			formatEqHz(value,ThreeBandEQ::HighFreqFromParam(x));
			break;
	}
}

// The combined curve, +-12 dB, 20 Hz .. 20 kHz on a log scale, shaded to
// the 0 dB line, with marks at 100 Hz, 1 kHz and 10 kHz. The focused
// band's frequency is marked in the bright colour.
void InstrumentView::drawEQPlot(I_Instrument *instr, int bx, int by, int bw, int bh) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	GUIColor grid=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,35);
	GUIColor curve=AppWindow::ThemeColor(CD_HILITE2);
	GUIColor fill=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE2,30);
	GUIColor hot=AppWindow::ThemeColor(CD_NORMAL);
	imp->SetColor(frame);
	GUIRect outer(bx,by,bx+bw,by+bh);
	imp->DrawRect(outer);
	imp->SetColor(panel);
	GUIRect inner(bx+1,by+1,bx+bw-1,by+bh-1);
	imp->DrawRect(inner);

	const int x0=bx+2, w=bw-4;
	const int top=by+3, bottom=by+bh-4;
	const int mid=(top+bottom)/2;
	const int half=(bottom-top)/2;
	imp->SetColor(grid);
	GUIRect zero(x0,mid,x0+w,mid+1);
	imp->DrawRect(zero);
	const float lo=(float)log10(20.0),hi=(float)log10(20000.0);
	const float marks[3]={100.0f,1000.0f,10000.0f};
	for (int m=0;m<3;m++) {
		int x=x0+(int)((log10(marks[m])-lo)/(hi-lo)*w);
		GUIRect tick(x,top,x+1,bottom+1);
		imp->DrawRect(tick);
	}

	int p[6];
	eqParams(instr,p);
	float rate=(float)Audio::GetInstance()->GetSampleRate();
	if (rate<8000.0f) rate=44100.0f;
	float coeffs[3][5];
	ThreeBandEQ::Design(p,rate,coeffs);
	int previous=-1;
	for (int x=0;x<w;x++) {
		float f=(float)pow(10.0,lo+(hi-lo)*x/w);
		float db=ThreeBandEQ::ResponseDb(coeffs,f,rate);
		if (db>EQ_PLOT_DB) db=EQ_PLOT_DB;
		if (db<-EQ_PLOT_DB) db=-EQ_PLOT_DB;
		int y=mid-(int)(db/EQ_PLOT_DB*half);
		imp->SetColor(fill);
		GUIRect body(x0+x,y<mid?y:mid,x0+x+1,y<mid?mid:y+1);
		imp->DrawRect(body);
		imp->SetColor(curve);
		int a=previous<0?y:(previous<y?previous:y);
		int b=previous<0?y:(previous>y?previous:y);
		GUIRect dot(x0+x,a,x0+x+1,b+1);
		imp->DrawRect(dot);
		previous=y;
	}

	// The focused band: a short bright mark at its frequency
	UIIntVarField *focused=(UIIntVarField *)GetFocus();
	int index=focused?eqIndexOf(focused->GetVariableID()):-1;
	if (index>=0) {
		int band=index/2;
		float f=band==0?ThreeBandEQ::LowFreqFromParam(p[1]):
		        (band==1?ThreeBandEQ::MidFreqFromParam(p[3]):ThreeBandEQ::HighFreqFromParam(p[5]));
		int x=x0+(int)((log10(f)-lo)/(hi-lo)*w);
		if (x<x0) x=x0;
		if (x>x0+w-1) x=x0+w-1;
		imp->SetColor(hot);
		GUIRect marker(x,top,x+1,top+4);
		imp->DrawRect(marker);
		GUIRect marker2(x,bottom-3,x+1,bottom+1);
		imp->DrawRect(marker2);
	}
	if (ThreeBandEQ::IsFlat(p)) {
		GUITextProperties props;
		SetColor(CD_MUTE);
		DrawString(bx/8+2,(by+bh/2)/8-1,"flat: gains at 80",props);
		SetColor(CD_NORMAL);
	}
#endif
}
