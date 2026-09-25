#include "Application/Utils/UndoHistory.h"
#include "Application/Instruments/InstrumentBank.h"
#include "System/Console/CrashLog.h"
#include "View.h"
#include "System/Console/Trace.h"
#include "Application/Player/Player.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include "Application/Model/Config.h"
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#include "ModalView.h"
#include "Application/Views/ModalDialogs/GuideDialog.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Views/ModalDialogs/RenderToSampleDialog.h"
#include <string.h>

bool View::initPrivate_=false ;

int View::margin_=0 ;
bool View::undoGesture_=false ;
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

// The helper draws in screen coordinates, also when it is shown over a
// dialog (whose DrawString is offset to the dialog window).
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
	View::DrawString(x,y,buffer,props);
}

void View::drawContextMap(int x, int y, int width, GUITextProperties &props) {
	const char *row1="Project     Groove";
	const char *row2="   |           |";
	const char *row3="Song > Chain > Phrase";
	const char *row4="  |       |       |";
	const char *row5="Mixer   Table   Instr";
	const char *row6="  |";
	const char *row7="  FX > EQ";
	drawOverlayLine(x,y,width,row1,props);
	drawOverlayLine(x,y+1,width,row2,props);
	drawOverlayLine(x,y+2,width,row3,props);
	drawOverlayLine(x,y+3,width,row4,props);
	drawOverlayLine(x,y+4,width,row5,props);
	drawOverlayLine(x,y+5,width,row6,props);
	drawOverlayLine(x,y+6,width,row7,props);

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
		case VT_FX:
			hx=x+2;
			hy=y+6;
			label="FX";
			break;
		case VT_EQ:
			hx=x+7;
			hy=y+6;
			label="EQ";
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
	View::ClearRect(x,y,boxW,boxH);
	SetColor(CD_BORDER);
	for (int i=0;i<boxW;i++) {
		View::DrawString(x+i,y,"-",props);
		View::DrawString(x+i,y+boxH-1,"-",props);
	}
	for (int j=0;j<boxH;j++) {
		View::DrawString(x,y+j,"|",props);
		View::DrawString(x+boxW-1,y+j,"|",props);
	}
	View::DrawString(x,y,"+",props);
	View::DrawString(x+boxW-1,y,"+",props);
	View::DrawString(x,y+boxH-1,"+",props);
	View::DrawString(x+boxW-1,y+boxH-1,"+",props);

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
			cmd7="Select LIVE mode";
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
			cmd7="LB+Start render sample";
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
			cmd6="Start play  LB+St render";
			cmd7="RB+Start song ctx";
			break;
		case VT_PROJECT:
			name="PROJECT";
			where="RB+Down Song";
			edit="Dpad field A+Dpad val";
			field="Tempo key scale render";
			cmd1="Dpad choose field";
			cmd2="A activate field";
			cmd3="A+Dpad edit value";
			cmd4="B secondary action";
			cmd5="Set tempo/key/scale";
			cmd6="Render/save here";
			cmd7="RB+Down Song";
			break;
		case VT_INSTRUMENT:
			// Synth and sample instruments replace this (MIDI keeps it)
			name="INSTR";
			where="RB+Left Phrase";
			edit="A+Dpad edit value";
			field="MIDI instrument";
			cmd1="Dpad choose field";
			cmd2="A+Left/Right small step";
			cmd3="A+Up/Down big step";
			cmd4="type: synth/sample/MIDI";
			cmd5="B+Left/Right other instr";
			cmd6="RB+Up all sounds";
			cmd7="RB+Down instr table";
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
			cmd5="B+LB mark  B+RB interp";
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
			cmd5="B+LB mark  B+RB interp";
			cmd6="Start play phrase";
			cmd7="RB+Left Table";
			break;
		case VT_GROOVE:
			name="GROOVE";
			where="RB+Down Phrase";
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
			where="RB+Up Song  RB+Dn FX";
			edit="A+Up/Down fader";
			field="Track, FX and master";
			cmd1="Left/Right pick a strip";
			cmd2="A+Up/Down big step";
			cmd3="A+Left/Right fine";
			cmd4="B+RB mute  A+RB solo";
			cmd5="C D R = FX returns";
			cmd6="Start play/stop song";
			cmd7="RB+Down FX settings";
			break;
		case VT_FX:
			name="FX";
			where="RB+Up Mixer";
			edit="A+Dpad edit value";
			field="Chorus, echo, reverb";
			cmd1="Up/Down pick a knob";
			cmd2="A+Left/Right small step";
			cmd3="A+Up/Down big step";
			cmd4="Instruments send to FX";
			cmd5="Mixer C D R = returns";
			cmd6="Start play the song";
			cmd7="RB+Up Mixer RB+Right EQ";
			break;
		case VT_EQ:
			name="EQ";
			where="RB+Left FX";
			edit="A+Dpad edit value";
			field="Master low/mid/high";
			cmd1="Up/Down pick a knob";
			cmd2="A+Left/Right small step";
			cmd3="A+Up/Down big step";
			cmd4="gain 80 = flat";
			cmd5="B+A back to flat";
			cmd6="Start play the song";
			cmd7="RB+Left FX";
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
		SetColor(CD_NORMAL);
		drawOverlayLine(innerX,y+17,innerW,"B+Sel undo  LB+Sel redo",props);
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
		// The rest of this screen's keys, where the helper box is tall enough
		const char *more[10];
		int count=getMoreKeys(more,10);
		int room=boxH-3-(17+1);
		if (count>0 && room>1) {
			SetColor(CD_HILITE2);
			drawOverlayLine(innerX,y+17,innerW,"MORE KEYS",props);
			SetColor(CD_NORMAL);
			for (int i=0;i<count && i<room;i++) {
				drawOverlayLine(innerX,y+18+i,innerW,more[i],props);
			}
		}
	}
	SetColor(CD_CURSOR);
	drawOverlayLine(innerX,y+boxH-3,innerW,"A: full guide for this",props);
	SetColor(CD_HILITE2);
	drawOverlayLine(innerX,y+boxH-2,innerW,"Up/Dn page RB+Sel close",props);
	SetColor(CD_NORMAL);
}

// Keys the 7 command lines have no room for, per screen (helper page 2)
int View::getMoreKeys(const char **lines,int max) {
	static const char *song[]={"Select  LIVE mode on/off","Live: Start cue a cell",
		"Live: LB+Start cue the row","Live: RB+Start stop track","Live: B+Start stop all",
		"B+Up/Dn 16 rows  B+A delete","B+LB select, then B copy","A+LB paste",
		"B+RB mute  A+RB solo","RB+LB unmute all"};
	static const char *chain[]={"B+Dpad other chain/track","B+A delete",
		"B+LB select, then B copy","A+LB paste","2nd column: transpose",
		"B+RB mute  A+RB solo","RB+LB unmute all"};
	static const char *phrase[]={"B+Dpad other phrase/track","B+A delete",
		"B+LB select, then B copy","A+LB paste","Sel LB+R random  LB+L fill",
		"Sel LB+U shuffle LB+D rev","A+Up/Dn on cmd: A to Z",
		"LB+Start render to sample","RB+Up Groove RB+Dn Table"};
	static const char *instrument[]={"B+Dpad other instrument","B+A clear sample/table",
		"RB+Up list of all sounds","RB+Down instrument table","RB+Start play the song",
		"MOD page: LB+Up/Dn slot"};
	static const char *table[]={"B+Left/Right other table","B+A delete",
		"B+LB select, then B copy","A+LB paste","RB+Start play the song"};
	static const char *groove[]={"B+Left/Right other groove","B+A clear the step",
		"RB+Start play the song"};
	static const char *project[]={"Start play/stop the song","B on Tempo: tap tempo"};
	static const char *mixer[]={"B+RB mute  A+RB solo","A+Left/Right fine step","RB+LB unmute all",
		"C/D/R = FX returns M=master","RB+Down FX settings"};
	static const char *fx[]={"Up/Down next knob","A+Left/Right small step",
		"A+Up/Down big step","B+A knob to default","RB+Right EQ","Start play/stop the song"};
	static const char *eq[]={"Up/Down next knob","A+Left/Right small step",
		"A+Up/Down big step","B+A back to flat","RB+Left FX","Start play/stop the song"};
	const char **list=0;
	int count=0;
#define MORE_KEYS(a) list=a; count=sizeof(a)/sizeof(a[0]);
	switch(viewType_) {
		case VT_SONG: MORE_KEYS(song); break;
		case VT_CHAIN: MORE_KEYS(chain); break;
		case VT_PHRASE: MORE_KEYS(phrase); break;
		case VT_INSTRUMENT: MORE_KEYS(instrument); break;
		case VT_TABLE:
		case VT_TABLE2: MORE_KEYS(table); break;
		case VT_GROOVE: MORE_KEYS(groove); break;
		case VT_PROJECT: MORE_KEYS(project); break;
		case VT_MIXER: MORE_KEYS(mixer); break;
		case VT_FX: MORE_KEYS(fx); break;
		case VT_EQ: MORE_KEYS(eq); break;
		default: break;
	}
#undef MORE_KEYS
	// Dialogs have their own commands, not the screen's
	if (IsModal()) count=0;
	if (count>max) count=max;
	for (int i=0;i<count;i++) lines[i]=list[i];
	return count;
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
			lines[6]="Select: LIVE, cue cells";
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
			lines[2]="sounds. A+Start = hear";
			lines[3]="LB+Left/Right: more pages";
			lines[4]="ENV: short or long notes";
			lines[5]="FILTER dark/bright, MOD";
			lines[6]="sweeps+wobbles, MIX space";
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
			lines[3]="Master/Drive/Clip: output";
			lines[4]="FX: Mixer then RB+Down";
			lines[5]="Render Stereo+Start = WAV";
			lines[6]="Save Song keeps your work";
			break;
		case VT_MIXER:
			lines[0]="Pick a strip: 1-8 tracks,";
			lines[1]="C chorus D echo R reverb,";
			lines[2]="M master.";
			lines[3]="A+Up/Down moves the fader";
			lines[4]="B+RB mute, A+RB solo";
			lines[5]="Levels save with the song";
			lines[6]="RB+Down FX settings";
			break;
		case VT_FX:
			lines[0]="Shared effects for all";
			lines[1]="instruments. Turn up an";
			lines[2]="instrument's chorus/delay/";
			lines[3]="reverb send to use them.";
			lines[4]="Chorus: speed, depth";
			lines[5]="Echo: time, repeats";
			lines[6]="Reverb: size, damp";
			break;
		case VT_EQ:
			lines[0]="Shapes the whole mix.";
			lines[1]="LOW: bass shelf,";
			lines[2]="MID: a bell in the middle,";
			lines[3]="HIGH: treble shelf.";
			lines[4]="gain 80 = flat, +-12 dB";
			lines[5]="freq moves each band";
			lines[6]="B+A puts a knob back";
			break;
		default:
			break;
	}
	// A screen's page can tell its own story (the instrument's MOD page)
	CustomizeHowToSteps(lines);
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
		
		// Track strip under the grid. Stopped: track numbers with the current
		// one lit. Playing: each track's note, octave and instrument.
		bool live = player->IsRunning() && viewData_->playMode_ != PM_AUDITION;
		props.invert_ = false;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			bool current = (i==viewData_->songX_);
			if (live) {
				SetColor(current ? CD_HILITE2 : CD_NORMAL);
				DrawString(pos._x,pos._y,player->GetPlayedNote(i),props) ;
				pos._y++ ;
				DrawString(pos._x,pos._y,player->GetPlayedOctive(i),props) ;
				pos._y++ ;
				SetColor(current ? CD_HILITE2 : CD_MUTE);
				DrawString(pos._x,pos._y,player->GetPlayedInstrument(i),props) ;
			} else {
				char label[3] = {' ', (char)('1' + i), 0};
				SetColor(current ? CD_HILITE2 : CD_MUTE);
				DrawString(pos._x,pos._y,label,props) ;
				pos._y++ ;
				DrawString(pos._x,pos._y,"  ",props) ;
				pos._y++ ;
				DrawString(pos._x,pos._y,"  ",props) ;
			}
			pos._y = initialY ;
			pos._x+= 3;
		}
		SetColor(CD_NORMAL);
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
	props.invert_=false;
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
			SetColor(CD_MUTE);
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

static void renderToSampleCallback(View &v, ModalView &dialog) {
	RenderToSampleDialog &render=(RenderToSampleDialog &)dialog ;
	if (dialog.GetReturnCode()>0 && render.GetInstrument()>=0) {
		v.ShowInstrument(render.GetInstrument()) ;
	}
}

void View::ShowInstrument(int instrument) {
	viewData_->currentInstrument_=instrument ;
	ViewType vt=VT_INSTRUMENT ;
	ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
	SetChanged() ;
	NotifyObservers(&ve) ;
}

void View::renderToSample(int mode) {
	int bars=1 ;
	if (mode==PM_CHAIN) {
		// The chain plays its rows from the top until the first empty one
		unsigned char *data=viewData_->song_->chain_->data_+16*viewData_->currentChain_ ;
		bars=0 ;
		while (bars<16 && data[bars]!=0xFF) bars++ ;
		if (bars==0) {
			SetNotification("Chain is empty",0) ;
			return ;
		}
		viewData_->chainRow_=0 ;
	}
	DoModal(new RenderToSampleDialog(*this,mode,bars),renderToSampleCallback) ;
}

void View::drawPhraseRoll(int phrase, int x, int y, int w, int h, int playStep,
                          bool active, int cursorStep) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor background=AppWindow::ThemeColor(CD_BACKGROUND);
	imp->SetColor(background);
	GUIRect clear(x,y,x+w,y+h);
	imp->DrawRect(clear);
	if (phrase<0 || phrase>=PHRASE_COUNT) {
		return;
	}
	Phrase *ph=viewData_->song_->phrase_;
	unsigned char *notes=ph->note_+16*phrase;
	FourCC *cmd1=ph->cmd1_+16*phrase;
	FourCC *cmd2=ph->cmd2_+16*phrase;
	const int cell=w/16;
	if (cell<2) {
		return;
	}

	if (playStep>=0 && playStep<16) {
		GUIColor head=AppWindow::ThemeBlend(CD_BACKGROUND,CD_PLAY,30);
		imp->SetColor(head);
		GUIRect col(x+playStep*cell,y,x+playStep*cell+cell,y+h);
		imp->DrawRect(col);
	}

	// Edit cursor: a mark along the bottom under its step
	if (cursorStep>=0 && cursorStep<16) {
		GUIColor cursor=AppWindow::ThemeColor(CD_CURSOR);
		imp->SetColor(cursor);
		GUIRect mark(x+cursorStep*cell,y+h-2,x+cursorStep*cell+cell-1,y+h);
		imp->DrawRect(mark);
	}

	// Beat dots along the bottom
	GUIColor dots=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	imp->SetColor(dots);
	for (int step=0;step<16;step+=4) {
		GUIRect dot(x+step*cell,y+h-1,x+step*cell+1,y+h);
		imp->DrawRect(dot);
	}

	int lo=255,hi=-1;
	for (int i=0;i<16;i++) {
		if (notes[i]==0xFF) continue;
		if (notes[i]<lo) lo=notes[i];
		if (notes[i]>hi) hi=notes[i];
	}
	if (hi<0) {
		return;
	}
	// Keep at least an octave of range so a repeated note sits in the middle
	if (hi-lo<12) {
		int mid=(hi+lo)/2;
		lo=mid-6;
		hi=mid+6;
	}
	const int noteH=(h>=24)?3:2;
	const int span=h-noteH-2;
	// Quiet when idle so it never competes with the numbers being edited;
	// the playing phrase lights up
	GUIColor head=active?AppWindow::ThemeColor(CD_HILITE2)
	                    :AppWindow::ThemeBlend(CD_BACKGROUND,CD_NORMAL,60);
	GUIColor tail=AppWindow::ThemeBlend(CD_BACKGROUND,active?CD_HILITE2:CD_NORMAL,active?45:30);
	for (int i=0;i<16;i++) {
		if (notes[i]==0xFF) continue;
		int ny=y+span-((notes[i]-lo)*span)/(hi-lo);
		int end=i+1;
		while (end<16 && notes[end]==0xFF && cmd1[end]!=I_CMD_KILL && cmd2[end]!=I_CMD_KILL) {
			end++;
		}
		if (end>i+1) {
			imp->SetColor(tail);
			GUIRect t(x+i*cell+cell-1,ny+noteH/2,x+end*cell-1,ny+noteH/2+1);
			imp->DrawRect(t);
		}
		imp->SetColor(head);
		GUIRect n(x+i*cell,ny,x+i*cell+cell-1,ny+noteH);
		imp->DrawRect(n);
	}
#endif
}

void View::GetGuideTopic(const char *&page, const char *&section) {
	page="screens";
	section="";
	switch(viewType_) {
		case VT_SONG:
			section=(Player::GetInstance()->GetSequencerMode()==SM_LIVE)?"Live mode":"Song" ;
			break;
		case VT_CHAIN: section="Chain"; break;
		case VT_PHRASE: section="Phrase"; break;
		case VT_INSTRUMENT: section="Instrument (synth)"; break;
		case VT_TABLE:
		case VT_TABLE2: section="Table"; break;
		case VT_GROOVE: section="Groove"; break;
		case VT_PROJECT: section="Project"; break;
		case VT_MIXER: section="Mixer"; break;
		case VT_FX: section="FX"; break;
		case VT_EQ: section="EQ"; break;
		default: page=""; break;
	}
}

void View::DrawGraphics() {
	if (modalView_) {
		modalView_->DrawGraphics() ;
		return ;
	}
	if (contextOverlay_) {
		return ;
	}
	drawGraphics() ;
}

void View::DoModal(ModalView *view,ModalViewCallback cb) {
	CrashLog::Note("dialog open over view %d",viewType_) ;
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
	if (pressed) {
		CrashLog::Note("keys %04X view %d%s%s",mask,viewType_,modalView_?" dialog":"",
		               contextOverlay_?" helper":"") ;
	}
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
			if (mask == EPBM_A) {
				// Full guide at the page for this screen
				contextOverlay_ = false;
				const char *page = "";
				const char *section = "";
				GetGuideTopic(page, section);
				if (page) {  // 0: already the guide
					DoModal(new GuideDialog(*this, page, section));
				}
				((AppWindow &)w_).SetDirty();
				return;
			}
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

	// Undo / redo: B+Select steps back, LB+Select forward again
	// (no song loaded yet on the start screen: nothing to snapshot)
	bool haveSong=viewData_ && viewData_->project_ && viewData_->song_ ;
	if (haveSong && pressed && !modalView_ &&
	    (mask==(EPBM_B|EPBM_SELECT) || mask==(EPBM_L|EPBM_SELECT))) {
		bool undo=(mask&EPBM_B)!=0 ;
		const char *what=undo?UndoHistory::Undo(viewData_->project_,viewData_->song_)
		                     :UndoHistory::Redo(viewData_->project_,viewData_->song_) ;
		static char message[48] ;
		if (what) {
			snprintf(message,sizeof(message),"%s %s",undo?"Undone:":"Redone:",what) ;
		} else {
			snprintf(message,sizeof(message),"%s",undo?"Nothing to undo":"Nothing to redo") ;
		}
		CrashLog::Note("%s",message) ;
		SetNotification(message) ;
		undoGesture_=false ;
		isDirty_=true ;
		((AppWindow &)w_).SetDirty() ;
		return ;
	}
	if (!pressed && !(mask&EPBM_A)) {
		undoGesture_=false ;  // an A-hold of edits ends when A is let go
	}
	// Every press may change the song: keep what it was, in case it did
	UndoHistory::Snapshot before ;
	if (pressed && haveSong) {
		VariableContainer *instrument=viewData_->project_->GetInstrumentBank()
			->GetInstrument(viewData_->currentInstrument_) ;
		UndoHistory::Capture(viewData_->project_,viewData_->song_,instrument,
		                     ((AppWindow &)w_).GetCurrentViewName(),before) ;
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
			CrashLog::Note("dialog closed") ;
			isDirty_=true ;
			((AppWindow &)w_).InvalidateScreenCache() ;
		}
	} else {
		ProcessButtonMask(mask,pressed);
		if (pressed && (mask & EPBM_START)) {
			isDirty_=true;
		}
	}
	if (pressed && haveSong && viewData_->project_ && viewData_->song_) {
		// Holding A while editing is one step, however many values it moved
		// (the hold only joins steps once it has changed something itself)
		bool merge=undoGesture_ && (mask&EPBM_A) ;
		bool changed=UndoHistory::Record(viewData_->project_,viewData_->song_,before,merge) ;
		undoGesture_=(mask&EPBM_A) && (changed || merge) ;
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
