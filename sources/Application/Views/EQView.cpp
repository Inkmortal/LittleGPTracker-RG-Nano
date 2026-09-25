#include "EQView.h"
#include "Application/AppWindow.h"
#include "Application/Mixer/MasterEQ.h"
#include "Application/Model/Project.h"
#include "Application/Player/Player.h"
#include "BaseClasses/UIIntVarField.h"
#include "Services/Audio/Audio.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

// Rows: three bands (heading + two knobs), then the curve
#define BAND_ROW 4
#define BAND_STEP 3
#define PLOT_TOP_ROW 13
#define PLOT_ROWS 8
#define HELP_ROW 23
#define PLOT_X 8
#define PLOT_W 224
#define KNOB2_X 15
#define PLOT_DB 12.0f

static const FourCC knobIds[6] = {VAR_EQ_LOW_GAIN, VAR_EQ_LOW_FREQ, VAR_EQ_MID_GAIN,
                                  VAR_EQ_MID_FREQ, VAR_EQ_HIGH_GAIN, VAR_EQ_HIGH_FREQ};

EQView::EQView(GUIWindow &w,ViewData *data):FieldView(w,data) {
	viewType_=VT_EQ ;
	Project *project=data->project_ ;
	for (int i=0;i<6;i++) {
		Variable *v=project->FindVariable(knobIds[i]) ;
		NAssert(v) ;
		GUIPoint position((i%2)?KNOB2_X:1,BAND_ROW+1+(i/2)*BAND_STEP) ;
		UIIntVarField *field=new UIIntVarField(position,*v,(i%2)?"freq  %2.2X":"gain  %2.2X",
		                                       0,0xFF,1,0x10) ;
		T_SimpleList<UIField>::Insert(field) ;
	}
}

EQView::~EQView() {
}

void EQView::params(int out[6]) {
	for (int i=0;i<6;i++) {
		Variable *v=viewData_->project_->FindVariable(knobIds[i]) ;
		out[i]=v?v->GetInt():0x80 ;
	}
}

void EQView::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;
	if (mask==EPBM_UP || mask==EPBM_DOWN) {
		// Up/Down walk the six knobs in reading order, as on FX
		UIField *fields[6] ;
		int count=0 ;
		IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
		for (it->Begin();!it->IsDone() && count<6;it->Next()) {
			fields[count++]=&it->CurrentItem() ;
		}
		if (count==0) return ;
		int index=GetFocusIndex() ;
		index=(index+(mask==EPBM_DOWN?1:count-1))%count ;
		SetFocus(fields[index]) ;
		isDirty_=true ;
		return ;
	}
	if (mask==(EPBM_B|EPBM_A)) {
		// B+A: this knob back to its default (gain: flat)
		UIIntVarField *focus=(UIIntVarField *)GetFocus() ;
		if (focus) focus->GetVariable().ResetToDefault() ;
		isDirty_=true ;
		return ;
	}
	FieldView::ProcessButtonMask(mask) ;
	if (mask&EPBM_R) {
		if (mask&EPBM_LEFT) {
			ViewType vt=VT_FX ;
			ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
			SetChanged() ;
			NotifyObservers(&ve) ;
		}
	} else if (mask==EPBM_START) {
		Player::GetInstance()->OnStartButton(PM_SONG,viewData_->songX_,false,viewData_->songX_) ;
	}
	isDirty_=true ;
}

static void formatFreq(char *out,float hz) {
	if (hz>=1000.0f) sprintf(out,"%.1fk",hz/1000.0f) ;
	else sprintf(out,"%d",(int)(hz+0.5f)) ;
}

void EQView::DrawView() {
	Clear() ;
	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;
	SetColor(CD_NORMAL) ;
	DrawString(pos._x,pos._y,"EQ",props) ;
	SetColor(CD_MUTE) ;
	DrawString(pos._x+3,pos._y,"the whole mix",props) ;

	int p[6] ;
	params(p) ;
	static const char *names[3]={"LOW","MID","HIGH"} ;
	float freqs[3]={MasterEQ::LowFreqFromParam(p[1]),MasterEQ::MidFreqFromParam(p[3]),
	                MasterEQ::HighFreqFromParam(p[5])} ;
	char line[32],freq[12] ;
	for (int b=0;b<3;b++) {
		int row=BAND_ROW+b*BAND_STEP ;
		SetColor(CD_HILITE1) ;
		DrawString(1,row,names[b],props) ;
		formatFreq(freq,freqs[b]) ;
		sprintf(line,"%+5.1fdB  %sHz",MasterEQ::GainDbFromParam(p[b*2]),freq) ;
		SetColor(p[b*2]==0x80?CD_MUTE:CD_NORMAL) ;
		DrawString(7,row,line,props) ;
	}
	SetColor(CD_NORMAL) ;

	FieldView::Redraw() ;

	static const char *help[6][2]={
		{"boost or cut the bass","80 = flat"},
		{"where the bass shelf starts","30-400 Hz"},
		{"boost or cut the middle","80 = flat"},
		{"centre of the middle band","150 Hz - 6 kHz"},
		{"boost or cut the treble","80 = flat"},
		{"where the treble shelf starts","1.5-16 kHz"},
	} ;
	int focus=GetFocusIndex() ;
	DrawString(1,HELP_ROW,"                            ",props) ;
	DrawString(1,HELP_ROW+1,"                            ",props) ;
	if (focus>=0 && focus<6) {
		DrawString(1,HELP_ROW,help[focus][0],props) ;
		SetColor(CD_MUTE) ;
		DrawString(1,HELP_ROW+1,help[focus][1],props) ;
	}
	SetColor(CD_MUTE) ;
	DrawString(1,HELP_ROW+3,"B+A flat  RB+Left FX",props) ;
	SetColor(CD_NORMAL) ;
}

// The combined curve: +-12 dB, 20 Hz to 20 kHz on a log scale, with the
// 0 dB line and marks at 100 Hz, 1 kHz and 10 kHz
void EQView::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	const int top=PLOT_TOP_ROW*8 ;
	const int h=PLOT_ROWS*8 ;
	const int mid=top+h/2 ;
	GUIColor background=AppWindow::ThemeColor(CD_BACKGROUND) ;
	GUIColor grid=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,35) ;
	GUIColor curve=AppWindow::ThemeColor(CD_HILITE2) ;
	GUIColor fill=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE2,30) ;
	imp->SetColor(background) ;
	GUIRect area(PLOT_X,top,PLOT_X+PLOT_W,top+h) ;
	imp->DrawRect(area) ;
	imp->SetColor(grid) ;
	GUIRect zero(PLOT_X,mid,PLOT_X+PLOT_W,mid+1) ;
	imp->DrawRect(zero) ;
	const float lo=(float)log10(20.0),hi=(float)log10(20000.0) ;
	const float marks[3]={100.0f,1000.0f,10000.0f} ;
	for (int m=0;m<3;m++) {
		int x=PLOT_X+(int)((log10(marks[m])-lo)/(hi-lo)*PLOT_W) ;
		GUIRect tick(x,top,x+1,top+h) ;
		imp->DrawRect(tick) ;
	}
	int p[6] ;
	params(p) ;
	float rate=(float)Audio::GetInstance()->GetSampleRate() ;
	float coeffs[3][5] ;
	MasterEQ::Design(p,rate,coeffs) ;
	int previous=-1 ;
	for (int x=0;x<PLOT_W;x++) {
		float f=(float)pow(10.0,lo+(hi-lo)*x/PLOT_W) ;
		float db=MasterEQ::ResponseDb(coeffs,f,rate) ;
		if (db>PLOT_DB) db=PLOT_DB ;
		if (db<-PLOT_DB) db=-PLOT_DB ;
		int y=mid-(int)(db/PLOT_DB*(h/2-1)) ;
		// Shade between the curve and 0 dB so boosts and cuts read at a glance
		imp->SetColor(fill) ;
		GUIRect body(PLOT_X+x,y<mid?y:mid,PLOT_X+x+1,y<mid?mid:y+1) ;
		imp->DrawRect(body) ;
		imp->SetColor(curve) ;
		int a=previous<0?y:(previous<y?previous:y) ;
		int b=previous<0?y:(previous>y?previous:y) ;
		GUIRect dot(PLOT_X+x,a,PLOT_X+x+1,b+1) ;
		imp->DrawRect(dot) ;
		previous=y ;
	}
#endif
}
