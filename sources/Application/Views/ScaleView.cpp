#include "ScaleView.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Model/Project.h"
#include "Application/Model/Scale.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "BaseClasses/UIIntVarField.h"
#include "BaseClasses/UINoteNameVarField.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string.h>

// Text rows (8 px each)
#define KEY_ROW 4
#define BLACK_LABEL_ROW 7
#define KEYS_TOP_ROW 8
#define KEYS_ROWS 7
#define WHITE_LABEL_ROW 16
#define NOTES_ROW 18
#define HELP_ROW 22
#define HINT_ROW 25
// Keyboard picture: 7 white keys 32 px wide from x = 8
#define KEYS_X 8
#define WHITE_W 32
#define BLACK_W 20
#define BLACK_H 34

// Which white key each note sits on, or the white key left of a black one
static const int whiteIndex[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
static const bool isBlack[12] = {false, true,  false, true,  false, false,
                                 true,  false, true,  false, true,  false};

ScaleView::ScaleView(GUIWindow &w,ViewData *data):FieldView(w,data) {
	viewType_=VT_SCALE ;
	onKeys_=false ;
	keyCursor_=0 ;
	Project *project=data->project_ ;
	Variable *noteNames=project->FindVariable(VAR_NOTE_NAMES) ;
	Variable *key=project->FindVariable(VAR_SCALE_KEY) ;
	Variable *scale=project->FindVariable(VAR_SCALE) ;
	NAssert(key && scale) ;
	GUIPoint position(1,KEY_ROW) ;
	T_SimpleList<UIField>::Insert(
		new UINoteNameVarField(position,*key,noteNames,"Key:   %s",-1,11,1,1)) ;
	position._y++ ;
	T_SimpleList<UIField>::Insert(
		new UIIntVarField(position,*scale,"Scale: %s",0,scaleCount-1,1,10)) ;
}

ScaleView::~ScaleView() {
}

void ScaleView::OnFocus() {
	// Start on the keyboard's key note, the obvious place to begin
	int key=viewData_->project_->GetScaleKey() ;
	if (!onKeys_) {
		keyCursor_=(key<0)?0:key ;
	}
}

int ScaleView::mask() {
	Project *project=viewData_->project_ ;
	return scaleMask(project->GetScale(),project->GetScaleCustomMask()) ;
}

void ScaleView::hearNote(int note) {
	Player *player=Player::GetInstance() ;
	// Never cut the song short for a preview
	if (player->IsRunning() && viewData_->playMode_!=PM_AUDITION) return ;
	int i=viewData_->currentInstrument_ ;
	I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(i) ;
	if (!instr) return ;
	int base=60 ;
	if (instr->GetType()==IT_SAMPLE) {
		// Around the sample's own root, where it sounds natural
		Variable *root=instr->FindVariable(SIP_ROOTNOTE) ;
		if (root) base=root->GetInt()-(root->GetInt()%12) ;
	}
	player->AuditionInstrument(i,base+note) ;
}

void ScaleView::setNote(int note,bool in) {
	Project *project=viewData_->project_ ;
	Variable *keyVar=project->FindVariable(VAR_SCALE_KEY) ;
	Variable *scaleVar=project->FindVariable(VAR_SCALE) ;
	Variable *customVar=project->FindVariable(VAR_SCALE_CUSTOM) ;
	int key=project->GetScaleKey() ;
	if (key<0) {
		// A scale needs a key to hang on: start from C
		keyVar->SetInt(0) ;
		key=0 ;
		SetNotification("Key set to C") ;
	}
	int step=(note-key+12)%12 ;
	if (step==0) {
		if (!in) {
			SetNotification("The key note stays in") ;
		}
		hearNote(note) ;
		return ;
	}
	int scale=project->GetScale() ;
	int m=scaleMask(scale,project->GetScaleCustomMask()) ;
	bool was=((m>>step)&1)!=0 ;
	if (was==in) {
		if (in) hearNote(note) ;
		return ;
	}
	if (scale!=scaleCustom) {
		// Editing a scale makes it the song's own, starting from its notes
		scaleVar->SetInt(scaleCustom) ;
		SetNotification("Now your Custom scale") ;
	}
	m=in?(m|(1<<step)):(m&~(1<<step)) ;
	customVar->SetInt(m) ;
	if (in) hearNote(note) ;
}

void ScaleView::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;
	if (mask&EPBM_R) {
		if (mask==(EPBM_R|EPBM_LEFT)) {
			ViewType vt=VT_PROJECT ;
			ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
			SetChanged() ;
			NotifyObservers(&ve) ;
		}
		return ;
	}
	if (mask==EPBM_START) {
		Player::GetInstance()->OnStartButton(PM_SONG,viewData_->songX_,false,viewData_->songX_) ;
		isDirty_=true ;
		return ;
	}
	UIField *fields[2]={0,0} ;
	int count=0 ;
	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
	for (it->Begin();!it->IsDone() && count<2;it->Next()) {
		fields[count++]=&it->CurrentItem() ;
	}
	if (onKeys_) {
		if (mask==EPBM_LEFT) {
			keyCursor_=(keyCursor_+11)%12 ;
		} else if (mask==EPBM_RIGHT) {
			keyCursor_=(keyCursor_+1)%12 ;
		} else if (mask==EPBM_UP || mask==EPBM_DOWN) {
			// Up: back to Scale; Down wraps round to Key
			onKeys_=false ;
			SetFocus(fields[mask==EPBM_UP?1:0]) ;
		} else if (mask==EPBM_A) {
			int key=viewData_->project_->GetScaleKey() ;
			bool in=(key>=0) && ((this->mask()>>((keyCursor_-key+12)%12))&1) ;
			setNote(keyCursor_,!in) ;
		} else if (mask==(EPBM_B|EPBM_A)) {
			setNote(keyCursor_,false) ;
		}
		isDirty_=true ;
		return ;
	}
	int focus=GetFocusIndex() ;
	if ((mask==EPBM_DOWN && focus==1) || (mask==EPBM_UP && focus==0)) {
		onKeys_=true ;
		if (GetFocus()) GetFocus()->ClearFocus() ;
		isDirty_=true ;
		return ;
	}
	if (mask==(EPBM_B|EPBM_A)) {
		// Key back to none, Scale back to chromatic
		UIIntVarField *field=(UIIntVarField *)GetFocus() ;
		if (field) field->GetVariable().ResetToDefault() ;
		isDirty_=true ;
		return ;
	}
	FieldView::ProcessButtonMask(mask) ;
	isDirty_=true ;
}

void ScaleView::DrawView() {
	Clear() ;
	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;
	SetColor(CD_NORMAL) ;
	DrawString(pos._x,pos._y,"Scale",props) ;
	SetColor(CD_MUTE) ;
	DrawString(pos._x+6,pos._y,"the song's notes",props) ;

	if (onKeys_ && GetFocus()) {
		// The keyboard has the cursor: Key/Scale drawn unlit
		GetFocus()->ClearFocus() ;
	}
	FieldView::Redraw() ;

	Project *project=viewData_->project_ ;
	int mode=project->GetNoteNameMode() ;
	int key=project->GetScaleKey() ;
	int m=mask() ;
	char name[4] ;

	// Note names above the black keys and under the white ones
	for (int n=0;n<12;n++) {
		const char *label=getNoteName(n,mode) ;
		name[0]=label[0] ;
		name[1]=(label[1]==' ')?0:label[1] ;
		name[2]=0 ;
		bool in=(key<0) || ((m>>((n-key+12)%12))&1) ;
		bool cursor=onKeys_ && n==keyCursor_ ;
		SetColor(cursor?CD_CURSOR:(in?CD_NORMAL:CD_MUTE)) ;
		props.invert_=cursor ;
		int col=isBlack[n]?(4+4*whiteIndex[n]):(2+4*whiteIndex[n]) ;
		DrawString(col,isBlack[n]?BLACK_LABEL_ROW:WHITE_LABEL_ROW,name,props) ;
		props.invert_=false ;
	}

	// The notes in order from the key
	char line[64] ;
	if (key<0) {
		SetColor(CD_MUTE) ;
		DrawString(1,NOTES_ROW,"No key: every note fits",props) ;
		DrawString(1,NOTES_ROW+1,"(A on a key picks C)",props) ;
	} else {
		int count=0 ;
		line[0]=0 ;
		for (int step=0;step<12;step++) {
			if (!((m>>step)&1)) continue ;
			const char *label=getNoteName((key+step)%12,mode) ;
			char one[4]={label[0],(char)(label[1]==' '?0:label[1]),0,0} ;
			if (count>0) strcat(line," ") ;
			strcat(line,one) ;
			count++ ;
		}
		char head[32] ;
		const char *keyLabel=getNoteName(key,mode) ;
		sprintf(head,"%d notes from %c%c",count,keyLabel[0],keyLabel[1]==' '?' ':keyLabel[1]) ;
		SetColor(CD_HILITE1) ;
		DrawString(1,NOTES_ROW,head,props) ;
		SetColor(CD_NORMAL) ;
		// At most 28 characters a line
		if ((int)strlen(line)<=28) {
			DrawString(1,NOTES_ROW+1,line,props) ;
		} else {
			char first[32] ;
			int cut=28 ;
			while (cut>0 && line[cut]!=' ') cut-- ;
			strncpy(first,line,cut) ;
			first[cut]=0 ;
			DrawString(1,NOTES_ROW+1,first,props) ;
			DrawString(1,NOTES_ROW+2,line+cut+1,props) ;
		}
	}

	// What the cursor does
	SetColor(CD_NORMAL) ;
	if (onKeys_) {
		DrawString(1,HELP_ROW,"A: note in or out",props) ;
		SetColor(CD_MUTE) ;
		DrawString(1,HELP_ROW+1,"editing makes it Custom",props) ;
	} else if (GetFocusIndex()==0) {
		DrawString(1,HELP_ROW,"the note the scale starts",props) ;
		SetColor(CD_MUTE) ;
		DrawString(1,HELP_ROW+1,"on (-- = no scale)",props) ;
	} else {
		DrawString(1,HELP_ROW,"notes snap to it when you",props) ;
		SetColor(CD_MUTE) ;
		DrawString(1,HELP_ROW+1,"edit, RAND stays in it",props) ;
	}
	SetColor(CD_MUTE) ;
	DrawString(1,HINT_ROW,onKeys_?"B+A: out  Up: Scale":"Down: keys  B+A: default",props) ;
	DrawString(1,HINT_ROW+1,"Start: song  RB+Left Project",props) ;
	SetColor(CD_NORMAL) ;
	View::EnableNotification() ;
}

// One octave of piano keys: the scale's notes lit, the key in amber, a bar
// under the note the cursor is on
void ScaleView::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	Project *project=viewData_->project_ ;
	int key=project->GetScaleKey() ;
	int m=mask() ;
	const int top=KEYS_TOP_ROW*8 ;
	const int h=KEYS_ROWS*8 ;
	GUIColor background=AppWindow::ThemeColor(CD_BACKGROUND) ;
	GUIColor whiteOff=AppWindow::ThemeBlend(CD_BACKGROUND,CD_NORMAL,40) ;
	GUIColor blackOff=AppWindow::ThemeBlend(CD_BACKGROUND,CD_NORMAL,10) ;
	GUIColor whiteOn=AppWindow::ThemeColor(CD_HILITE2) ;
	GUIColor blackOn=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE2,70) ;
	GUIColor root=AppWindow::ThemeColor(CD_CURSOR) ;
	GUIColor cursor=AppWindow::ThemeColor(CD_CURSOR) ;

	imp->SetColor(background) ;
	GUIRect area(KEYS_X-2,top-2,KEYS_X+7*WHITE_W+2,top+h+6) ;
	imp->DrawRect(area) ;

	for (int pass=0;pass<2;pass++) {
		for (int n=0;n<12;n++) {
			if (isBlack[n]!=(pass==1)) continue ;  // white keys first
			bool in=(key>=0) && ((m>>((n-key+12)%12))&1) ;
			bool isRoot=(key==n) ;
			GUIColor fill=whiteOff ;
			if (isRoot) fill=root ;
			else if (in) fill=isBlack[n]?blackOn:whiteOn ;
			else fill=isBlack[n]?blackOff:whiteOff ;
			int x0,x1,y1 ;
			if (isBlack[n]) {
				int centre=KEYS_X+WHITE_W*(whiteIndex[n]+1) ;
				x0=centre-BLACK_W/2 ;
				x1=centre+BLACK_W/2 ;
				y1=top+BLACK_H ;
				// A dark edge so a black key stands out on lit white keys
				imp->SetColor(background) ;
				GUIRect edge(x0-1,top,x1+1,y1+1) ;
				imp->DrawRect(edge) ;
			} else {
				x0=KEYS_X+WHITE_W*whiteIndex[n]+1 ;
				x1=KEYS_X+WHITE_W*(whiteIndex[n]+1)-1 ;
				y1=top+h ;
			}
			imp->SetColor(fill) ;
			GUIRect body(x0,top,x1,y1) ;
			imp->DrawRect(body) ;
			if (onKeys_ && n==keyCursor_) {
				// Cursor: a bar under the key (under a black key: on it)
				imp->SetColor(cursor) ;
				int barTop=isBlack[n]?y1+2:top+h+2 ;
				GUIRect bar(x0+2,barTop,x1-2,barTop+3) ;
				imp->DrawRect(bar) ;
			}
		}
	}
#endif
}
