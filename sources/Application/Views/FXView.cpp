#include "FXView.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Mixer/SendFX.h"
#include "Application/Model/Project.h"
#include "Application/Player/Player.h"
#include "BaseClasses/UIIntVarField.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

// Screen rows: a heading, a row of two knobs and a three-row picture per effect
#define CHORUS_ROW 4
#define ECHO_ROW 10
#define REVERB_ROW 16
#define HELP_ROW 23
#define PLOT_X 8
#define PLOT_W 224
#define KNOB2_X 15

FXView::FXView(GUIWindow &w,ViewData *data):FieldView(w,data) {
	viewType_=VT_FX ;
	Project *project=data->project_ ;
	struct Knob { FourCC id; int row; int x; const char *format; int max; int big; int min; } ;
	static const Knob knobs[6]={
		{VAR_CHORUS_RATE,CHORUS_ROW+1,1,"speed %2.2X",0xFF,0x10,0},
		{VAR_CHORUS_DEPTH,CHORUS_ROW+1,KNOB2_X,"depth %2.2X",0xFF,0x10,0},
		{VAR_DELAY_STEPS,ECHO_ROW+1,1,"time  %d/16",16,4,1},
		{VAR_DELAY_FEEDBACK,ECHO_ROW+1,KNOB2_X,"fdbk  %2.2X",0xFF,0x10,0},
		{VAR_REVERB_SIZE,REVERB_ROW+1,1,"size  %2.2X",0xFF,0x10,0},
		{VAR_REVERB_DAMP,REVERB_ROW+1,KNOB2_X,"damp  %2.2X",0xFF,0x10,0},
	} ;
	for (int i=0;i<6;i++) {
		Variable *v=project->FindVariable(knobs[i].id) ;
		NAssert(v) ;
		GUIPoint position(knobs[i].x,knobs[i].row) ;
		UIIntVarField *field=new UIIntVarField(position,*v,knobs[i].format,
		                                       knobs[i].min,knobs[i].max,1,knobs[i].big) ;
		T_SimpleList<UIField>::Insert(field) ;
	}
}

FXView::~FXView() {
}

int FXView::paramValue(FourCC id) {
	Variable *v=viewData_->project_->FindVariable(id) ;
	return v?v->GetInt():0 ;
}

// How many instruments send something to this effect
int FXView::sendCount(FourCC sampleId,FourCC synthId) {
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	int count=0 ;
	for (int i=0;i<MAX_INSTRUMENT_COUNT;i++) {
		I_Instrument *instr=bank->GetInstrument(i) ;
		if (!instr) continue ;
		Variable *v=instr->FindVariable(instr->GetType()==IT_SYNTH?synthId:sampleId) ;
		if (v && v->GetInt()>0) count++ ;
	}
	return count ;
}

void FXView::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;
	if (mask==EPBM_UP || mask==EPBM_DOWN) {
		// Up/Down walk the six knobs in reading order, row by row
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
		// B+A: this knob back to its default, as everywhere
		UIIntVarField *focus=(UIIntVarField *)GetFocus() ;
		if (focus) focus->GetVariable().ResetToDefault() ;
		isDirty_=true ;
		return ;
	}
	FieldView::ProcessButtonMask(mask) ;
	if (mask&EPBM_R) {
		if (mask&EPBM_RIGHT) {
			ViewType vt=VT_EQ ;
			ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
			SetChanged() ;
			NotifyObservers(&ve) ;
		}
		if (mask&EPBM_UP) {
			ViewType vt=VT_MIXER ;
			ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
			SetChanged() ;
			NotifyObservers(&ve) ;
		}
	} else if (mask==EPBM_START) {
		Player::GetInstance()->OnStartButton(PM_SONG,viewData_->songX_,false,viewData_->songX_) ;
	}
	isDirty_=true ;
}

void FXView::drawSection(int row,const char *name,const char *units,int users) {
	GUITextProperties props ;
	char line[32] ;
	SetColor(CD_HILITE1) ;
	DrawString(1,row,name,props) ;
	SetColor(CD_NORMAL) ;
	DrawString(8,row,"                     ",props) ;
	DrawString(8,row,units,props) ;
	// Who uses it: the instruments with a send above zero
	SetColor(CD_MUTE) ;
	if (users>0) {
		sprintf(line,"%2d sound%s",users,users==1?" ":"s") ;
	} else {
		strcpy(line,"  unused") ;
	}
	DrawString(29-(int)strlen(line),row,line,props) ;
	SetColor(CD_NORMAL) ;
}

void FXView::DrawView() {
	Clear() ;
	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;
	SetColor(CD_NORMAL) ;
	DrawString(pos._x,pos._y,"FX",props) ;
	SetColor(CD_MUTE) ;
	DrawString(pos._x+3,pos._y,"shared by all sounds",props) ;

	char units[32] ;
	float rate=SendFX::ChorusRateFromParam(paramValue(VAR_CHORUS_RATE)) ;
	float depth=SendFX::ChorusDepthMsFromParam(paramValue(VAR_CHORUS_DEPTH)) ;
	sprintf(units,"%.1fHz %.1fms",rate,depth) ;
	drawSection(CHORUS_ROW,"CHORUS",units,sendCount(SIP_CHORUS,SYP_CHORUS)) ;

	int tempo=viewData_->project_->GetTempo() ;
	if (tempo<1) tempo=120 ;
	int steps=SendFX::DelayStepsFromParam(paramValue(VAR_DELAY_STEPS)) ;
	float feedback=(paramValue(VAR_DELAY_FEEDBACK)/255.0f)*0.9f ;
	// Repeats until the echo has faded by 40 dB
	int repeats=feedback>0.0f?(int)(log(0.01)/log(feedback))+1:1 ;
	sprintf(units,"%dms x%d",(steps*15000)/tempo,repeats) ;
	drawSection(ECHO_ROW,"ECHO",units,sendCount(SIP_DELAY,SYP_DELAY)) ;

	float gain=0.7f+SendFX::ReverbSizeFromParam(paramValue(VAR_REVERB_SIZE))*0.28f ;
	float tail=3.0f*0.0311f/(float)-log10(gain) ;
	sprintf(units,"tail %.1fs",tail) ;
	drawSection(REVERB_ROW,"REVERB",units,sendCount(SIP_REVERB,SYP_REVERB)) ;

	FieldView::Redraw() ;

	// The focused knob in words
	static const char *help[6][2]={
		{"how fast the pitch wobbles","low = slow, lush sweep"},
		{"how far the pitch wobbles","high = seasick, low = wide"},
		{"echo time in 16th steps","4/16 = one beat"},
		{"how many echoes come back","high = long trail"},
		{"how long the room rings","high = hall, low = room"},
		{"how dark the tail gets","high = soft, low = bright"},
	} ;
	int focus=GetFocusIndex() ;
	SetColor(CD_NORMAL) ;
	DrawString(1,HELP_ROW,"                            ",props) ;
	DrawString(1,HELP_ROW+1,"                            ",props) ;
	if (focus>=0 && focus<6) {
		DrawString(1,HELP_ROW,help[focus][0],props) ;
		SetColor(CD_MUTE) ;
		DrawString(1,HELP_ROW+1,help[focus][1],props) ;
	}
	SetColor(CD_MUTE) ;
	DrawString(1,HELP_ROW+3,"sends: instrument MIX page",props) ;
	DrawString(1,HELP_ROW+4,"RB+Up Mixer  RB+Right EQ",props) ;
	SetColor(CD_NORMAL) ;
}

void FXView::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	drawChorusPlot((CHORUS_ROW+2)*8+2,20) ;
	drawEchoPlot((ECHO_ROW+2)*8+2,20) ;
	drawReverbPlot((REVERB_ROW+2)*8+2,20) ;
#endif
}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
static void clearPlot(SDLGUIWindowImp *imp,int y,int h) {
	{
		GUIColor color=AppWindow::ThemeColor(CD_BACKGROUND) ;
		imp->SetColor(color) ;
	}
	GUIRect area(PLOT_X,y,PLOT_X+PLOT_W,y+h) ;
	imp->DrawRect(area) ;
	{
		GUIColor color=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,35) ;
		imp->SetColor(color) ;
	}
	GUIRect base(PLOT_X,y+h-1,PLOT_X+PLOT_W,y+h) ;
	imp->DrawRect(base) ;
}
#endif

// Two seconds of the delay sweep, left and right a quarter cycle apart;
// the swing is drawn to scale against the widest depth
void FXView::drawChorusPlot(int y,int h) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	clearPlot(imp,y,h) ;
	float rate=SendFX::ChorusRateFromParam(paramValue(VAR_CHORUS_RATE)) ;
	float swing=SendFX::ChorusDepthMsFromParam(paramValue(VAR_CHORUS_DEPTH))/7.5f ;
	int mid=y+(h-2)/2 ;
	int amp=(h-4)/2 ;
	GUIColor colors[2]={AppWindow::ThemeBlend(CD_BACKGROUND,CD_NORMAL,45),
	                    AppWindow::ThemeColor(CD_HILITE2)} ;
	for (int c=0;c<2;c++) {
		imp->SetColor(colors[c]) ;
		int previous=-1 ;
		for (int x=0;x<PLOT_W;x++) {
			float t=2.0f*x/PLOT_W ;
			float lfo=(float)sin(6.28318530718*(rate*t+(1-c)*0.25f)) ;
			int py=mid-(int)(lfo*swing*amp) ;
			int top=previous<0?py:(previous<py?previous:py) ;
			int bottom=previous<0?py:(previous>py?previous:py) ;
			GUIRect dot(PLOT_X+x,top,PLOT_X+x+1,bottom+1) ;
			imp->DrawRect(dot) ;
			previous=py ;
		}
	}
#endif
}

// Two bars of 16th steps: the dry hit, then each echo at its real height
void FXView::drawEchoPlot(int y,int h) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	clearPlot(imp,y,h) ;
	const int span=32 ;
	int steps=SendFX::DelayStepsFromParam(paramValue(VAR_DELAY_STEPS)) ;
	float feedback=(paramValue(VAR_DELAY_FEEDBACK)/255.0f)*0.9f ;
	int bottom=y+h-1 ;
	// Beat marks
	{
		GUIColor color=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,60) ;
		imp->SetColor(color) ;
	}
	for (int s=0;s<=span;s+=4) {
		int x=PLOT_X+(s*(PLOT_W-4))/span ;
		GUIRect mark(x,bottom-2,x+1,bottom) ;
		imp->DrawRect(mark) ;
	}
	float level=1.0f ;
	for (int s=0;s<=span && level>0.02f;s+=steps) {
		int x=PLOT_X+(s*(PLOT_W-4))/span ;
		int bar=(int)(level*(h-2)) ;
		if (bar<1) bar=1 ;
		{
			GUIColor color=s==0?AppWindow::ThemeColor(CD_NORMAL):AppWindow::ThemeColor(CD_HILITE2) ;
			imp->SetColor(color) ;
		}
		GUIRect tap(x,bottom-bar,x+4,bottom) ;
		imp->DrawRect(tap) ;
		// The first echo comes back at the send level, then fades by fdbk
		level=(s==0)?0.9f:level*feedback ;
	}
#endif
}

// Four seconds of the tail: the whole sound, and its highs, which the
// damping takes away a little on every pass round the room
void FXView::drawReverbPlot(int y,int h) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	clearPlot(imp,y,h) ;
	float gain=0.7f+SendFX::ReverbSizeFromParam(paramValue(VAR_REVERB_SIZE))*0.28f ;
	float damp=(paramValue(VAR_REVERB_DAMP)/255.0f)*0.4f ;
	const float pass=0.0311f ;
	float highLoss=(1.0f-damp)/(1.0f+damp) ;
	int bottom=y+h-1 ;
	GUIColor body=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE1,40) ;
	GUIColor highs=AppWindow::ThemeColor(CD_HILITE2) ;
	for (int x=0;x<PLOT_W;x++) {
		float t=4.0f*x/PLOT_W ;
		float passes=t/pass ;
		float amp=(float)pow(gain,passes) ;
		float hi=amp*(float)pow(highLoss,passes) ;
		int a=(int)(amp*(h-2)) ;
		int b=(int)(hi*(h-2)) ;
		if (a>0) {
			imp->SetColor(body) ;
			GUIRect col(PLOT_X+x,bottom-a,PLOT_X+x+1,bottom) ;
			imp->DrawRect(col) ;
		}
		if (b>0) {
			imp->SetColor(highs) ;
			GUIRect top(PLOT_X+x,bottom-b,PLOT_X+x+1,bottom-b+1) ;
			imp->DrawRect(top) ;
		}
	}
#endif
}
