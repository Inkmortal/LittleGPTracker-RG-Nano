#include "MessageBox.h"
#include <string.h>

static const char *buttonText[MBL_LAST] = {
	"Ok",
	"Yes",
	"Cancel",
	"No"
} ;

MessageBox::MessageBox(View &view,const char *message,int btnFlags):
	ModalView(view),
	message_(message) {

	// Reading order, so Cancel ends up last (and selected by default)
	static const int order[MBL_LAST]={MBL_OK,MBL_YES,MBL_NO,MBL_CANCEL} ;
	buttonCount_=0 ;
	for (int i=0;i<MBL_LAST;i++) {
		if (btnFlags&(1<<(order[i]))) {
			button_[buttonCount_]=order[i] ;
			buttonCount_++ ;
		}
	}
	selected_=buttonCount_-1 ;
	NAssert(buttonCount_!=0) ;
} ;

MessageBox::~MessageBox() {
} ;

void MessageBox::DrawView() {
// message size
	int size=message_.size() ;

// buttons are drawn with a 3 space gap between them

	int buttonsWidth=0 ;
	for (int i=0;i<buttonCount_;i++) {
		buttonsWidth+=strlen(buttonText[button_[i]]) ;
	}
	buttonsWidth+=3*(buttonCount_-1) ;
	int width=(size>buttonsWidth)?size:buttonsWidth ;
	width+=2 ;
	SetWindow(width,3) ;

// draw text

	int y=0 ;
	int x=(width-size)/2;
	GUITextProperties props ;
	SetColor(CD_NORMAL) ;
	DrawString(x,y,message_.c_str(),props) ;
	
	y=2 ;
	x=(width-buttonsWidth)/2 ;
	for (int i=0;i<buttonCount_;i++) {
		const char *text=buttonText[button_[i]] ;
		bool on=(i==selected_) ;
		SetColor(on?CD_CURSOR:CD_NORMAL) ;
		props.invert_=on ;
		DrawString(x,y,text,props) ;
		x+=strlen(text)+3 ;
	}
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
} ;

void MessageBox::OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) {
} ;
void MessageBox::OnFocus() {
} ;
void MessageBox::CustomizeContextOverlay(
	const char *&name, const char *&where, const char *&edit,
	const char *&field, const char *&cmd1, const char *&cmd2,
	const char *&cmd3, const char *&cmd4, const char *&cmd5,
	const char *&cmd6, const char *&cmd7) {
	name="CONFIRM";
	where="Dialog choice";
	edit="A confirm";
	field="Confirm/cancel";
	cmd1="Left/Right choose";
	cmd2="A confirm choice";
	cmd3="B backs out";
	cmd4="Read message first";
	cmd5="Use No if unsure";
	cmd6="Delete is final";
	cmd7="RB+Select helper";
}
void MessageBox::SelectButton(int button) {
	for (int i=0;i<buttonCount_;i++) {
		if (button_[i]==button) selected_=i ;
	}
}

void MessageBox::ProcessButtonMask(unsigned short mask,bool pressed) {
	// Act on the press only (the release used to move the selection back)
	if (!pressed) return ;
	if (mask==EPBM_A) {
		EndModal(button_[selected_]) ;
		return ;
	}
	if (mask==EPBM_B) {
		// B always backs out without doing anything
		EndModal(MBL_CANCEL) ;
		return ;
	}
	if (mask==EPBM_LEFT) {
		selected_=(selected_+buttonCount_-1)%buttonCount_ ;
	}
	if (mask==EPBM_RIGHT) {
		selected_=(selected_+1)%buttonCount_ ;
	}
	isDirty_=true ;
} ;

