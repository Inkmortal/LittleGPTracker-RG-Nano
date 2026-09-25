// MOD page of the Instrument screen, shared by synth and sample instruments:
// four slots like the M8's instrument modulation view. The top shows all
// four at a glance (type, destination, amount and a tiny curve each), the
// middle draws the chosen slot's curve over real time, and the fields below
// edit that one slot, each with its value in real units (ms, Hz, %).
// LB+Up/Down (or the "slot" field) picks the slot.

#include "Application/AppWindow.h"
#include "InstrumentView.h"
#include "Application/Instruments/ModSources.h"
#include "Application/Utils/char.h"
#include "BaseClasses/UIIntVarField.h"
#include "Foundation/Variables/Variable.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MOD_FIELD_ROW 15
#define MOD_AMOUNT_KIND (-1)

static InstrumentMods *modsOf(I_Instrument *instr) {
	return instr?instr->GetMods():0;
}

// Which slot and which setting an id belongs to: 0 type, 1 dest, 2 amount,
// 3.. parameters (-1 if none)
static int modSlotOf(FourCC id, int &which) {
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		if (id==MOD_TYPE_ID(s)) { which=0; return s; }
		if (id==MOD_DEST_ID(s)) { which=1; return s; }
		if (id==MOD_AMOUNT_ID(s)) { which=2; return s; }
		for (int p=0;p<MOD_PARAM_COUNT;p++) {
			if (id==MOD_PARAM_ID(s,p)) { which=3+p; return s; }
		}
	}
	return -1;
}

static void formatModTime(char *out, float seconds) {
	if (seconds<=0.0f) {
		strcpy(out,"0 ms");
	} else if (seconds<0.01f) {
		sprintf(out,"%.1f ms",seconds*1000.0f);
	} else if (seconds<1.0f) {
		sprintf(out,"%d ms",(int)(seconds*1000.0f+0.5f));
	} else if (seconds<10.0f) {
		sprintf(out,"%.2f s",seconds);
	} else {
		sprintf(out,"%.1f s",seconds);
	}
}

static void formatModHz(char *out, float hz) {
	if (hz<10.0f) {
		sprintf(out,"%.2f Hz",hz);
	} else {
		sprintf(out,"%.1f Hz",hz);
	}
}

// A parameter's value as shown on screen: hex, then real units
static void describeModParam(int kind, int value, char *out) {
	char unit[24];
	switch(kind) {
		case MPK_TIME:
			formatModTime(unit,ModSource::TimeFromParam(value));
			sprintf(out,"%2.2X %s",value,unit);
			break;
		case MPK_LEVEL:
			sprintf(out,"%2.2X %d%%",value,(value*100+127)/255);
			break;
		case MPK_RATE:
			formatModHz(unit,ModSource::RateFromParam(value));
			sprintf(out,"%2.2X %s",value,unit);
			break;
		case MPK_SHAPE:
			strcpy(out,InstrumentMods::ShapeName(value));
			break;
		case MPK_TRIGMODE:
			strcpy(out,InstrumentMods::TrigName(value));
			break;
		case MPK_PEAK:
			sprintf(out,"%2.2X %dms dip %d%%",value,
			        (int)(ModSource::PeakTime(value)*1000.0f+0.5f),
			        (int)(ModSource::PeakDip(value)*100.0f+0.5f));
			break;
		case MPK_TRACKSRC:
			sprintf(out,"track %d",(value&7)+1);
			break;
		case MPK_NOTE:
			note2char((unsigned char)(value<0?0:(value>127?127:value)),out);
			out[4]=0;
			break;
		case MPK_SIGNED:
			sprintf(out,"%+d",value);
			break;
		case MPK_LEGACYENV:
			formatModTime(unit,ModSource::LegacyEnvTime(value));
			sprintf(out,"%2.2X %s",value,unit);
			break;
		default:
			out[0]=0;
			break;
	}
}

// The amount with what it means for the slot's destination
static void describeModAmount(const ModSlotSettings &s, char *out) {
	int a=s.amount_;
	switch(s.dest_) {
		case MD_PITCH:
			sprintf(out,"%+d %+.1f semi",a,a*MOD_PITCH_RANGE/MOD_AMOUNT_MAX);
			break;
		case MD_FINE:
			sprintf(out,"%+d %+d cents",a,(int)(a*100.0f*MOD_FINE_RANGE/MOD_AMOUNT_MAX));
			break;
		default:
			sprintf(out,"%+d %+d%%",a,(a*100)/MOD_AMOUNT_MAX);
			break;
	}
}

// A mod setting whose value reads in real units next to its hex
class UIModField: public UIIntVarField {
public:
	UIModField(GUIPoint &position, Variable &v, const char *label, int kind,
	           int min, int max, int bigStep, InstrumentMods *mods, int slot)
		:UIIntVarField(position,v,label,min,max,1,bigStep),
		 label_(label),kind_(kind),mods_(mods),slot_(slot) {}
	virtual void Draw(GUIWindow &w, int offset=0) {
		GUITextProperties props;
		GUIPoint position=GetPosition();
		position._y+=offset;
		if (focus_) {
			((AppWindow &)w).SetColor(CD_HILITE2);
			props.invert_=true;
		} else {
			((AppWindow &)w).SetColor(CD_NORMAL);
		}
		char value[40];
		if (kind_==MOD_AMOUNT_KIND) {
			ModSlotSettings s;
			mods_->GetSlot(slot_,s);
			describeModAmount(s,value);
		} else {
			describeModParam(kind_,src_.GetInt(),value);
		}
		char buffer[48];
		sprintf(buffer,"%-7s%s",label_,value);
		buffer[29]=0;
		w.DrawString(buffer,position,props);
	}
private:
	const char *label_;
	int kind_;
	InstrumentMods *mods_;
	int slot_;
};

int InstrumentView::currentModType() {
	I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(viewData_->currentInstrument_);
	InstrumentMods *mods=modsOf(instr);
	if (!mods) return MT_OFF;
	int slot=modSlotVar_->GetInt()-1;
	if (slot<0 || slot>=MOD_SLOT_COUNT) slot=0;
	return mods->TypeVar(slot)->GetInt();
}

bool InstrumentView::isModField(FourCC id) {
	int which;
	return id==INSTRUMENT_MOD_SLOT_FIELD || modSlotOf(id,which)>=0;
}

void InstrumentView::fillModPage(I_Instrument *instr, GUIPoint position) {
	InstrumentMods *mods=modsOf(instr);
	if (!mods) return;
	int slot=modSlotVar_->GetInt()-1;
	if (slot<0 || slot>=MOD_SLOT_COUNT) {
		slot=0;
		modSlotVar_->SetInt(1,false);
	}
	position._x=1;
	position._y=MOD_FIELD_ROW;
	UIIntVarField *f=new UIIntVarField(position,*modSlotVar_,"slot   %d of 4",1,MOD_SLOT_COUNT,1,1);
	T_SimpleList<UIField>::Insert(f);
	position._y+=1;
	f=new UIIntVarField(position,*mods->TypeVar(slot),"type   %s",0,MT_LAST-1,1,1);
	T_SimpleList<UIField>::Insert(f);
	position._y+=1;
	int type=mods->TypeVar(slot)->GetInt();
	modFieldsType_=type;
	modFieldsSlot_=slot;
	if (type==MT_OFF) {
		return;
	}
	f=new UIIntVarField(position,*mods->DestVar(slot),"dest   %s",0,mods->DestCount()-1,1,1);
	T_SimpleList<UIField>::Insert(f);
	position._y+=1;
	if (type!=MT_TRACK) {
		f=new UIModField(position,*mods->AmountVar(slot),"amount",MOD_AMOUNT_KIND,
		                 -MOD_AMOUNT_MAX,MOD_AMOUNT_MAX,0x10,mods,slot);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
	}
	for (int p=0;p<InstrumentMods::ParamCount(type);p++) {
		const ModParamDef *d=InstrumentMods::Param(type,p);
		if (!d) continue;
		f=new UIModField(position,*mods->ParamVar(slot,p),d->label_,d->kind_,
		                 d->min_,d->max_,d->bigStep_,mods,slot);
		T_SimpleList<UIField>::Insert(f);
		position._y+=1;
	}
}

// Shows another slot: the fields are rebuilt for it, the cursor stays on
// the same kind of setting when the new slot has it
void InstrumentView::selectModSlot(int slot) {
	if (slot<0) slot=MOD_SLOT_COUNT-1;
	if (slot>=MOD_SLOT_COUNT) slot=0;
	UIIntVarField *focus=(UIIntVarField *)GetFocus();
	int which=-1;
	if (focus) {
		FourCC id=focus->GetVariableID();
		if (id==INSTRUMENT_MOD_SLOT_FIELD) {
			which=-2;
		} else {
			modSlotOf(id,which);
		}
	}
	modSlotVar_->SetInt(slot+1,false);
	if (which==-2) {
		lastFocusID_=INSTRUMENT_MOD_SLOT_FIELD;
	} else if (which==0) {
		lastFocusID_=MOD_TYPE_ID(slot);
	} else if (which==1) {
		lastFocusID_=MOD_DEST_ID(slot);
	} else if (which==2) {
		lastFocusID_=MOD_AMOUNT_ID(slot);
	} else if (which>=3) {
		lastFocusID_=MOD_PARAM_ID(slot,which-3);
	} else {
		lastFocusID_=MOD_TYPE_ID(slot);
	}
	onInstrumentChange();
	isDirty_=true;
}

// LB+Up/Down: previous / next slot
bool InstrumentView::processModKeys(unsigned short mask) {
	if (labPage_!=INSTRUMENT_MOD_PAGE) return false;
	if (getInstrumentType()!=IT_SYNTH && getInstrumentType()!=IT_MACRO && getInstrumentType()!=IT_SAMPLE) return false;
	if (!(mask&EPBM_L) || (mask&(EPBM_A|EPBM_B|EPBM_R|EPBM_START|EPBM_SELECT))) return false;
	int slot=modSlotVar_->GetInt()-1;
	if (mask&EPBM_UP) {
		selectModSlot(slot-1);
		return true;
	}
	if (mask&EPBM_DOWN) {
		selectModSlot(slot+1);
		return true;
	}
	return false;
}

// After an edit on the MOD page: another slot picked on the slot field, or
// a new type (which loads that type's starting values and its own fields)
bool InstrumentView::syncModPage(int instrumentBefore, int slotBefore, int typeBefore) {
	if (labPage_!=INSTRUMENT_MOD_PAGE) return false;
	// Another instrument (B+Left/Right): its slots are its own
	if (viewData_->currentInstrument_!=instrumentBefore) return false;
	I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(viewData_->currentInstrument_);
	InstrumentMods *mods=modsOf(instr);
	if (!mods) return false;
	int slot=modSlotVar_->GetInt()-1;
	if (slot!=slotBefore) {
		lastFocusID_=INSTRUMENT_MOD_SLOT_FIELD;
		onInstrumentChange();
		isDirty_=true;
		return true;
	}
	if (slot<0 || slot>=MOD_SLOT_COUNT) return false;
	int type=mods->TypeVar(slot)->GetInt();
	if (type!=typeBefore) {
		mods->ApplyTypeDefaults(slot);
		lastFocusID_=MOD_TYPE_ID(slot);
		onInstrumentChange();
		isDirty_=true;
		return true;
	}
	return false;
}

// B+A on a MOD setting: back to its default. A parameter goes back to the
// starting value of the slot's type; the type itself to off.
bool InstrumentView::resetModField() {
	if (labPage_!=INSTRUMENT_MOD_PAGE) return false;
	UIIntVarField *focus=(UIIntVarField *)GetFocus();
	if (!focus) return false;
	FourCC id=focus->GetVariableID();
	if (!isModField(id)) return false;
	I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(viewData_->currentInstrument_);
	InstrumentMods *mods=modsOf(instr);
	if (!mods) return false;
	if (id==INSTRUMENT_MOD_SLOT_FIELD) {
		selectModSlot(0);
		return true;
	}
	int which=0;
	int slot=modSlotOf(id,which);
	if (which==0) {
		int before=mods->TypeVar(slot)->GetInt();
		mods->TypeVar(slot)->ResetToDefault();
		syncModPage(viewData_->currentInstrument_,slot,before);
	} else if (which>=3) {
		const ModParamDef *d=InstrumentMods::Param(mods->TypeVar(slot)->GetInt(),which-3);
		focus->GetVariable().SetInt(d?d->default_:0);
	} else {
		focus->GetVariable().ResetToDefault();
	}
	isDirty_=true;
	return true;
}

// Undo/redo can swap a slot's type under the page: rebuild its fields
void InstrumentView::refreshStaleModFields() {
	if (labPage_!=INSTRUMENT_MOD_PAGE) return;
	if (getInstrumentType()!=IT_SYNTH && getInstrumentType()!=IT_MACRO && getInstrumentType()!=IT_SAMPLE) return;
	int slot=modSlotVar_->GetInt()-1;
	if (slot==modFieldsSlot_ && currentModType()==modFieldsType_) return;
	UIIntVarField *focus=(UIIntVarField *)GetFocus();
	if (focus) lastFocusID_=focus->GetVariableID();
	onInstrumentChange();
}

// Explanations of the focused setting, and its value in real units
void InstrumentView::getModFieldHelp(FourCC id, I_Instrument *instr, char *line1,
                                     char *line2, char *value) {
	line1[0]=line2[0]=value[0]=0;
	InstrumentMods *mods=modsOf(instr);
	if (!mods) return;
	if (id==INSTRUMENT_MOD_SLOT_FIELD) {
		strcpy(line1,"4 slots, each moves one");
		strcpy(line2,"thing. LB+Up/Down: next");
		sprintf(value,"slot %d",modSlotVar_->GetInt());
		return;
	}
	int which=0;
	int slot=modSlotOf(id,which);
	if (slot<0) return;
	ModSlotSettings s;
	mods->GetSlot(slot,s);
	if (which==0) {
		switch(s.type_) {
			case MT_AHD: strcpy(line1,"ahd: attack, hold, decay"); strcpy(line2,"on every note"); break;
			case MT_ADSR: strcpy(line1,"adsr: holds at sustain,"); strcpy(line2,"releases after KILL"); break;
			case MT_DRUM: strcpy(line1,"drum: sharp peak, a dip,"); strcpy(line2,"then body and decay"); break;
			case MT_LFO: strcpy(line1,"lfo: repeating wobble"); strcpy(line2,"10 shapes, free or retrig"); break;
			case MT_TRIG: strcpy(line1,"trig: ahd fired by notes"); strcpy(line2,"on another track"); break;
			case MT_TRACK: strcpy(line1,"track: the note picks the"); strcpy(line2,"value, low to high notes"); break;
			case MT_DECAY: strcpy(line1,"decay: full, then falls"); strcpy(line2,"(first release, kept)"); break;
			case MT_SWELL: strcpy(line1,"swell: rises, then holds"); strcpy(line2,"(first release, kept)"); break;
			default: strcpy(line1,"pick ahd, adsr, drum, lfo,"); strcpy(line2,"trig or track"); break;
		}
		return;
	}
	if (which==1) {
		strcpy(line1,"what this slot moves");
		if (s.dest_==MD_VOLUME && ModSource::IsEnvelope(s.type_)) {
			strcpy(line2,"envelope = the note's shape");
		} else if (s.dest_==MD_START || s.dest_==MD_LOOP) {
			strcpy(line2,"100% = the whole trim");
		} else if (s.dest_==MD_FINE) {
			strcpy(line2,"100% = one semitone");
		} else if (s.dest_==MD_PITCH) {
			strcpy(line2,"100% = two octaves");
		} else {
			strcpy(line2,"cutoff = filter sweep");
		}
		return;
	}
	if (which==2) {
		strcpy(line1,"how far it moves");
		strcpy(line2,"minus = the other way");
		describeModAmount(s,value);
		return;
	}
	const ModParamDef *d=InstrumentMods::Param(s.type_,which-3);
	if (!d) return;
	describeModParam(d->kind_,s.param_[which-3],value);
	switch(d->kind_) {
		case MPK_TIME:
			if (!strcmp(d->label_,"attack")) strcpy(line1,"time to reach full");
			else if (!strcmp(d->label_,"hold")) strcpy(line1,"time it stays at full");
			else if (!strcmp(d->label_,"body")) strcpy(line1,"time the body holds");
			else if (!strcmp(d->label_,"releas")) strcpy(line1,"fade after KILL / new instr");
			else strcpy(line1,"time to fall away");
			strcpy(line2,"00 = instant");
			break;
		case MPK_LEVEL: strcpy(line1,"level while the note holds"); strcpy(line2,"00 = pluck, FF = organ"); break;
		case MPK_RATE: strcpy(line1,"wobbles per second"); strcpy(line2,"higher = faster"); break;
		case MPK_SHAPE: strcpy(line1,"tri sine ramps exp squares"); strcpy(line2,"random = new level each cycle"); break;
		case MPK_TRIGMODE: strcpy(line1,"free: runs on, retrig: new"); strcpy(line2,"note restarts, hold/once 1x"); break;
		case MPK_PEAK: strcpy(line1,"transient: dip depth and"); strcpy(line2,"time, 00 = no dip"); break;
		case MPK_TRACKSRC: strcpy(line1,"fires when this track"); strcpy(line2,"plays a note"); break;
		case MPK_NOTE: strcpy(line1,"from: note that gets low"); strcpy(line2,"to: note that gets high"); break;
		case MPK_SIGNED: strcpy(line1,"value at that note"); strcpy(line2,"notes between: in between"); break;
		case MPK_LEGACYENV: strcpy(line1,"higher = faster"); strcpy(line2,"envelope length"); break;
		default: break;
	}
}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
// Time window that shows a slot's whole curve, and where its note-off is
static float modWindow(const ModSlotSettings &s, float &gate) {
	gate=-1.0f;
	float t=0.0f;
	switch(s.type_) {
		case MT_AHD:
		case MT_TRIG:
			t=ModSource::TimeFromParam(s.param_[0])+ModSource::TimeFromParam(s.param_[1])+
			  ModSource::TimeFromParam(s.param_[2]);
			return t*1.15f+0.005f;
		case MT_ADSR: {
			float ad=ModSource::TimeFromParam(s.param_[0])+ModSource::TimeFromParam(s.param_[1]);
			float r=ModSource::TimeFromParam(s.param_[3]);
			float hold=(ad+r)*0.3f;
			if (hold<0.02f) hold=0.02f;
			gate=ad+hold;
			return (gate+r)*1.1f+0.005f;
		}
		case MT_DRUM:
			t=2.0f*ModSource::PeakTime(s.param_[0])+ModSource::TimeFromParam(s.param_[1])+
			  ModSource::TimeFromParam(s.param_[2]);
			return t*1.15f+0.005f;
		case MT_LFO: {
			float hz=ModSource::RateFromParam(s.param_[0]);
			float cycles=(s.param_[2]==MLT_HOLD || s.param_[2]==MLT_ONCE)?1.5f:2.0f;
			if (s.param_[2]!=MLT_HOLD && s.param_[2]!=MLT_ONCE && hz>2.0f) cycles=hz<8.0f?hz:8.0f;
			return cycles/hz;
		}
		case MT_DECAY:
			return ModSource::LegacyEnvTime(s.param_[0])*1.2f+0.005f;
		case MT_SWELL:
			return ModSource::LegacyEnvTime(s.param_[0])*1.5f+0.005f;
		default:
			return 1.0f;
	}
}
#endif

// One slot's curve inside a box: envelopes rise from the bottom (from the
// top when the amount is negative), LFOs and key tracking swing around the
// middle. 'big' adds the frame, the centre line and the note-off marker.
void InstrumentView::drawModCurve(I_Instrument *instr, int slot, int x, int y,
                                  int w, int h, bool big) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	InstrumentMods *mods=modsOf(instr);
	if (!mods || w<4 || h<3) return;
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	ModSlotSettings s;
	mods->GetSlot(slot,s);
	int selected=modSlotVar_->GetInt()-1;
	int top=y, bottom=y+h-1, left=x, width=w;
	if (big) {
		imp->SetColor(frame);
		GUIRect outer(x,y,x+w,y+h);
		imp->DrawRect(outer);
		imp->SetColor(panel);
		GUIRect inner(x+1,y+1,x+w-1,y+h-1);
		imp->DrawRect(inner);
		top=y+3;
		bottom=y+h-4;
		left=x+2;
		width=w-4;
	}
	if (s.type_==MT_OFF) {
		// a flat dotted line: nothing moves
		imp->SetColor(frame);
		int mid=(top+bottom)/2;
		for (int i=0;i<width;i+=4) {
			GUIRect dot(left+i,mid,left+i+2,mid+1);
			imp->DrawRect(dot);
		}
		return;
	}
	bool bipolar=(s.type_==MT_LFO || s.type_==MT_TRACK);
	bool flip=(s.amount_<0 && s.type_!=MT_TRACK);
	int mid=(top+bottom)/2;
	float levels[240];
	int count=width>240?240:width;
	float gate=-1.0f;
	float window=1.0f;
	if (s.type_==MT_TRACK) {
		for (int i=0;i<count;i++) {
			levels[i]=ModSource::TrackLevel(s,(i*127)/(count>1?count-1:1));
		}
	} else {
		window=modWindow(s,gate);
		ModSource::Preview(s,window,gate,60,levels,count);
	}
	if (big) {
		// centre line for swinging curves, baseline for envelopes
		imp->SetColor(frame);
		int line=bipolar?mid:(flip?top:bottom);
		for (int i=0;i<width;i+=6) {
			GUIRect dot(left+i,line,left+i+2,line+1);
			imp->DrawRect(dot);
		}
		if (gate>0.0f && window>0.0f) {
			// note-off: dotted vertical line
			int gx=left+(int)(gate/window*(count-1));
			for (int j=top;j<=bottom;j+=3) {
				GUIRect dot(gx,j,gx+1,j+2);
				imp->DrawRect(dot);
			}
		}
		if (s.type_==MT_TRACK) {
			// the from / to notes
			int fx=left+(s.param_[0]*(count-1))/127;
			int tx=left+(s.param_[1]*(count-1))/127;
			for (int j=top;j<=bottom;j+=3) {
				GUIRect a(fx,j,fx+1,j+2);
				imp->DrawRect(a);
				GUIRect b(tx,j,tx+1,j+2);
				imp->DrawRect(b);
			}
		}
	}
	GUIColor color=(slot==selected)?AppWindow::ThemeColor(CD_HILITE2):
	               AppWindow::ThemeBlend(CD_BACKGROUND,CD_NORMAL,70);
	imp->SetColor(color);
	int span=bottom-top;
	int prev=0;
	for (int i=0;i<count;i++) {
		float v=levels[i];
		if (flip) v=-v;
		int py;
		if (bipolar) {
			py=mid-(int)(v*span/2.0f);
		} else if (flip) {
			py=top-(int)(v*span);     // v is -level: down from the top
		} else {
			py=bottom-(int)(v*span);
		}
		if (py<top) py=top;
		if (py>bottom) py=bottom;
		int a=(i==0)?py:(py<prev?py:prev);
		int b=(i==0)?py:(py>prev?py:prev);
		GUIRect r(left+i,a,left+i+1,b+1);
		imp->DrawRect(r);
		prev=py;
	}
#endif
}

void InstrumentView::drawModPage(I_Instrument *instr) {
	InstrumentMods *mods=modsOf(instr);
	if (!mods) return;
	GUITextProperties props;
	char line[48];
	int selected=modSlotVar_->GetInt()-1;
	if (selected<0 || selected>=MOD_SLOT_COUNT) selected=0;
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	imp->SetColor(panel);
	GUIRect clearPanel(0,32,240,120);
	imp->DrawRect(clearPanel);
#endif
	// All four slots: type, what it moves, how far, a tiny curve
	for (int s=0;s<MOD_SLOT_COUNT;s++) {
		ModSlotSettings m;
		mods->GetSlot(s,m);
		char amount[8];
		if (m.type_==MT_OFF) {
			amount[0]=0;
		} else if (m.type_==MT_TRACK) {
			strcpy(amount,"key");
		} else {
			sprintf(amount,"%+d",m.amount_);
		}
		if (m.type_==MT_OFF) {
			sprintf(line,"%c%d off",s==selected?'>':' ',s+1);
		} else {
			sprintf(line,"%c%d %-5s %-7s %s",s==selected?'>':' ',s+1,
			        InstrumentMods::TypeName(m.type_),InstrumentMods::DestName(m.dest_),amount);
		}
		line[21]=0;
		SetColor(s==selected?CD_HILITE1:(m.type_==MT_OFF?CD_MUTE:CD_NORMAL));
		DrawString(0,4+s,line,props);
		drawModCurve(instr,s,172,(4+s)*8+1,64,6,false);
	}
	SetColor(CD_NORMAL);

	// The chosen slot, big, over real time
	drawModCurve(instr,selected,8,66,224,44,true);
	ModSlotSettings m;
	mods->GetSlot(selected,m);
	char span[24];
	line[0]=0;
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	float gate=-1.0f;
	float window=modWindow(m,gate);
	formatModTime(span,window);
	switch(m.type_) {
		case MT_OFF:
			sprintf(line,"slot %d is off: pick a type",selected+1);
			break;
		case MT_ADSR:
			sprintf(line,"0 .. %s  : = note off",span);
			break;
		case MT_LFO: {
			char hz[16];
			formatModHz(hz,ModSource::RateFromParam(m.param_[0]));
			sprintf(line,"%s %s  0 .. %s",hz,InstrumentMods::TrigName(m.param_[2]),span);
			break;
		}
		case MT_TRIG:
			sprintf(line,"on track %d notes  0..%s",(m.param_[3]&7)+1,span);
			break;
		case MT_TRACK:
		{
			char lo[8],hi[8];
			note2char(0,lo);
			lo[4]=0;
			note2char(127,hi);
			hi[4]=0;
			sprintf(line,"all notes %s .. %s",lo,hi);
		}
			break;
		default:
			sprintf(line,"note start .. %s",span);
			break;
	}
#endif
	line[29]=0;
	SetColor(m.type_==MT_OFF?CD_HILITE1:CD_MUTE);
	DrawString((30-(int)strlen(line))/2,14,line,props);
	SetColor(CD_NORMAL);
}

void InstrumentView::customizeModOverlay(const char *&field, const char *&where,
                                         const char *&edit, const char *&cmd1,
                                         const char *&cmd2, const char *&cmd3,
                                         const char *&cmd4, const char *&cmd5,
                                         const char *&cmd6, const char *&cmd7) {
	field="4 slots: envelope/LFO/key";
	cmd1="LB+Up/Down other slot";
	cmd2="type ahd adsr drum lfo..";
	cmd3="dest: what it moves";
	cmd4="amount: how far (+/-)";
	cmd5="LB+Left/Right page";
	cmd6="A+Start hear  B+A reset";
	cmd7="B+Left/Right other instr";
	UIIntVarField *focused=(UIIntVarField *)GetFocus();
	if (focused && isModField(focused->GetVariableID())) {
		static char help1[40],help2[40],value[40];
		I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(viewData_->currentInstrument_);
		getModFieldHelp(focused->GetVariableID(),instr,help1,help2,value);
		if (help1[0]) {
			where=help1;
			edit=help2;
		}
	}
}

void InstrumentView::CustomizeHowToSteps(const char **lines) {
	if (labPage_!=INSTRUMENT_MOD_PAGE) return;
	if (getInstrumentType()!=IT_SYNTH && getInstrumentType()!=IT_MACRO && getInstrumentType()!=IT_SAMPLE) return;
	lines[0]="MOD: 4 slots move the sound";
	lines[1]="on every note by themselves";
	lines[2]="1 type: adsr, lfo, drum..";
	lines[3]="2 dest: cutoff, pitch, vol";
	lines[4]="3 amount: how far, +/-";
	lines[5]="4 A+Start hear it";
	lines[6]="LB+Up/Down: next slot";
}
