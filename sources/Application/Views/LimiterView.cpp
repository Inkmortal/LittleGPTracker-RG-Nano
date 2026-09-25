#include "LimiterView.h"
#include "Application/AppWindow.h"
#include "Application/Mixer/MasterLimiter.h"
#include "Application/Model/Project.h"
#include "Application/Player/Player.h"
#include "BaseClasses/UIIntVarField.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

// Rows: state, two rows of two knobs (each with its value in real units),
// the gain-reduction meter, the scope, help
#define STATE_ROW 3
#define KNOB_ROW1 5
#define KNOB_ROW2 8
#define METER_ROW 11
#define LEVELS_ROW 12
#define SCOPE_TOP_ROW 14
#define SCOPE_ROWS 7
#define HELP_ROW 22
#define KNOB2_X 15
#define SCOPE_X 8
#define SCOPE_FLOOR_DB -48.0f   // bottom of the level scope
#define GR_FULL_DB 12.0f        // meter / scope full scale for limiting

static const FourCC knobIds[4]={VAR_LIM_DRIVE,VAR_LIM_CEILING,VAR_LIM_ATTACK,VAR_LIM_RELEASE} ;

LimiterView::LimiterView(GUIWindow &w,ViewData *data):FieldView(w,data) {
	viewType_=VT_LIMIT ;
	Project *project=data->project_ ;
	static const char *formats[4]={"drive %2.2X","ceil  %2.2X","attk  %2.2X","rels  %2.2X"} ;
	for (int i=0;i<4;i++) {
		Variable *v=project->FindVariable(knobIds[i]) ;
		NAssert(v) ;
		GUIPoint position((i%2)?KNOB2_X:1,(i<2)?KNOB_ROW1:KNOB_ROW2) ;
		UIIntVarField *field=new UIIntVarField(position,*v,formats[i],0,0xFF,1,0x10) ;
		T_SimpleList<UIField>::Insert(field) ;
	}
}

LimiterView::~LimiterView() {
}

int LimiterView::param(int index) {
	Variable *v=viewData_->project_->FindVariable(knobIds[index]) ;
	return v?v->GetInt():0 ;
}

void LimiterView::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;
	if (mask==EPBM_UP || mask==EPBM_DOWN) {
		// Up/Down walk the four knobs in reading order, as on FX and EQ
		UIField *fields[4] ;
		int count=0 ;
		IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
		for (it->Begin();!it->IsDone() && count<4;it->Next()) {
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
		// B+A: this knob back to its default (drive: off)
		UIIntVarField *focus=(UIIntVarField *)GetFocus() ;
		if (focus) focus->GetVariable().ResetToDefault() ;
		isDirty_=true ;
		return ;
	}
	FieldView::ProcessButtonMask(mask) ;
	if (mask&EPBM_R) {
		if (mask&EPBM_LEFT) {
			ViewType vt=VT_EQ ;
			ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
			SetChanged() ;
			NotifyObservers(&ve) ;
		}
	} else if (mask==EPBM_START) {
		Player::GetInstance()->OnStartButton(PM_SONG,viewData_->songX_,false,viewData_->songX_) ;
	}
	isDirty_=true ;
}

static void formatDb(char *out,float db) {
	sprintf(out,"%+.1f dB",db) ;
}

void LimiterView::DrawView() {
	Clear() ;
	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;
	SetColor(CD_NORMAL) ;
	DrawString(pos._x,pos._y,"LIMIT",props) ;
	SetColor(CD_MUTE) ;
	DrawString(pos._x+6,pos._y,"the whole mix",props) ;

	// Each knob's value in real units, under it
	char line[32] ;
	int drive=param(0) ;
	SetColor(drive==0?CD_MUTE:CD_NORMAL) ;
	if (drive==0) {
		strcpy(line,"off") ;
	} else {
		formatDb(line,MasterLimiter::DriveDbFromParam(drive)) ;
	}
	DrawString(7,KNOB_ROW1+1,line,props) ;
	SetColor(CD_NORMAL) ;
	formatDb(line,MasterLimiter::CeilingDbFromParam(param(1))) ;
	DrawString(KNOB2_X+6,KNOB_ROW1+1,line,props) ;
	sprintf(line,"%.1f ms",MasterLimiter::AttackMsFromParam(param(2))) ;
	DrawString(7,KNOB_ROW2+1,line,props) ;
	float rel=MasterLimiter::ReleaseMsFromParam(param(3)) ;
	if (rel<0.0f) {
		strcpy(line,"auto") ;
	} else {
		sprintf(line,"%d ms",(int)(rel+0.5f)) ;
	}
	DrawString(KNOB2_X+6,KNOB_ROW2+1,line,props) ;

	FieldView::Redraw() ;

	static const char *help[4][2]={
		{"pushes the mix into it","00 = off, more = louder"},
		{"the loudest it may get","FC = -0.3 dB, FF = 0 dB"},
		{"looks ahead this long","short = snappy, long = soft"},
		{"how fast it lets go","00 = auto, 100-900 ms"},
	} ;
	int focus=GetFocusIndex() ;
	DrawString(1,HELP_ROW,"                            ",props) ;
	DrawString(1,HELP_ROW+1,"                            ",props) ;
	if (focus>=0 && focus<4) {
		DrawString(1,HELP_ROW,help[focus][0],props) ;
		SetColor(CD_MUTE) ;
		DrawString(1,HELP_ROW+1,help[focus][1],props) ;
	}
	SetColor(CD_MUTE) ;
	DrawString(1,HELP_ROW+3,"B+A default  RB+Left EQ",props) ;
	SetColor(CD_NORMAL) ;
	drawReadout() ;
}

// The state line and the live numbers (redrawn while the song plays)
void LimiterView::drawReadout() {
	GUITextProperties props ;
	char line[40] ;
	MasterLimiter *lim=MasterLimiter::GetInstance() ;
	bool on=param(0)!=0 ;
	SetColor(CD_HILITE1) ;
	DrawString(1,STATE_ROW,"LIMITER",props) ;
	// Up to column 19: the play state sits at the right of this row
	DrawString(9,STATE_ROW,"           ",props) ;
	if (on) {
		SetColor(CD_NORMAL) ;
		sprintf(line,"on %.1fms",MasterLimiter::AttackMsFromParam(param(2))) ;
	} else {
		SetColor(CD_MUTE) ;
		strcpy(line,"off") ;
	}
	DrawString(9,STATE_ROW,line,props) ;

	float gr=on?lim->GetGainReductionDb():0.0f ;
	SetColor(gr>0.05f?CD_NORMAL:CD_MUTE) ;
	DrawString(1,METER_ROW,"GR",props) ;
	sprintf(line,"%5.1f dB",gr>=0.05f?-gr:0.0f) ;
	DrawString(21,METER_ROW,line,props) ;
	SetColor(CD_MUTE) ;
	sprintf(line,"in %5.1f  out %5.1f dBFS",lim->GetInputPeakDb(),lim->GetOutputPeakDb()) ;
	DrawString(1,LEVELS_ROW,"                            ",props) ;
	DrawString(1,LEVELS_ROW,line,props) ;
	SetColor(CD_NORMAL) ;
}

void LimiterView::OnPlayerUpdate(PlayerEventType,unsigned int) {
	drawReadout() ;
}

// Gain-reduction bar, then the scope: one column per audio buffer, the mix
// level (after drive) rising from the bottom, the limiting hanging from the
// top, the ceiling as a line
void LimiterView::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	MasterLimiter *lim=MasterLimiter::GetInstance() ;
	bool on=param(0)!=0 ;
	GUIColor background=AppWindow::ThemeColor(CD_BACKGROUND) ;
	GUIColor slot=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,35) ;
	GUIColor level=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE2,55) ;
	GUIColor limiting=AppWindow::ThemeColor(CD_CURSOR) ;
	GUIColor ceiling=AppWindow::ThemeColor(CD_NORMAL) ;

	// Gain reduction: fills from the right (like a GR needle), 0..12 dB
	const int mx=4*8, mw=16*8, my=METER_ROW*8+1, mh=6 ;
	imp->SetColor(slot) ;
	GUIRect meter(mx,my,mx+mw,my+mh) ;
	imp->DrawRect(meter) ;
	float gr=on?lim->GetGainReductionDb():0.0f ;
	int gw=(int)(gr/GR_FULL_DB*mw+0.5f) ;
	if (gw>mw) gw=mw ;
	if (gw>0) {
		imp->SetColor(limiting) ;
		GUIRect bar(mx+mw-gw,my,mx+mw,my+mh) ;
		imp->DrawRect(bar) ;
	}

	const int top=SCOPE_TOP_ROW*8 ;
	const int h=SCOPE_ROWS*8 ;
	const int w=LIMITER_HISTORY*2 ;
	imp->SetColor(background) ;
	GUIRect area(SCOPE_X,top,SCOPE_X+w,top+h) ;
	imp->DrawRect(area) ;
	for (int i=0;i<LIMITER_HISTORY;i++) {
		int x=SCOPE_X+i*2 ;
		float in=lim->GetHistoryInDb(i) ;
		int lh=(int)((in-SCOPE_FLOOR_DB)/(-SCOPE_FLOOR_DB)*h) ;
		if (lh>h) lh=h ;
		if (lh>0) {
			imp->SetColor(level) ;
			GUIRect col(x,top+h-lh,x+2,top+h) ;
			imp->DrawRect(col) ;
		}
		float g=on?lim->GetHistoryGrDb(i):0.0f ;
		int gh=(int)(g/GR_FULL_DB*h+0.5f) ;
		if (gh>h) gh=h ;
		if (gh>0) {
			imp->SetColor(limiting) ;
			GUIRect col(x,top,x+2,top+gh) ;
			imp->DrawRect(col) ;
		}
	}
	// The ceiling (only means something while the limiter is on)
	if (on) {
		float c=MasterLimiter::CeilingDbFromParam(param(1)) ;
		int cy=top+h-(int)((c-SCOPE_FLOOR_DB)/(-SCOPE_FLOOR_DB)*h) ;
		if (cy<top) cy=top ;
		imp->SetColor(ceiling) ;
		for (int x=0;x<w;x+=6) {
			GUIRect dash(SCOPE_X+x,cy,SCOPE_X+x+3,cy+1) ;
			imp->DrawRect(dash) ;
		}
	}
	imp->SetColor(slot) ;
	GUIRect base(SCOPE_X,top+h,SCOPE_X+w,top+h+1) ;
	imp->DrawRect(base) ;
#endif
}
