#include "View.h"
#include "System/Console/Trace.h"
#include "Application/Player/Player.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include "Application/Model/Config.h"
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#include "ModalView.h"
#include <string.h>

bool View::initPrivate_=false ;

int View::margin_=0 ;
int View::songRowCount_; //=21 sets screen height among other things
bool View::miniLayout_=false ;
bool View::ultraCompactLayout_=false ;
int View::altRowNumber_ = 4;
int View::cursorAnimFrame_ = 0;
bool View::contextOverlay_ = false;
int View::contextOverlayPage_ = 0;

View::View(GUIWindow &w,ViewData *viewData):
	w_(w),
	modalView_(0),
	modalViewCallback_(0),
	hasFocus_(false)
{
	suppressPlaybackScope_=false;
  if (!initPrivate_)
  {
	   GUIRect rect=w.GetRect() ;
     miniLayout_=(rect.Width()<320);
     ultraCompactLayout_=(rect.Width()<=240 && rect.Height()<=240);
	   View::margin_=0 ;
		songRowCount_ = ultraCompactLayout_ ? 18 : (miniLayout_ ? 16 : 22); // RG Nano can fit more rows now

		const char *altRowStr = Config::GetInstance()->GetValue("ALTROWNUMBER");
		if (altRowStr) {
			altRowNumber_ = atoi(altRowStr);
		}

     initPrivate_=true ;
  }
	mask_=0 ;
	viewMode_=VM_NORMAL ;
	viewType_=VT_SONG ;
	locked_=false ;
	viewData_=viewData;
	NOTIFICATION_TIMEOUT = 1000;
	displayNotification_ = "";
} ;

GUIPoint View::GetAnchor() {
	// Calculate width based on actual screen dimensions
	// Each character is 8 pixels wide, so width in chars = screen width in pixels / 8
	GUIRect rect = w_.GetRect();
	int width = rect.Width() / 8;   // 30 for RG Nano (240px), 40 for standard (320px)
	int height = rect.Height() / 8; // 30 for both RG Nano and standard (240px)

	int anchorX = (width-SONG_CHANNEL_COUNT*3)/2+2;

	// On mini layouts (no map), we have more width available
	// Shift left by 1 to use more screen space
	if (miniLayout_) {
		// 8 channels × 3 chars = 24 chars, plus 3 for row numbers = 27 total
		// On 240px (30 chars): shift slightly left for better use of space
		anchorX = ((width - SONG_CHANNEL_COUNT*3 - 3) / 2) + 3 - 1;
	}

#ifdef PLATFORM_RGNANO
	Trace::Log("RGNANO","GetAnchor: rect.Width()=%d, width=%d, anchorX=%d", rect.Width(), width, anchorX);
#endif

	return GUIPoint(anchorX, (height-View::songRowCount_)/2 - 1) ;
}

GUIPoint View::GetTitlePosition() {
#ifndef PLATFORM_CAANOO
	return GUIPoint(0,0) ;
#else
	return GUIPoint(0,1) ;
#endif
} ;

bool View::Lock() {
	if (locked_) return false ;
	locked_=true ;
	return true ;
} ;

void View::WaitForObject() {
	while (locked_) {} ;
}

void View::Unlock() {
	locked_=false ;
}

void View::drawMap() {
    if (!miniLayout_) {
        GUIPoint anchor=GetAnchor() ;
		GUIPoint pos(View::margin_,anchor._y);
    	GUITextProperties props ;

		//draw entire map
		SetColor(CD_HILITE1) ;
    	char buffer[5] ;
		props.invert_=true ;
		//row1
		sprintf(buffer,"P G ");
        DrawString(pos._x,pos._y,buffer,props) ;
		pos._y++ ;		
		//row2
		sprintf(buffer,"SCPI");
        DrawString(pos._x,pos._y,buffer,props) ;
		pos._y++ ;		
		//row3
		sprintf(buffer,"  TT");
        DrawString(pos._x,pos._y,buffer,props) ;

		//draw current screen on map
		SetColor(CD_HILITE2) ;
		pos._y = anchor._y;
		switch(viewType_)
		{
		case VT_CHAIN:
			pos._x+=1;
			pos._y+=1;
	        DrawString(pos._x,pos._y,"C",props) ;
			break;
		case VT_PHRASE:
			pos._x+=2;
			pos._y+=1;
	        DrawString(pos._x,pos._y,"P",props) ;
			break;
		case VT_PROJECT:
	        DrawString(pos._x,pos._y,"P",props) ;
			break;
		case VT_INSTRUMENT:
			pos._x+=3;
			pos._y+=1;
	        DrawString(pos._x,pos._y,"I",props) ;
			break;
		case VT_TABLE: //under phrase
			pos._x+=2;
			pos._y+=2;
	        DrawString(pos._x,pos._y,"T",props) ;
			break;
		case VT_TABLE2: //under instrument
			pos._x+=3;
			pos._y+=2;
	        DrawString(pos._x,pos._y,"T",props) ;
			break;
		case VT_GROOVE:
			pos._x+=2;
	        DrawString(pos._x,pos._y,"G",props) ;
			break;
		default: //VT_SONG
			pos._y+=1;
	        DrawString(pos._x,pos._y,"S",props) ;
			int foo=0;
		}

	}//!minilayout
}

void View::drawOverlayLine(int x, int y, int width, const char *text,
                           GUITextProperties &props) {
	if (width<=0) return;
	char buffer[40];
	int maxWidth=width;
	if (maxWidth>38) maxWidth=38;
	int len=text ? (int)strlen(text) : 0;
	if (len>maxWidth) len=maxWidth;
	if (len>0) {
		strncpy(buffer,text,len);
	}
	for (int i=len;i<maxWidth;i++) {
		buffer[i]=' ';
	}
	buffer[maxWidth]=0;
	DrawString(x,y,buffer,props);
}

void View::drawContextMap(int x, int y, int width, GUITextProperties &props) {
	const char *row1="Project     Groove";
	const char *row2="   |           |";
	const char *row3="Song > Chain > Phrase";
	const char *row4="  |       |       |";
	const char *row5="Mixer   Table   Instr";
	drawOverlayLine(x,y,width,row1,props);
	drawOverlayLine(x,y+1,width,row2,props);
	drawOverlayLine(x,y+2,width,row3,props);
	drawOverlayLine(x,y+3,width,row4,props);
	drawOverlayLine(x,y+4,width,row5,props);

	int hx=x;
	int hy=y+2;
	const char *label="Song";
	switch(viewType_) {
		case VT_PROJECT:
			hx=x;
			hy=y;
			label="Project";
			break;
		case VT_GROOVE:
			hx=x+12;
			hy=y;
			label="Groove";
			break;
		case VT_CHAIN:
			hx=x+7;
			label="Chain";
			break;
		case VT_PHRASE:
			hx=x+15;
			label="Phrase";
			break;
		case VT_INSTRUMENT:
			hx=x+16;
			hy=y+4;
			label="Instr";
			break;
		case VT_MIXER:
			hx=x;
			hy=y+4;
			label="Mixer";
			break;
		case VT_TABLE:
			hx=x+8;
			hy=y+4;
			label="Table";
			break;
		case VT_TABLE2:
			hx=x+16;
			hy=y+4;
			label="Instr";
			break;
		default:
			hx=x;
			label="Song";
			break;
	}
	if (hx+((int)strlen(label))>x+width) {
		hx=x+width-(int)strlen(label);
	}
	if (hx<x) hx=x;
	SetColor(CD_HILITE1);
	props.invert_=true;
	drawOverlayLine(hx,hy,(int)strlen(label),label,props);
	props.invert_=false;
	SetColor(CD_NORMAL);
}

void View::drawContextOverlay() {
	GUITextProperties props;
	props.invert_=false;
	GUIRect rect=w_.GetRect();
	int width=rect.Width()/8;
	int height=rect.Height()/8;
	int x=ultraCompactLayout_ ? 0 : 6;
	int y=ultraCompactLayout_ ? 0 : 3;
	int boxW=ultraCompactLayout_ ? width : width-12;
	int boxH=ultraCompactLayout_ ? height : height-6;
	if (boxW<24) boxW=24;
	if (boxH<18) boxH=18;

	SetColor(CD_BACKGROUND);
	ClearRect(x,y,boxW,boxH);
	SetColor(CD_BORDER);
	for (int i=0;i<boxW;i++) {
		DrawString(x+i,y,"-",props);
		DrawString(x+i,y+boxH-1,"-",props);
	}
	for (int j=0;j<boxH;j++) {
		DrawString(x,y+j,"|",props);
		DrawString(x+boxW-1,y+j,"|",props);
	}
	DrawString(x,y,"+",props);
	DrawString(x+boxW-1,y,"+",props);
	DrawString(x,y+boxH-1,"+",props);
	DrawString(x+boxW-1,y+boxH-1,"+",props);

	const char *name="SONG";
	const char *where="RB+Right Chain";
	const char *edit="A+Dpad edit";
	const char *field="Rows=time Cols=tracks";
	const char *cmd1="Dpad move cursor";
	const char *cmd2="A paste/new item";
	const char *cmd3="A+Dpad edit value";
	const char *cmd4="Start play";
	const char *cmd5="A+R solo  B+R mute";
	const char *cmd6="RB+Dpad change view";
	const char *cmd7="RB+Select helper";
	switch(viewType_) {
		case VT_SONG:
			name="SONG";
			where="RB+Right Chain RB+Down Mix";
			edit="A+Dpad chain Start play";
			field="Rows=time Cols=tracks";
			cmd1="Dpad move row/track";
			cmd2="A paste/new chain";
			cmd3="A+Dpad edit chain";
			cmd4="Start play song row";
			cmd5="LB+Dpad jump/tempo";
			cmd6="A+R solo  B+R mute";
			cmd7="RB+Right Chain";
			break;
		case VT_CHAIN:
			name="CHAIN";
			where="RB+Left Song RB+Right Phr";
			edit="A+Dpad phrase/trans";
			field="Rows=phrase playlist";
			cmd1="Dpad move row/field";
			cmd2="A paste/new phrase";
			cmd3="A+Dpad edit value";
			cmd4="Start play cur ch";
			cmd5="RB+Start song ctx";
			cmd6="A+R solo  B+R mute";
			cmd7="RB+Right Phrase";
			break;
		case VT_PHRASE:
			name="PHRASE";
			where="RB+Left Chain RB+Right Inst";
			edit="A+Dpad notes/cmds";
			field="Rows=steps Cols=event";
			cmd1="Dpad move note/cmd";
			cmd2="A note preview/paste";
			cmd3="A+Dpad edit value";
			cmd4="LB+Dpad free note";
			cmd5="Select command picker";
			cmd6="Start play cur ch";
			cmd7="RB+Start song ctx";
			break;
		case VT_PROJECT:
			name="PROJECT";
			where="RB+Up Song";
			edit="Dpad field A+Dpad val";
			field="Tempo key scale render";
			cmd1="Dpad choose field";
			cmd2="A activate field";
			cmd3="A+Dpad edit value";
			cmd4="B secondary action";
			cmd5="Set tempo/key/scale";
			cmd6="Render/save here";
			cmd7="RB+Up Song";
			break;
		case VT_INSTRUMENT:
			name="INSTR";
			where="RB+Left Phrase Sel sample";
			edit="LB+LR page LB+UD mark";
			field="Sample lab + markers";
			cmd1="Dpad choose field";
			cmd2="A activate/sample";
			cmd3="A+Dpad edit value";
			cmd4="LB+A+LR nudge mark";
			cmd5="RB+A arrows audition";
			cmd6="Sel root detect/accept";
			cmd7="RB+Left Phrase";
			break;
		case VT_TABLE:
			name="TABLE";
			where="RB+Up Phrase";
			edit="A+Dpad command/value";
			field="Command mini-sequence";
			cmd1="Dpad move cmd/param";
			cmd2="Select command picker";
			cmd3="A+Dpad edit value";
			cmd4="A paste last cmd";
			cmd5="B+R interpolate";
			cmd6="Start play cur ch";
			cmd7="RB+Right Inst Table";
			break;
		case VT_TABLE2:
			name="INST TABLE";
			where="RB+Up Instrument";
			edit="A+Dpad command/value";
			field="Instrument motion";
			cmd1="Dpad move cmd/param";
			cmd2="Select command picker";
			cmd3="A+Dpad edit value";
			cmd4="A paste last cmd";
			cmd5="B+R interpolate";
			cmd6="Start play phrase";
			cmd7="RB+Left Table";
			break;
		case VT_GROOVE:
			name="GROOVE";
			where="RB+Up Phrase";
			edit="A+Dpad tick values";
			field="Timing/swing table";
			cmd1="Dpad move tick cell";
			cmd2="A+Dpad edit ticks";
			cmd3="B+Dpad prev/next";
			cmd4="Start play cur ch";
			cmd5="Groove changes feel";
			cmd6="Use GROV command";
			cmd7="RB+Down Phrase";
			break;
		case VT_MIXER:
			name="MIXER";
			where="RB+Up Song";
			edit="Left/Right select ch";
			field="Meters + master wave";
			cmd1="Left/Right select ch";
			cmd2="Start play song";
			cmd3="RB+Start stop";
			cmd4="Watch channel meters";
			cmd5="Master wave below";
			cmd6="Check level activity";
			cmd7="RB+Up Song";
			break;
		default:
			break;
	}
	CustomizeContextOverlay(name,where,edit,field,cmd1,cmd2,cmd3,cmd4,cmd5,cmd6,cmd7);

	int innerX=x+1;
	int innerW=boxW-2;
	if (innerW>28) innerW=28;

	SetColor(CD_HILITE2);
	drawOverlayLine(innerX,y+1,innerW,name,props);
	SetColor(CD_NORMAL);
	if (contextOverlayPage_ < 0 || contextOverlayPage_ > 2) {
		contextOverlayPage_ = 0;
	}
	if (contextOverlayPage_ == 0) {
		drawOverlayLine(innerX,y+3,innerW,"MAP",props);
		drawContextMap(innerX,y+4,innerW,props);

		SetColor(CD_HILITE1);
		drawOverlayLine(innerX,y+11,innerW,field,props);
		SetColor(CD_NORMAL);
		drawOverlayLine(innerX,y+13,innerW,where,props);
		drawOverlayLine(innerX,y+14,innerW,edit,props);
		SetColor(CD_HILITE2);
		drawOverlayLine(innerX,y+16,innerW,"Down: command list",props);
	} else if (contextOverlayPage_ == 2) {
		const char *steps[7];
		getHowToSteps(steps);
		drawOverlayLine(innerX,y+3,innerW,"HOW TO",props);
		SetColor(CD_NORMAL);
		for (int i=0;i<7;i++) {
			drawOverlayLine(innerX,y+5+i,innerW,steps[i],props);
		}
		SetColor(CD_HILITE1);
		drawOverlayLine(innerX,y+14,innerW,"Full guide: project website",props);
		SetColor(CD_NORMAL);
	} else {
		drawOverlayLine(innerX,y+3,innerW,"COMMANDS",props);
		SetColor(CD_HILITE1);
		drawOverlayLine(innerX,y+4,innerW,field,props);
		SetColor(CD_NORMAL);
		drawOverlayLine(innerX,y+6,innerW,cmd1,props);
		drawOverlayLine(innerX,y+7,innerW,cmd2,props);
		drawOverlayLine(innerX,y+8,innerW,cmd3,props);
		drawOverlayLine(innerX,y+9,innerW,cmd4,props);
		drawOverlayLine(innerX,y+10,innerW,cmd5,props);
		drawOverlayLine(innerX,y+11,innerW,cmd6,props);
		drawOverlayLine(innerX,y+12,innerW,cmd7,props);
		SetColor(CD_HILITE1);
		drawOverlayLine(innerX,y+14,innerW,where,props);
		SetColor(CD_NORMAL);
		drawOverlayLine(innerX,y+15,innerW,edit,props);
	}
	SetColor(CD_HILITE2);
	drawOverlayLine(innerX,y+boxH-2,innerW,"Up/Dn page RB+Sel close",props);
	SetColor(CD_NORMAL);
}

// Page 3 of the RB+Select helper: a tiny walkthrough for this screen,
// written for someone who has never used a tracker.
void View::getHowToSteps(const char **lines) {
	for (int i=0;i<7;i++) lines[i]="";
	switch(viewType_) {
		case VT_SONG:
			lines[0]="Song = whole track. Rows go";
			lines[1]="down in time, 8 columns =";
			lines[2]="8 instruments at once.";
			lines[3]="1 A on -- makes a chain";
			lines[4]="2 RB+Right opens that chain";
			lines[5]="3 Start plays from this row";
			lines[6]="Same row = plays together";
			break;
		case VT_CHAIN:
			lines[0]="Chain = a list of bars";
			lines[1]="(phrases) for one track.";
			lines[2]="1 A on -- adds a phrase";
			lines[3]="2 RB+Right opens it";
			lines[4]="3 next row = next bar";
			lines[5]="2nd column transposes it";
			lines[6]="RB+Left back to Song";
			break;
		case VT_PHRASE:
			lines[0]="Phrase = 1 bar, 16 steps.";
			lines[1]="A on a step adds a note";
			lines[2]="A+Left/Right: semitone";
			lines[3]="A+Up/Down: octave";
			lines[4]="I col: which sound (00-0F)";
			lines[5]="RB+Right: shape the sound";
			lines[6]="Start: loop this bar";
			break;
		case VT_INSTRUMENT:
			lines[0]="Instrument = the sound.";
			lines[1]="preset: A+Left/Right tries";
			lines[2]="ready sounds. Start = hear";
			lines[3]="LB+Left/Right: more pages";
			lines[4]="ENV: short or long notes";
			lines[5]="FILTER: dark or bright";
			lines[6]="MIX: reverb and echo";
			break;
		case VT_TABLE:
		case VT_TABLE2:
			lines[0]="Table = automation: each";
			lines[1]="row runs a command, one";
			lines[2]="row per tick. Loops.";
			lines[3]="Select: pick a command";
			lines[4]="VOLM fades, PTCH bends,";
			lines[5]="ARPG arpeggios";
			lines[6]="RB+Up back";
			break;
		case VT_GROOVE:
			lines[0]="Groove = step lengths.";
			lines[1]="6 6 = straight";
			lines[2]="7 5 = light swing";
			lines[3]="8 4 = heavy shuffle";
			lines[4]="A+Left/Right edits ticks";
			lines[5]="GROV command switches";
			lines[6]="RB+Down back to Phrase";
			break;
		case VT_PROJECT:
			lines[0]="Tempo: speed of the song";
			lines[1]="Key/Scale: notes snap to";
			lines[2]="the scale while editing";
			lines[3]="Reverb/Echo: shared FX,";
			lines[4]="instruments set sends";
			lines[5]="Render Stereo+Start = WAV";
			lines[6]="Save Song keeps your work";
			break;
		case VT_MIXER:
			lines[0]="Mixer shows each track's";
			lines[1]="level while playing.";
			lines[2]="Too loud? lower instrument";
			lines[3]="volume or Project Drive.";
			lines[4]="Start plays the song";
			lines[5]="RB+Start stops";
			lines[6]="RB+Up back to Song";
			break;
		default:
			break;
	}
}

void View::CustomizeContextOverlay(const char *&name, const char *&where,
                                   const char *&edit, const char *&field,
                                   const char *&cmd1, const char *&cmd2,
                                   const char *&cmd3, const char *&cmd4,
                                   const char *&cmd5, const char *&cmd6,
                                   const char *&cmd7) {
}

void View::drawPlaybackScope() {
	Player *player=Player::GetInstance();
	const char *label="STOP";
	if (player && player->IsRunning()) {
		switch(viewData_->playMode_) {
			case PM_SONG:
				label="PLAY:SONG";
				break;
			case PM_CHAIN:
				label="PLAY:CHAIN";
				break;
			case PM_PHRASE:
				label="PLAY:PHR";
				break;
			case PM_LIVE:
				label="PLAY:LIVE";
				break;
			case PM_AUDITION:
				label="AUDITION";
				break;
			default:
				label="PLAY:----";
				break;
		}
	}

	GUIRect rect=w_.GetRect();
	int width=rect.Width()/8;
	int x=width-10;
	if (x<0) x=0;
	int y=ultraCompactLayout_ ? 3 : 1;
	GUITextProperties props;
	props.invert_=false;
	SetColor((player && player->IsRunning()) ? CD_PLAY : CD_HILITE1);
	DrawString(x,y,"          ",props);
	DrawString(x,y,label,props);
	SetColor(CD_NORMAL);
}

void View::drawNotes() {

	GUIPoint anchor=GetAnchor() ;
		int initialX = anchor._x ;  // Align with song columns
		int initialY = anchor._y + View::songRowCount_ + 1 ;  // Just below song grid
		GUIPoint pos(initialX,initialY) ;
		GUITextProperties props ;

        Player *player=Player::GetInstance() ;
		
		//column banger refactor
		props.invert_= true;
        for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			if (i==viewData_->songX_) {
				SetColor(CD_HILITE2) ;
			} else {
				SetColor(CD_HILITE1) ;
			}
			if (player->IsRunning() && viewData_->playMode_ != PM_AUDITION) {
				DrawString(pos._x,pos._y,player->GetPlayedNote(i),props) ; //row for the note values
				pos._y++ ;
				DrawString(pos._x,pos._y,player->GetPlayedOctive(i),props) ; //row for the octive values
				pos._y++ ;
				DrawString(pos._x,pos._y,player->GetPlayedInstrument(i),props) ; //draw instrument number
			} else {
				DrawString(pos._x,pos._y,"  ",props) ; //row for the note values
				pos._y++ ;
				DrawString(pos._x,pos._y,"  ",props) ; //row for the octive values
				pos._y++ ;
				DrawString(pos._x,pos._y,"  ",props) ; //draw instrument number
			}
			pos._y = initialY ;
			pos._x+= 3;
		}
}

void View::drawMiniMeters() {
	if (!ultraCompactLayout_) {
		return;
	}
	Player *player=Player::GetInstance();
	if (!player || !player->IsRunning() || viewData_->playMode_ == PM_AUDITION) {
		return;
	}

	GUIPoint anchor=GetAnchor();
	int y = anchor._y + View::songRowCount_ + 4;
	if (y > 29) {
		return;
	}

	GUITextProperties props;
	props.invert_=true;
	int x = anchor._x;
	int masterLevel = MixerService::GetInstance()->GetMasterPeakPercent();
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		bool isPlaying = player->IsChannelPlaying(i);
		int level = MixerService::GetInstance()->GetBusPeakPercent(i);
		if (isPlaying && level < masterLevel) {
			level = masterLevel;
		}
		int width = 0;
		if (level > 4) width = 1;
		if (level > 28) width = 2;
		if (level > 62) width = 3;
		if (i==viewData_->songX_) {
			SetColor(CD_HILITE2);
		} else if (level > 62) {
			SetColor(CD_PLAY);
		} else {
			SetColor(CD_HILITE1);
		}
		if (!isPlaying) {
			DrawString(x,y,"   ",props);
		} else if (width==1) {
			DrawString(x,y,"=  ",props);
		} else if (width==2) {
			DrawString(x,y,"== ",props);
		} else {
			DrawString(x,y,"===",props);
		}
		x+=3;
	}
}

void View::drawMiniWaveform(bool force) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	if (!ultraCompactLayout_) {
		return;
	}

	Player *player = Player::GetInstance();
	unsigned int now = SDL_GetTicks();
	static unsigned int lastDrawMs = 0;
	if (!force && lastDrawMs != 0 && now - lastDrawMs < 33) {
		return;
	}
	lastDrawMs = now;

	SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
	MixerService *mixer = MixerService::GetInstance();
	GUIRect rect = w_.GetRect();
	const int x = 0;
	const int width = rect.Width();
	const int height = 16;
	const int y = rect.Height() - height;
	const int mid = y + (height / 2);
	const int columns = AudioMixer::WAVEFORM_SIZE;
	GUIColor scopeBackground=AppWindow::ThemeColor(CD_BACKGROUND);
	GUIColor scopeTrace=AppWindow::ThemeColor(CD_HILITE2);
	GUIColor scopeCenter=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);

	imp->SetColor(scopeBackground);
	GUIRect clear(x, y, x + width, y + height);
	imp->DrawRect(clear);

	if (!player || !player->IsRunning()) {
		return;
	}

	int peakAbs = 1;
	for (int i = 0; i < columns; i++) {
		int sample = mixer->GetMasterWaveformSample(i);
		int absSample = sample < 0 ? -sample : sample;
		if (absSample > peakAbs) {
			peakAbs = absSample;
		}
	}
	int gain = (peakAbs < 110) ? ((110 * 100) / peakAbs) : 100;
	if (gain > 620) {
		gain = 620;
	}

	imp->SetColor(scopeCenter);
	for (int col = 0; col < width; col += 12) {
		GUIRect center(x + col, mid, x + col + 2, mid + 1);
		imp->DrawRect(center);
	}

	int prevY = mid;
	for (int col = 0; col < width; col++) {
		int scaled = (col * (columns - 1) * 256) / (width - 1);
		int sampleIndex = scaled / 256;
		int frac = scaled - (sampleIndex * 256);
		int nextIndex = sampleIndex + 1;
		if (nextIndex >= columns) {
			nextIndex = columns - 1;
		}
		int sample = mixer->GetMasterWaveformSample(sampleIndex);
		int nextSample = mixer->GetMasterWaveformSample(nextIndex);
		int smoothed = sample + (((nextSample - sample) * frac) / 256);
		int waveY = mid - ((smoothed * gain * (height / 2 - 2)) / 10000);
		if (waveY < y + 2) {
			waveY = y + 2;
		}
		if (waveY >= y + height - 2) {
			waveY = y + height - 3;
		}

		int top = waveY < prevY ? waveY : prevY;
		int bottom = waveY > prevY ? waveY : prevY;
		if (top < y + 2) {
			top = y + 2;
		}
		if (bottom >= y + height - 2) {
			bottom = y + height - 3;
		}

		imp->SetColor(scopeTrace);
		GUIRect wave(x + col, top, x + col + 1, bottom + 1);
		imp->DrawRect(wave);
		prevY = waveY;
	}
#endif
}

void View::DoModal(ModalView *view,ModalViewCallback cb) {
	modalView_=view ;
	modalView_->OnFocus() ;
	modalViewCallback_=cb ;
	isDirty_=true ;
} ;

void View::Redraw() {
	if (modalView_) {
		if (isDirty_) {
			DrawView() ;
		}
		modalView_->Redraw() ;
	} else {
		DrawView() ;
		if (!suppressPlaybackScope_) {
			drawPlaybackScope();
		}
		if (contextOverlay_) {
			drawContextOverlay();
		}
	}
	isDirty_=false ;
} ;

void View::SetDirty(bool isDirty) {
	isDirty_=true ;
} ;

void View::ProcessButton(unsigned short mask, bool pressed) {
	isDirty_=false ;
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	const char *dumpInput = Config::GetInstance()->GetValue("DUMPEVENT");
	bool shouldLogInput = dumpInput && !strcmp(dumpInput, "YES");
	if (shouldLogInput) {
		Trace::Log("RGNANO_INPUT",
				   "view-button mask=0x%04X pressed=%d contextOverlay=%d page=%d modal=%d dirty=%d",
				   mask, pressed ? 1 : 0, contextOverlay_ ? 1 : 0,
				   contextOverlayPage_, modalView_ ? 1 : 0, isDirty_ ? 1 : 0);
		((AppWindow &)w_).LogDebugState("before-view-button", false);
	}
#endif

	// Increment cursor animation frame
	cursorAnimFrame_++;
	if (cursorAnimFrame_ > 60) cursorAnimFrame_ = 0;

	if (pressed && !modalView_) {
		if (mask == (EPBM_R|EPBM_SELECT)) {
			contextOverlay_ = !contextOverlay_;
			if (contextOverlay_) {
				contextOverlayPage_ = 0;
			}
			isDirty_ = true;
			((AppWindow &)w_).SetDirty();
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
			if (shouldLogInput) {
				Trace::Log("RGNANO_INPUT", "context-helper toggled open=%d page=%d",
						   contextOverlay_ ? 1 : 0, contextOverlayPage_);
				((AppWindow &)w_).LogDebugState("after-context-helper-toggle", true);
			}
#endif
			return;
		}
		if (contextOverlay_) {
			if (mask == EPBM_DOWN || mask == EPBM_UP) {
				// Map -> commands -> how-to, and back
				contextOverlayPage_ = (contextOverlayPage_ + (mask == EPBM_DOWN ? 1 : 2)) % 3;
				isDirty_ = true;
				((AppWindow &)w_).SetDirty();
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
				if (shouldLogInput) {
					Trace::Log("RGNANO_INPUT", "context-helper page=%d", contextOverlayPage_);
					((AppWindow &)w_).LogDebugState("after-context-helper-page", true);
				}
#endif
			}
			return;
		}
	}
	if (!pressed && contextOverlay_ && !modalView_) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
		if (shouldLogInput) {
			Trace::Log("RGNANO_INPUT", "release ignored by context helper mask=0x%04X", mask);
		}
#endif
		return;
	}

	if (modalView_) {
		modalView_->ProcessButton(mask,pressed);
		modalView_->isDirty_;
		if (modalView_->IsFinished()) {
			// process callback sending the modal dialog
			if (modalViewCallback_) {
				modalViewCallback_(*this,*modalView_) ;
			}
			SAFE_DELETE(modalView_) ;
			isDirty_=true ;
		}
	} else {
		ProcessButtonMask(mask,pressed);
		if (pressed && (mask & EPBM_START)) {
			isDirty_=true;
		}
	}
	if (isDirty_) ((AppWindow &)w_).SetDirty() ;
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	if (shouldLogInput) {
		((AppWindow &)w_).LogDebugState("after-view-button", pressed);
	}
#endif
} ;

void View::OnPlayerUpdate(PlayerEventType type, unsigned int currentTick) {
	// Propagate player updates to modal view if one is active
	if (modalView_) {
		modalView_->OnPlayerUpdate(type, currentTick);
	}
	// Subclasses can override this and add their own logic
}

void View::Clear() {
	((AppWindow &)w_).Clear() ;
}

void View::SetColor(ColorDefinition cd) {
	((AppWindow &)w_).SetColor(cd) ;
} ;

void View::ClearRect(int x,int y,int w,int h) {
	GUIRect rect(x,y,(x+w),(y+h)) ;
	w_.ClearRect(rect) ;
} ;

void View::DrawString(int x,int y,const char *txt,GUITextProperties &props) {
	GUIPoint pos(x,y) ;
	w_.DrawString(txt,pos,props) ;
} ;

/*
	Displays the saved notification for 1 second
*/
void View::EnableNotification() {
	if ((SDL_GetTicks() - notificationTime_) <= NOTIFICATION_TIMEOUT) {
		SetColor(CD_NORMAL);
		GUITextProperties props;
        int xOffset = 4;
        DrawString(xOffset, notiDistY_, displayNotification_.c_str(), props);
    } else {
		displayNotification_ = "";
	}
}

/*
    Set displayed notification
    Saves the current time
    Optionally set display y offset if not in a project (default == 2)
    Allows negative offsets, use with care!
*/
void View::SetNotification(const char *notification, int offset) {
    notificationTime_ = SDL_GetTicks();
    displayNotification_ = notification;
    notiDistY_ = offset;
    isDirty_ = true;
}
