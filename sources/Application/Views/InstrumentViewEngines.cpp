// SOUND page of the synth engines FM4, HYPER and WAV (the ENV, FILTER, LFO,
// MOD and MIX pages are shared with the original synth). Each page draws
// what the engine does: FM4 its operator routing and the resulting wave,
// HYPER the swarm of detuned saws across the stereo field, WAV its shape.

#include "Application/AppWindow.h"
#include "InstrumentView.h"
#include "Application/Instruments/SynthInstrument.h"
#include "BaseClasses/UIIntVarField.h"
#include "Foundation/Variables/Variable.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

// FM4 grid: one column per operator
#define FM4_GRID_X 7
#define FM4_GRID_STEP 6
#define FM4_GRID_ROW 19
// HYPER: the six notes on two rows of three
#define HYPER_NOTE_X 7
#define HYPER_NOTE_STEP 5

static const char *fm4RowLabels[FM4_PARAM_COUNT]={
	"shape","ratio","level","fbk","atk","dec","sus"
};

static const char fm4RowParams[FM4_PARAM_COUNT]={
	FM4P_SHAPE,FM4P_RATIO,FM4P_LEVEL,FM4P_FEEDBACK,FM4P_ATTACK,FM4P_DECAY,FM4P_SUSTAIN
};

static int engineInt(I_Instrument *instr, FourCC id) {
	Variable *v=instr?instr->FindVariable(id):0;
	return v?v->GetInt():0;
}

static void engineSeconds(char *out, float seconds) {
	if (seconds<1.0f) {
		sprintf(out,"%d ms",(int)(seconds*1000.0f+0.5f));
	} else {
		sprintf(out,"%.1f s",seconds);
	}
}

// Which operator and which setting an FM4 id is (-1: not an FM4 knob)
static int fm4Which(FourCC id, int &param) {
	for (int op=0;op<FM4_OPS;op++) {
		for (int k=0;k<FM4_PARAM_COUNT;k++) {
			if ((FourCC)FM4_ID(op,fm4RowParams[k])==id) {
				param=k;
				return op;
			}
		}
	}
	return -1;
}

static int hyperNoteIndex(FourCC id) {
	for (int n=0;n<HYPER_NOTES;n++) {
		if ((FourCC)HYP_NOTE(n)==id) return n;
	}
	return -1;
}

/***************************************************************
 FM ratio field: "1.00". A+Left/Right 0.01, A+Up/Down the next
 whole ratio (0.25, 0.5, 1, 2, 3 ... 16)
 ***************************************************************/

class UIFm4RatioField: public UIIntVarField {
public:
	UIFm4RatioField(GUIPoint &position, Variable &v)
		:UIIntVarField(position,v,"%d",FM4_RATIO_MIN,FM4_RATIO_MAX,1,100) {}
	virtual void Draw(GUIWindow &w, int offset=0) {
		GUITextProperties props;
		GUIPoint position=GetPosition();
		position._y+=offset;
		if (focus_) {
			((AppWindow&)w).SetColor(CD_HILITE2);
			props.invert_=true;
		} else {
			((AppWindow&)w).SetColor(CD_NORMAL);
		}
		char buffer[16];
		int r=src_.GetInt();
		if (r>=1000) {
			sprintf(buffer,"%d.%d",r/100,(r%100)/10);
		} else {
			sprintf(buffer,"%d.%02d",r/100,r%100);
		}
		w.DrawString(buffer,position,props);
	}
	virtual void ProcessArrow(unsigned short mask) {
		int r=src_.GetInt();
		switch(mask) {
			case EPBM_LEFT: r-=1; break;
			case EPBM_RIGHT: r+=1; break;
			case EPBM_UP:
				if (r<50) r=50;
				else if (r<100) r=100;
				else r=(r/100+1)*100;
				break;
			case EPBM_DOWN:
				if (r>100) r=((r-1)/100)*100;
				else if (r>50) r=50;
				else r=25;
				break;
		}
		if (r<FM4_RATIO_MIN) r=FM4_RATIO_MIN;
		if (r>FM4_RATIO_MAX) r=FM4_RATIO_MAX;
		src_.SetInt(r);
	}
};

/***************************************************************
 Fields
 ***************************************************************/

void InstrumentView::fillEngineSoundPage(SynthInstrument *s, GUIPoint position, int engine) {
	UIIntVarField *f;
	Variable *v;

#define ENGINE_FIELD(id,fmt,mn,mx,x,y) \
	v=s->FindVariable(id); \
	f=new UIIntVarField(position,*v,fmt,mn,mx,x,y); \
	T_SimpleList<UIField>::Insert(f); \
	position._y+=1;

	// The grids move up/down in their own column
	nearestColumn_=true;

	switch(engine) {
		case SE_FM4: {
			v=s->FindVariable(FM4P_ALGO);
			f=new UIIntVarField(position,*v,"algo   %s",0,FM4_ALGO_COUNT-1,1,4);
			T_SimpleList<UIField>::Insert(f);
			GUIPoint tunePos(16,position._y);
			v=s->FindVariable(SYP_TUNE);
			f=new UIIntVarField(tunePos,*v,"tune %+d",-48,48,1,12);
			T_SimpleList<UIField>::Insert(f);
			// Row 18 is the A B C D header, drawn with the picture
			for (int k=0;k<FM4_PARAM_COUNT;k++) {
				for (int op=0;op<FM4_OPS;op++) {
					GUIPoint p(FM4_GRID_X+op*FM4_GRID_STEP,FM4_GRID_ROW+k);
					Variable *ov=s->FindVariable(FM4_ID(op,fm4RowParams[k]));
					if (fm4RowParams[k]==FM4P_SHAPE) {
						f=new UIIntVarField(p,*ov,"%s",0,F4S_LAST-1,1,1);
					} else if (fm4RowParams[k]==FM4P_RATIO) {
						f=new UIFm4RatioField(p,*ov);
					} else {
						f=new UIIntVarField(p,*ov,"%2.2X",0,0xFF,1,0x10);
					}
					T_SimpleList<UIField>::Insert(f);
				}
			}
			break;
		}
		case SE_HYPER: {
			ENGINE_FIELD(HYP_CHORD,"chord  %s",0,HYPER_CHORD_COUNT-1,1,1);
			for (int row=0;row<2;row++) {
				for (int k=0;k<3;k++) {
					int n=row*3+k;
					GUIPoint p(HYPER_NOTE_X+k*HYPER_NOTE_STEP,position._y);
					Variable *nv=s->FindVariable(HYP_NOTE(n));
					f=new UIIntVarField(p,*nv,"%+d",HYPER_NOTE_MIN,HYPER_NOTE_MAX,1,12);
					T_SimpleList<UIField>::Insert(f);
				}
				position._y+=1;
			}
			ENGINE_FIELD(HYP_SHIFT,"shift  %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(HYP_SWARM,"swarm  %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(HYP_WIDTH,"width  %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(HYP_SUB,"sub    %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(HYP_SCALE,"scale  %s",0,1,1,1);
			ENGINE_FIELD(SYP_TUNE,"tune   %+d",-48,48,1,12);
			break;
		}
		default: {
			ENGINE_FIELD(WVP_SHAPE,"wave   %s",0,WVS_LAST-1,1,1);
			ENGINE_FIELD(WVP_SIZE,"size   %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(WVP_MULT,"mult   %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(WVP_WARP,"warp   %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(WVP_MIRROR,"mirror %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(SYP_DRIVE,"drive  %2.2X",0,0xFF,1,0x10);
			ENGINE_FIELD(WVP_LIMIT,"limit  %s",0,WVL_LAST-1,1,1);
			ENGINE_FIELD(SYP_TUNE,"tune   %+d",-48,48,1,12);
			break;
		}
	}
#undef ENGINE_FIELD
}

// Rebuild the fields when the engine changed under them (the engine knob,
// a preset of another engine, undo)
void InstrumentView::syncSynthEngine() {
	if (getInstrumentType()!=IT_SYNTH || !current_ || shownEngine_<0) {
		return;
	}
	SynthInstrument *s=(SynthInstrument *)current_;
	if (s->GetEngine()==shownEngine_) {
		return;
	}
	UIIntVarField *focus=(UIIntVarField *)GetFocus();
	if (focus) {
		lastFocusID_=focus->GetVariableID();
	}
	onInstrumentChange();
	isDirty_=true;
}

const char *InstrumentView::getEngineGuideSection() {
	if (getInstrumentType()!=IT_SYNTH || labPage_!=0) {
		return "";
	}
	int i=viewData_->currentInstrument_;
	SynthInstrument *s=(SynthInstrument *)viewData_->project_->GetInstrumentBank()->GetInstrument(i);
	switch(s->GetEngine()) {
		case SE_FM4: return "FM4 engine";
		case SE_HYPER: return "HYPER engine";
		case SE_WAV: return "WAV engine";
		default: return "";
	}
}

/***************************************************************
 Help
 ***************************************************************/

static const char *fm4ShapeLong[F4S_LAST]={
	"sine","half sine","two humps","quarter sine","fast half","fast humps",
	"triangle","saw","square","pulse 25%","click","noise"
};

static const char *hyperIntervalNames[13]={
	"root","minor 2nd","2nd","minor 3rd","major 3rd","4th","tritone",
	"5th","minor 6th","6th","minor 7th","major 7th","octave"
};

bool InstrumentView::getEngineFieldHelp(FourCC id, I_Instrument *instr, char *line1,
                                        char *line2, char *value) {
	SynthInstrument *s=(SynthInstrument *)instr;
	int x=engineInt(instr,id);
	char buf[32];
	int param=0;
	int op=fm4Which(id,param);
	if (op>=0) {
		int algo=engineInt(instr,FM4P_ALGO);
		if (algo<0 || algo>=FM4_ALGO_COUNT) algo=0;
		bool carrier=(fm4Algos[algo].carriers_&(1<<op))!=0;
		switch(fm4RowParams[param]) {
			case FM4P_SHAPE:
				strcpy(line1,"the operator's wave");
				strcpy(line2,"sin = pure FM");
				sprintf(value,"%c: %s",'A'+op,fm4ShapeLong[(x>=0 && x<F4S_LAST)?x:0]);
				break;
			case FM4P_RATIO:
				strcpy(line1,"pitch x note: 1,2 warm");
				strcpy(line2,"3.5, 7, 1.41 = bells");
				sprintf(value,"%c: x%d.%02d",'A'+op,x/100,x%100);
				break;
			case FM4P_LEVEL:
				if (carrier) {
					strcpy(line1,"how loud it is heard");
					strcpy(line2,"(it goes to the output)");
					sprintf(value,"%c: heard %d%%",'A'+op,(int)(fm4LevelAmp(x)*100.0f+0.5f));
				} else {
					strcpy(line1,"how much it bends the");
					strcpy(line2,"op below: more = brighter");
					sprintf(value,"%c: FM %d%%",'A'+op,(int)(fm4LevelAmp(x)*100.0f+0.5f));
				}
				break;
			case FM4P_FEEDBACK:
				strcpy(line1,"modulates itself: sine to");
				strcpy(line2,"saw, then to noise");
				sprintf(value,"%c: fb %d%%",'A'+op,(x*100)/255);
				break;
			case FM4P_ATTACK:
				strcpy(line1,"time to reach its level");
				strcpy(line2,"slow on a modulator = swell");
				engineSeconds(buf,SynthInstrument::TimeFromParam(x));
				sprintf(value,"%c: %s",'A'+op,buf);
				break;
			case FM4P_DECAY:
				strcpy(line1,"time to fall to sustain");
				strcpy(line2,"short on a modulator = pluck");
				engineSeconds(buf,SynthInstrument::TimeFromParam(x));
				sprintf(value,"%c: %s",'A'+op,buf);
				break;
			default:
				strcpy(line1,"level it holds at");
				strcpy(line2,"FF = stays full");
				sprintf(value,"%c: %d%%",'A'+op,(x*100)/255);
				break;
		}
		return true;
	}
	int note=hyperNoteIndex(id);
	if (note>=0) {
		strcpy(line1,"chord note, semitones up");
		strcpy(line2,note<3?"1-3: heard at shift 00":"4-6: heard at shift FF");
		int semis[HYPER_NOTES];
		s->GetHyperNotes(60,semis);
		int iv=semis[note];
		int a=iv<0?-iv:iv;
		if (a<=12) {
			sprintf(value,"%s%s",iv<0?"-":"",hyperIntervalNames[a]);
		} else {
			sprintf(value,"%+d semitones",iv);
		}
		return true;
	}
	switch(id) {
		case FM4P_ALGO: {
			strcpy(line1,"who modulates whom: top");
			strcpy(line2,"ops bend the ones below");
			int a=(x>=0 && x<FM4_ALGO_COUNT)?x:0;
			strcpy(value,fm4Algos[a].formula_);
			return true;
		}
		case HYP_CHORD:
			strcpy(line1,"fills the six notes");
			strcpy(line2,"edit them: custom");
			return true;
		case HYP_SHIFT: {
			strcpy(line1,"fades notes 1-3 into 4-6");
			strcpy(line2,"00 = 1-3 only, FF = 4-6");
			float a,b;
			HyperShiftGains(x,a,b);
			sprintf(value,"%d%% / %d%%",(int)(a*100.0f+0.5f),(int)(b*100.0f+0.5f));
			return true;
		}
		case HYP_SWARM:
			strcpy(line1,"detunes each note's two");
			strcpy(line2,"saws: thicker, wider");
			sprintf(value,"%d cents",(int)(HyperDetuneCents(x,0)+0.5f));
			return true;
		case HYP_WIDTH:
			strcpy(line1,"spreads the saws left and");
			strcpy(line2,"right; 00 = mono");
			sprintf(value,"%d%%",(x*100)/255);
			return true;
		case HYP_SUB: {
			strcpy(line1,"square sub: to 7F two");
			strcpy(line2,"octaves down, 80+ one down");
			int oct;
			float level;
			HyperSub(x,oct,level);
			if (level<=0.0f) strcpy(value,"off");
			else sprintf(value,"-%d oct %d%%",oct,(int)(level*100.0f+0.5f));
			return true;
		}
		case HYP_SCALE:
			strcpy(line1,"on: notes move onto the");
			strcpy(line2,"song's Key/Scale");
			return true;
		case WVP_SHAPE:
			strcpy(line1,"base shape; tonenoise and");
			strcpy(line2,"noise = 8-bit noise");
			return true;
		case WVP_SIZE:
			strcpy(line1,"steps per cycle: low =");
			strcpy(line2,"crunchy, FF = smooth");
			sprintf(value,"%d steps",WavStepsFromSize(x));
			return true;
		case WVP_MULT:
			strcpy(line1,"repeats the shape in one");
			strcpy(line2,"cycle: a sync sound");
			sprintf(value,"x%.1f",WavMultFromParam(x));
			return true;
		case WVP_WARP:
			strcpy(line1,"squeezes the shape to one");
			strcpy(line2,"side, silence after");
			sprintf(value,"shape in %d%%",100-(x*90)/255);
			return true;
		case WVP_MIRROR:
			strcpy(line1,"moves the wave's middle:");
			strcpy(line2,"pulse50 = pulse width");
			sprintf(value,"middle %d%%",(int)((0.5f+(x-128)/127.0f*0.48f)*100.0f+0.5f));
			return true;
		case WVP_LIMIT:
			strcpy(line1,"what drive does when loud:");
			strcpy(line2,"soft/clip/sin/fold/wrap");
			return true;
		default:
			return false;
	}
}

void InstrumentView::customizeEngineOverlay(int engine, const char *&name, const char *&field,
                                            const char *&cmd1, const char *&cmd2,
                                            const char *&cmd3) {
	switch(engine) {
		case SE_FM4:
			name="FM4 SOUND";
			field="4 operators, 12 algos";
			cmd1="algo: who modulates whom";
			cmd2="Dpad: A B C D columns";
			cmd3="level: loud / FM depth";
			break;
		case SE_HYPER:
			name="HYPER SOUND";
			field="6-note detuned saw chord";
			cmd1="chord fills the 6 notes";
			cmd2="shift: notes 1-3 to 4-6";
			cmd3="swarm/width: thick, wide";
			break;
		default:
			name="WAV SOUND";
			field="8-bit shape bender";
			cmd1="wave, then bend it:";
			cmd2="size mult warp mirror";
			cmd3="drive+limit: fold/wrap";
			break;
	}
}

/***************************************************************
 Pictures
 ***************************************************************/

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)

GUIColor synthPanel();
GUIColor synthFrame();
GUIColor synthTrace();
GUIColor synthHot();
void synthPlot(SDLGUIWindowImp *imp, const int *ys, int count, int x, int top,
               int bottom, bool hot);

static void enginePixel(SDLGUIWindowImp *imp, int x, int y) {
	GUIRect r(x,y,x+1,y+1);
	imp->DrawRect(r);
}

static void engineLine(SDLGUIWindowImp *imp, int x0, int y0, int x1, int y1) {
	int dx=x1>x0?x1-x0:x0-x1;
	int dy=y1>y0?y1-y0:y0-y1;
	int sx=x0<x1?1:-1;
	int sy=y0<y1?1:-1;
	int err=dx-dy;
	for (int guard=0;guard<512;guard++) {
		enginePixel(imp,x0,y0);
		if (x0==x1 && y0==y1) break;
		int e2=2*err;
		if (e2>-dy) { err-=dy; x0+=sx; }
		if (e2<dx) { err+=dx; y0+=sy; }
	}
}

// 3x5 letters A..D for the operator boxes
static const unsigned char opGlyphs[4][5]={
	{2,5,7,5,5},
	{6,5,6,5,6},
	{3,4,4,4,3},
	{6,5,5,5,6},
};

static void opLetter(SDLGUIWindowImp *imp, int op, int x, int y) {
	for (int r=0;r<5;r++) {
		for (int c=0;c<3;c++) {
			if (opGlyphs[op][r]&(4>>c)) enginePixel(imp,x+c,y+r);
		}
	}
}

// The routing: operators as boxes (filled when they make sound), lines from
// each modulator down to what it modulates, carriers to the output bar. The
// operator under the cursor is drawn in the bright colour; a hook on a box
// means feedback.
static void drawFm4Algo(SDLGUIWindowImp *imp, SynthInstrument *s, int focusOp,
                        int left, int top, int width) {
	int algo=engineInt(s,FM4P_ALGO);
	if (algo<0 || algo>=FM4_ALGO_COUNT) algo=0;
	const Fm4Algo &a=fm4Algos[algo];
	const int boxW=11, boxH=9, colStep=12, rowStep=12;
	int maxCol=0, maxRow=0;
	for (int op=0;op<FM4_OPS;op++) {
		if (a.col_[op]>maxCol) maxCol=a.col_[op];
		if (a.row_[op]>maxRow) maxRow=a.row_[op];
	}
	int spanW=maxCol*colStep+boxW;
	int x0=left+(width-spanW)/2;
	// Boxes, then 4 px of wire, then the 2 px output bar, centred in 52 px
	int spanH=maxRow*rowStep+boxH+4+2;
	int y0=top+(52-spanH)/2;
	int outY=y0+maxRow*rowStep+boxH+4;
	int bx[FM4_OPS], by[FM4_OPS];
	for (int op=0;op<FM4_OPS;op++) {
		bx[op]=x0+a.col_[op]*colStep;
		by[op]=y0+a.row_[op]*rowStep;
	}
	GUIColor trace=synthTrace();
	GUIColor frame=synthFrame();
	GUIColor hot=synthHot();
	GUIColor panel=synthPanel();
	// Links first, so boxes sit on top of them
	imp->SetColor(trace);
	int outMin=1000, outMax=-1;
	for (int op=0;op<FM4_OPS;op++) {
		for (int t=0;t<FM4_OPS;t++) {
			if (a.mods_[op]&(1<<t)) {
				engineLine(imp,bx[op]+boxW/2,by[op]+boxH,bx[t]+boxW/2,by[t]-1);
			}
		}
		if (a.carriers_&(1<<op)) {
			int cx=bx[op]+boxW/2;
			engineLine(imp,cx,by[op]+boxH,cx,outY);
			if (cx<outMin) outMin=cx;
			if (cx>outMax) outMax=cx;
		}
	}
	if (outMax>=0) {
		GUIRect bus(outMin-2,outY,outMax+3,outY+2);
		imp->DrawRect(bus);
	}
	for (int op=0;op<FM4_OPS;op++) {
		bool on=engineInt(s,FM4_ID(op,FM4P_LEVEL))>0;
		GUIColor edge=(op==focusOp)?hot:(on?trace:frame);
		imp->SetColor(edge);
		GUIRect outer(bx[op],by[op],bx[op]+boxW,by[op]+boxH);
		imp->DrawRect(outer);
		if (!on) {
			imp->SetColor(panel);
			GUIRect inner(bx[op]+1,by[op]+1,bx[op]+boxW-1,by[op]+boxH-1);
			imp->DrawRect(inner);
			imp->SetColor(frame);
		} else {
			imp->SetColor(panel);
		}
		opLetter(imp,op,bx[op]+4,by[op]+2);
		if (engineInt(s,FM4_ID(op,FM4P_FEEDBACK))>0) {
			// feedback hook on the right edge
			imp->SetColor(edge);
			int rx=bx[op]+boxW;
			engineLine(imp,rx,by[op]+2,rx+2,by[op]+2);
			engineLine(imp,rx+2,by[op]+2,rx+2,by[op]+boxH-3);
			engineLine(imp,rx,by[op]+boxH-3,rx+2,by[op]+boxH-3);
		}
	}
}

// Each chord note is a lane (its interval on the left, drawn as text).
// Left part: the stereo field; the note's two saws are dots, pushed apart
// by width. Right part: one second of how the pair beats against itself
// (swarm): flat = in tune, more bumps = faster shimmer. Lanes faded out by
// shift are dim.
static void drawHyperSwarm(SDLGUIWindowImp *imp, SynthInstrument *s,
                           int left, int split, int right) {
	int semis[HYPER_NOTES];
	s->GetHyperNotes(60,semis);
	float first,second;
	HyperShiftGains(engineInt(s,HYP_SHIFT),first,second);
	float width=engineInt(s,HYP_WIDTH)/255.0f;
	int swarm=engineInt(s,HYP_SWARM);
	int centre=(left+split)/2;
	int reach=(split-left)/2-3;
	GUIColor trace=synthTrace();
	GUIColor frame=synthFrame();
	GUIColor hot=synthHot();
	imp->SetColor(frame);
	// L | R field with its mono centre line, and the divider
	GUIRect mid(centre,40,centre+1,88);
	imp->DrawRect(mid);
	GUIRect divider(split+3,40,split+4,88);
	imp->DrawRect(divider);
	for (int n=0;n<HYPER_NOTES;n++) {
		int laneY=40+n*8+3;
		float g=(n<3)?first:second;
		GUIColor ink=g>0.6f?hot:(g>0.05f?trace:frame);
		imp->SetColor(frame);
		GUIRect lane(left,laneY+1,split,laneY+2);
		imp->DrawRect(lane);
		imp->SetColor(ink);
		int dx=(int)(width*reach+0.5f);
		GUIRect a(centre-dx-2,laneY-1,centre-dx+2,laneY+3);
		GUIRect b(centre+dx-1,laneY-1,centre+dx+3,laneY+3);
		imp->DrawRect(a);
		imp->DrawRect(b);
		// Beat of the pair on C4: |cos(pi * df * t)| over one second
		float hz=261.63f*(float)pow(2.0,semis[n]/12.0);
		float df=hz*((float)pow(2.0,HyperDetuneCents(swarm,n)/1200.0)-1.0f);
		int span=right-(split+8);
		int prev=-1;
		for (int x=0;x<span;x++) {
			float t=(float)x/(float)span;
			float e=(float)fabs(cos(3.14159265f*df*t));
			int y=laneY+2-(int)(e*5.0f+0.5f);
			int a0=prev<0?y:(prev<y?prev:y);
			int b0=prev<0?y:(prev>y?prev:y);
			GUIRect r(split+8+x,a0,split+9+x,b0+1);
			imp->DrawRect(r);
			prev=y;
		}
	}
}

void InstrumentView::drawEngineSoundVisual(SynthInstrument *s, int bx, int by, int bw, int bh) {
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUITextProperties props;
	const int top=by+3, bottom=by+bh-4;
	const int mid=(top+bottom)/2;
	int ys[240];
	float wave[240];
	int engine=s->GetEngine();

	if (engine==SE_FM4) {
		// Operator under the cursor (grid column), for the bright box
		int focusOp=-1;
		UIIntVarField *focused=(UIIntVarField *)GetFocus();
		if (focused) {
			int param;
			focusOp=fm4Which(focused->GetVariableID(),param);
		}
		drawFm4Algo(imp,s,focusOp,bx+2,by+2,96);
		// divider, then two cycles of the sound
		GUIColor frame=synthFrame();
		imp->SetColor(frame);
		GUIRect divider(bx+100,by+4,bx+101,by+bh-4);
		imp->DrawRect(divider);
		const int wx=bx+104, ww=bw-108;
		s->RenderPreview(wave,ww,2.0f);
		for (int x=0;x<ww;x++) {
			ys[x]=mid-(int)(wave[x]*(bottom-top)/2*0.9f);
		}
		synthPlot(imp,ys,ww,wx,top,bottom,false);
		// A B C D over the grid columns (the cursor's column bright)
		SetColor(CD_MUTE);
		DrawString(1,FM4_GRID_ROW-1,"op",props);
		for (int op=0;op<FM4_OPS;op++) {
			char letter[2]={(char)('A'+op),0};
			SetColor(op==focusOp?CD_HILITE1:CD_NORMAL);
			DrawString(FM4_GRID_X+1+op*FM4_GRID_STEP,FM4_GRID_ROW-1,letter,props);
		}
		SetColor(CD_NORMAL);
		for (int k=0;k<FM4_PARAM_COUNT;k++) {
			DrawString(1,FM4_GRID_ROW+k,fm4RowLabels[k],props);
		}
	} else if (engine==SE_HYPER) {
		drawHyperSwarm(imp,s,bx+36,bx+124,bx+bw-4);
		// intervals as they sound on C (after the scale)
		int semis[HYPER_NOTES];
		s->GetHyperNotes(60,semis);
		float first,second;
		HyperShiftGains(engineInt(s,HYP_SHIFT),first,second);
		for (int n=0;n<HYPER_NOTES;n++) {
			char label[8];
			sprintf(label,"%+d",semis[n]);
			float g=(n<3)?first:second;
			SetColor(g>0.05f?CD_NORMAL:CD_MUTE);
			DrawString(2,5+n,label,props);
		}
		SetColor(CD_NORMAL);
		DrawString(1,18,"1-3",props);
		DrawString(1,19,"4-6",props);
	} else {
		const int plotW=bw-4;
		s->RenderPreview(wave,plotW,2.0f);
		for (int x=0;x<plotW;x++) {
			ys[x]=mid-(int)(wave[x]*(bottom-top)/2*0.9f);
		}
		synthPlot(imp,ys,plotW,bx+2,top,bottom,false);
	}
}

#else

void InstrumentView::drawEngineSoundVisual(SynthInstrument *s, int bx, int by, int bw, int bh) {
}

#endif
