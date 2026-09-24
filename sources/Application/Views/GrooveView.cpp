
#include "GrooveView.h"
#include "Application/Model/Groove.h"
#include "Application/Utils/char.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Application/AppWindow.h"
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdio.h>

GrooveView::GrooveView(GUIWindow &w,ViewData *viewData):View(w,viewData) {
	position_=0 ;
	lastPosition_=0 ;
}

GrooveView::~GrooveView() {
} 

void GrooveView::updateCursor(int dir) {
	position_+=dir ;
	if (position_<0) position_+=16 ;
	if (position_>15) position_-=16 ;
	isDirty_=true;
} ;

void GrooveView::updateCursorValue(int val,bool sync) {
	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	int value=grooveData[position_] ;
	val+=value ;
	if (val<1) val=1 ;
	if (val>0xF) val=0xF ;
	grooveData[position_]=val ;
	isDirty_=true; 
} ;

void GrooveView::warpGroove(int dir) {
	int current=viewData_->currentGroove_ ;
	current+=dir ;
	if (current>=MAX_GROOVES) {
		current-=MAX_GROOVES ;
	} ;
	if (current<0) {
		current+=MAX_GROOVES ;
	} ;
	viewData_->currentGroove_=current ;
	isDirty_=true ;
} ;

void GrooveView::initCursorValue() {
	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	if (grooveData[position_]==NO_GROOVE_DATA) {
		grooveData[position_]=1 ;
	} ;
	isDirty_=true ;
} ;

void GrooveView::clearCursorValue() {
	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	grooveData[position_]=NO_GROOVE_DATA ;
	isDirty_=true ;
}	

void GrooveView::ProcessButtonMask(unsigned short mask,bool pressed) {

	if (!pressed) return ;
	
	Player *player=Player::GetInstance() ;

	if (mask&EPBM_B) {         
			if (mask&EPBM_LEFT) {
				warpGroove(-1) ;
			}
			if (mask&EPBM_RIGHT) {
				warpGroove(1) ;
			}
			if (mask&EPBM_DOWN) {
				warpGroove(-0x10) ;
			}
			if (mask&EPBM_UP) {
				warpGroove(0x10) ;
			}
			if (mask&EPBM_A) {
				clearCursorValue() ;
			} ;
	} else {

	  // A modifier
	  if (mask&EPBM_A) {         
			if (mask&EPBM_LEFT) {
				updateCursorValue(-1) ;
			}
			if (mask&EPBM_RIGHT) {
				updateCursorValue(1) ;
			}
			if (mask&EPBM_DOWN) {
				updateCursorValue(-1,true) ;
			}
			if (mask&EPBM_UP) {
				updateCursorValue(1,true) ;
			}
			if (mask==EPBM_A) {
				initCursorValue() ;
			} ;
	  } else {
		  // R Modifier

          	if (mask&EPBM_R) {
				if (mask&EPBM_DOWN) {
					ViewType vt=VT_PHRASE;
					ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
					SetChanged();
					NotifyObservers(&ve) ;
				}
				if (mask&EPBM_START) {
					player->OnStartButton(PM_PHRASE,viewData_->songX_,true,viewData_->chainRow_) ;
    			}			

	    	} else {
                // No modifier
    			if (mask&EPBM_DOWN) updateCursor(1) ;
    			if (mask&EPBM_UP) updateCursor(-1) ;
    			if (mask&EPBM_START) {
					player->OnStartButton(PM_PHRASE,viewData_->songX_,false,viewData_->chainRow_) ;
    			}
		    }
	  } 
	    
	}
} ;

void GrooveView::DrawView() {

	Clear() ;

	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;

// Draw title

	char title[40] ;

	SetColor(CD_NORMAL) ;

	sprintf(title,"Groove %2.2X",viewData_->currentGroove_) ;
	DrawString(pos._x,pos._y,title,props) ;

// Compute song grid location

	GUIPoint anchor=GetAnchor() ;
	
// Display row numbers

	char buffer[6] ;
	pos=anchor ;
	pos._x-=3 ;
	for (int j=0;j<16;j++) {
		((j/altRowNumber_)%2)?SetColor(CD_ROW):SetColor(CD_ROW2);
		hex2char(j,buffer) ;
		DrawString(pos._x,pos._y,buffer,props) ;
		pos._y++ ;
	}


// Display current groove

	pos=anchor ;

	SetColor(CD_NORMAL) ;

	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	for (int j=0;j<16;j++) {
		if (grooveData[j]!=NO_GROOVE_DATA) {
			hex2char(grooveData[j],buffer) ;
			buffer[3]=0 ;
		} else {
			strcpy(buffer,"--") ;
		} ;
		props.invert_=(j==position_) ; 
		DrawString(pos._x,pos._y,buffer,props) ;
		pos._y++ ;
	}

	drawMap() ;
	drawNotes() ;
	drawTimingRuler(-1) ;
} ;

void GrooveView::OnPlayerUpdate(PlayerEventType ,unsigned int tick) {

	GUITextProperties props ;
	GUIPoint anchor=GetAnchor() ;
	GUIPoint pos ;

	pos._x=anchor._x-1 ;
	pos._y=anchor._y+lastPosition_ ;
	DrawString(pos._x,pos._y," ",props) ;
		
	Groove *gr=Groove::GetInstance() ;
	// Get current channel
	int channel=viewData_->songX_ ;

	int groove ;
	int groovepos ;

	gr->GetChannelData(channel,&groove,&groovepos) ;

	if (groove==viewData_->currentGroove_ &&
		viewData_->playMode_ != PM_AUDITION) {
		lastPosition_=groovepos ;
		pos._x=anchor._x-1 ;
		pos._y=anchor._y+lastPosition_ ;
        SetColor(CD_PLAY);
        DrawString(pos._x,pos._y,">",props);
        SetColor(CD_NORMAL);
		drawTimingRuler(groovepos) ;
	} ;

    drawNotes() ;
} ;

void GrooveView::OnFocus() {
} ;

// Right of the list: a bar of 16 steps as blocks whose width is their tick
// count, over an even grid, so swing and shuffle show as late offbeats.
void GrooveView::drawTimingRuler(int playEntry) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	if (!ultraCompactLayout_) {
		return ;
	}
	unsigned char *data=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	int seq[16] ;
	int count=0 ;
	while (count<16 && data[count]!=NO_GROOVE_DATA) {
		seq[count]=data[count] ;
		count++ ;
	}

	GUIPoint anchor=GetAnchor() ;
	const int x=(anchor._x+4)*8 ;
	const int w=w_.GetRect().Width()-6-x ;
	const int evenY=(anchor._y+1)*8 ;
	const int grooveY=(anchor._y+4)*8 ;
	const int barH=10 ;
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow() ;
	GUIColor background=AppWindow::ThemeColor(CD_BACKGROUND) ;
	imp->SetColor(background) ;
	GUIRect clear(x,evenY-2,x+w+2,grooveY+barH+2) ;
	imp->DrawRect(clear) ;

	GUITextProperties props ;
	SetColor(CD_HILITE1) ;
	DrawString(anchor._x+4,anchor._y,"even",props) ;
	DrawString(anchor._x+4,anchor._y+3,"this groove",props) ;
	SetColor(CD_NORMAL) ;
	if (count==0) {
		DrawString(anchor._x+4,anchor._y+6,"empty: add tick counts",props) ;
		return ;
	}

	int total=0 ;
	for (int i=0;i<16;i++) total+=seq[i%count] ;
	if (total<=0) total=1 ;

	GUIColor even=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45) ;
	GUIColor on=AppWindow::ThemeColor(CD_NORMAL) ;
	GUIColor beat=AppWindow::ThemeColor(CD_HILITE2) ;
	GUIColor play=AppWindow::ThemeColor(CD_PLAY) ;
	int acc=0 ;
	for (int i=0;i<16;i++) {
		// Even grid: 16 equal cells
		int ex0=x+(i*w)/16 ;
		int ex1=x+((i+1)*w)/16-1 ;
		imp->SetColor(even) ;
		GUIRect e(ex0,evenY,ex1,evenY+barH) ;
		imp->DrawRect(e) ;
		// Groove: cell widths follow the ticks
		int gx0=x+(acc*w)/total ;
		acc+=seq[i%count] ;
		int gx1=x+(acc*w)/total-1 ;
		bool playing=(playEntry>=0 && (i%count)==playEntry) ;
		imp->SetColor(playing?play:((i%4)==0?beat:on)) ;
		GUIRect g(gx0,grooveY,gx1,grooveY+barH) ;
		imp->DrawRect(g) ;
	}

	// Plain words for the feel
	char line[32] ;
	const char *feel="straight" ;
	if (count>=2 && seq[0]!=seq[1]) {
		feel=(seq[0]>seq[1])?"swing":"push" ;
	}
	int pair=(count>=2)?seq[0]+seq[1]:seq[0]*2 ;
	int percent=(pair>0)?(seq[0]*100)/pair:50 ;
	sprintf(line,"%s %d%%",feel,percent) ;
	SetColor(CD_HILITE2) ;
	DrawString(anchor._x+4,anchor._y+6,line,props) ;
	SetColor(CD_NORMAL) ;
	DrawString(anchor._x+4,anchor._y+8,"ticks per step",props) ;
	DrawString(anchor._x+4,anchor._y+9,"06 06 = even",props) ;
	DrawString(anchor._x+4,anchor._y+10,"07 05 = swing",props) ;
#endif
}
