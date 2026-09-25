#include "InstrumentView.h"
#include "Application/Instruments/MidiInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Model/Table.h"
#include "Application/AppWindow.h"
#include "Application/Model/Config.h"
#include "BaseClasses/UIBigHexVarField.h"
#include "BaseClasses/UIIntVarOffField.h"
#include "BaseClasses/UINoteVarField.h"
#include "BaseClasses/UIStaticField.h"
#include "Foundation/Variables/Variable.h"
#include "ModalDialogs/ImportSampleDialog.h"
#include "ModalDialogs/InstrumentListDialog.h"
#include "ModalDialogs/MessageBox.h"
#include "System/System/System.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string.h>


static char *instrumentTypeNames[3]={(char *)"sample",(char *)"synth",(char *)"macro"} ;

// Synth and macro share the synth pages, keys and audition
static bool isSynthLike(InstrumentType t) {
	return t==IT_SYNTH || t==IT_MACRO ;
}

// Upstream c8b71bb: rebuild the fields after importing, so the sample
// list includes the new files
static void ImportSampleDialogCallback(View &v, ModalView &dialog) {
    ((InstrumentView &)v).OnFocus();
}

InstrumentView::InstrumentView(GUIWindow &w,ViewData *data):FieldView(w,data) {

	project_=data->project_ ;
	lastFocusID_=0 ;
	current_=0 ;
	currentSlot_=-1 ;
	labPage_=0 ;
	markerFocus_=SIP_START ;
	previewLoop_=false ;
	typeVar_=new Variable("type",INSTRUMENT_TYPE_FIELD,instrumentTypeNames,3,0) ;
	onInstrumentChange() ;
}

InstrumentView::~InstrumentView() {
	delete typeVar_ ;
}

InstrumentType InstrumentView::getInstrumentType() {
	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instrument=bank->GetInstrument(i) ;
    return instrument->GetType() ;
} ;

void InstrumentView::onInstrumentChange() {

	ClearFocus() ;

	I_Instrument *old=current_ ;

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	current_=bank->GetInstrument(i) ;
	currentSlot_=i ;

	if (old && current_!=old) {
		old->RemoveObserver(*this) ;
	} ;
	T_SimpleList<UIField>::Empty() ;

	InstrumentType it=getInstrumentType() ;
	// The type field mirrors this instrument from now on: applyTypeChange()
	// reads it after every key, on every page
	if (i<MAX_SAMPLEINSTRUMENT_COUNT) {
		typeVar_->SetInt(it==IT_MACRO?2:(it==IT_SYNTH?1:0),false) ;
	}

    switch (it) {
		case IT_MIDI:
			fillMidiParameters() ;
			break ;
		case IT_SAMPLE:
			fillSampleParameters() ;
			break ;
		case IT_SYNTH:
		case IT_MACRO:
			fillSynthParameters() ;
			break ;
		default:
			break ;
	} ;

	// Restore the remembered field; otherwise land on what you most likely
	// want to change: the sound preset or the sample, not the type switch.
	SetFocus(T_SimpleList<UIField>::GetFirst()) ;
	FourCC preferred=isSynthLike(it)?SYP_PRESET:SIP_SAMPLE ;
	UIField *remembered=0 ;
	UIField *fallback=0 ;
	IteratorPtr<UIField> it2(T_SimpleList<UIField>::GetIterator()) ;
	for (it2->Begin();!it2->IsDone();it2->Next()) {
        UIIntVarField &field=(UIIntVarField &)it2->CurrentItem() ;
        if (lastFocusID_!=0 && field.GetVariableID()==lastFocusID_) {
            remembered=&field ;
        }
        if (field.GetVariableID()==preferred) {
            fallback=&field ;
        }
    } ;
	if (remembered) {
		SetFocus(remembered) ;
	} else if (fallback) {
		SetFocus(fallback) ;
	}
	if (current_!=old) {
		current_->AddObserver(*this) ;
	}
} ;

void InstrumentView::fillSampleParameters() {

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instr=bank->GetInstrument(i) ;
	SampleInstrument *instrument=(SampleInstrument *)instr  ;
	GUIPoint position=GetAnchor() ;

	// On mini layouts, left-align the instrument menu
	if (miniLayout_) {
		position._x = 1;
	}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	// Shift menu up by 3 lines to fit more fields on screen
	position._y -= 3;
#endif

	position._y=17;
	switch(labPage_) {
		case 0:
			fillSampleSourcePage(instrument,position);
			break;
		case 1:
			fillSampleShapePage(instrument,position);
			break;
		case 2:
			fillSampleFilterPage(instrument,position);
			break;
		case 3:
			fillSampleLoopPage(instrument,position);
			break;
		case INSTRUMENT_MOD_PAGE:
			fillModPage(instrument,position);
			break;
		default:
			fillSampleMotionPage(instrument,position);
			break;
	}
} ;

void InstrumentView::fillSampleSourcePage(SampleInstrument *instrument, GUIPoint position) {
	addTypeField(position) ;
	Variable *v=instrument->FindVariable(SIP_SAMPLE) ;
	SamplePool *sp=SamplePool::GetInstance() ;
	int sampleMax=sp->GetNameListSize()-1;
	if (sampleMax<0) sampleMax=0;
	UIIntVarField *f1=new UIIntVarField(position,*v,"sample %s",0,sampleMax,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_ROOTNOTE) ;
	f1=new UINoteVarField(position,*v,"root   %s",0,0x7F,1,0x0C) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_FINETUNE) ;
	f1=new UIIntVarField(position,*v,"detune %2.2X",0,255,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_SLICES) ;
	f1=new UIIntVarField(position,*v,"slices %2.2X",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
}

void InstrumentView::fillSampleShapePage(SampleInstrument *instrument, GUIPoint position) {
	Variable *v=instrument->FindVariable(SIP_VOLUME) ;
	UIIntVarField *f1=new UIIntVarField(position,*v,"vol    %3d %2.2X",0,255,1,10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_PAN) ;
	f1=new UIIntVarField(position,*v,"pan    %2.2X",0,0xFE,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_REVERB) ;
	f1=new UIIntVarField(position,*v,"reverb %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_DELAY) ;
	f1=new UIIntVarField(position,*v,"delay  %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_CHORUS) ;
	f1=new UIIntVarField(position,*v,"chorus %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_CRUSH);
	f1=new UIIntVarField(position,*v,"crush  %2.2d",1,0x10,1,4) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_CRUSHVOL);
	f1=new UIIntVarField(position,*v,"drive  %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_DOWNSMPL) ;
	f1=new UIIntVarField(position,*v,"down   %d",0,8,1,4) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_INTERPOLATION) ;
	f1=new UIIntVarField(position,*v,"interp %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
}

void InstrumentView::fillSampleFilterPage(SampleInstrument *instrument, GUIPoint position) {
	Variable *v=instrument->FindVariable(SIP_FILTCUTOFF) ;
	UIIntVarField *f1=new UIIntVarField(position,*v,"cut    %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_FILTRESO) ;
	f1=new UIIntVarField(position,*v,"reso   %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_FILTMIX) ;
	f1=new UIIntVarField(position,*v,"type   %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_FILTMODE) ;
	f1=new UIIntVarField(position,*v,"mode   %s",0,2,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_ATTENUATE) ;
	f1=new UIIntVarField(position,*v,"atten  %3d %2.2X",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
}

void InstrumentView::fillSampleLoopPage(SampleInstrument *instrument, GUIPoint position) {
	int max=instrument->GetSampleSize()-1;
	if (max<0) max=0;
	Variable *v=instrument->FindVariable(SIP_LOOPMODE) ;
	UIIntVarField *f1=new UIIntVarField(position,*v,"mode   %s",0,SILM_LAST-1,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_SLICES) ;
	f1=new UIIntVarField(position,*v,"slices %2.2X",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_START) ;
	f1=new UIBigHexVarField(position,*v,7,"start  %7.7X",0,max,16) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_LOOPSTART) ;
	f1=new UIBigHexVarField(position,*v,7,"lstart %7.7X",0,max,16) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_END) ;
	f1=new UIBigHexVarField(position,*v,7,"lend   %7.7X",0,max,16) ;
	T_SimpleList<UIField>::Insert(f1) ;
}

void InstrumentView::fillSampleMotionPage(SampleInstrument *instrument, GUIPoint position) {
	Variable *v=instrument->FindVariable(SIP_TABLEAUTO) ;
	UIIntVarField *f2=new UIIntVarField(position,*v,"auto   %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_TABLE) ;
	UIIntVarField *f1=new UIIntVarOffField(position,*v,"table  %2.2X",0x00,0x7F,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_FBTUNE) ;
	f1=new UIIntVarField(position,*v,"fb tun %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_FBMIX) ;
	f1=new UIIntVarField(position,*v,"fb mix %2.2X",0,0xFF,1,0X10) ;
	T_SimpleList<UIField>::Insert(f1) ;
}

void InstrumentView::fillMidiParameters() {

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instr=bank->GetInstrument(i) ;
	MidiInstrument *instrument=(MidiInstrument *)instr  ;
	GUIPoint position=GetAnchor() ;

	Variable *v=instrument->FindVariable(MIP_CHANNEL) ;
	UIIntVarField* f1=new UIIntVarField(position,*v,"channel: %2.2d",0,0x0F,1,0x04,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	f1->SetFocus() ;

	position._y+=1;
	v=instrument->FindVariable(MIP_VOLUME) ;
	f1=new UIIntVarField(position,*v,"volume: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1;
	v=instrument->FindVariable(MIP_NOTELENGTH) ;
	f1=new UIIntVarField(position,*v,"length: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	v=instrument->FindVariable(MIP_TABLEAUTO) ;
	position._y+=2 ;
	UIIntVarField *f2=new UIIntVarField(position,*v,"automation: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1;
	v=instrument->FindVariable(MIP_TABLE) ;
	f1=new UIIntVarOffField(position,*v,"table: %2.2X",0,0x7F,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

} ;

static int GetVarInt(I_Instrument *instr, FourCC id) {
	if (!instr) return 0;
	Variable *v=instr->FindVariable(id);
	if (!v) return 0;
	return v->GetInt();
}

static const char *GetVarString(I_Instrument *instr, FourCC id) {
	if (!instr) return "";
	Variable *v=instr->FindVariable(id);
	if (!v) return "";
	const char *value=v->GetString();
	return value ? value : "";
}

const char *InstrumentView::getLabPageName() {
	switch(labPage_) {
		case 0: return "SOURCE";
		case 1: return "SHAPE";
		case 2: return "FILTER";
		case 3: return "LOOP";
		case INSTRUMENT_MOD_PAGE: return "MOD";
		default: return "MOTION";
	}
}

bool InstrumentView::isWaveMarkerPage() {
	return getInstrumentType()==IT_SAMPLE && (labPage_==0 || labPage_==3);
}

const char *InstrumentView::getWaveMarkerName() {
	if (markerFocus_==SIP_LOOPSTART) return "LSTART";
	if (markerFocus_==SIP_END) return "END";
	return "START";
}

void InstrumentView::normalizeWaveMarkers(SampleInstrument *instrument, FourCC changedMarker) {
	if (!instrument) {
		return;
	}
	int sampleSize=instrument->GetSampleSize();
	if (sampleSize<=0) {
		return;
	}
	Variable *startVar=instrument->FindVariable(SIP_START);
	Variable *loopVar=instrument->FindVariable(SIP_LOOPSTART);
	Variable *endVar=instrument->FindVariable(SIP_END);
	if (!startVar || !loopVar || !endVar) {
		return;
	}
	int start=startVar->GetInt();
	int loopStart=loopVar->GetInt();
	int end=endVar->GetInt();
	if (end<=0 || end>sampleSize) end=sampleSize;
	if (start<0) start=0;
	if (loopStart<0) loopStart=0;
	if (start>sampleSize) start=sampleSize;
	if (loopStart>sampleSize) loopStart=sampleSize;

	if (start>end) {
		if (changedMarker==SIP_END) {
			end=start;
		} else {
			start=end;
		}
	}
	if (loopStart<start) loopStart=start;
	if (loopStart>end) loopStart=end;

	startVar->SetInt(start);
	loopVar->SetInt(loopStart);
	endVar->SetInt(end);
}

const char *InstrumentView::getWaveMarkerShortName() {
	if (markerFocus_==SIP_LOOPSTART) return "L";
	if (markerFocus_==SIP_END) return "E";
	return "S";
}

void InstrumentView::cycleWaveMarker(int offset) {
	int index=0;
	if (markerFocus_==SIP_LOOPSTART) index=1;
	if (markerFocus_==SIP_END) index=2;
	index+=offset;
	if (index<0) index=2;
	if (index>2) index=0;
	switch(index) {
		case 1:
			markerFocus_=SIP_LOOPSTART;
			break;
		case 2:
			markerFocus_=SIP_END;
			break;
		default:
			markerFocus_=SIP_START;
			break;
	}
	isDirty_=true;
}

void InstrumentView::nudgeWaveMarker(int offset, int multiplier) {
	if (!isWaveMarkerPage()) {
		return;
	}
	int i=viewData_->currentInstrument_;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
	SampleInstrument *instrument=(SampleInstrument *)bank->GetInstrument(i);
	if (!instrument) {
		return;
	}
	int sampleSize=instrument->GetSampleSize();
	if (sampleSize<=0) {
		return;
	}
	Variable *v=instrument->FindVariable(markerFocus_);
	if (!v) {
		return;
	}
	int step=sampleSize/128;
	if (step<1) step=1;
	if (multiplier<1) multiplier=1;
	step*=multiplier;
	int value=v->GetInt()+(offset*step);
	int start=GetVarInt(instrument,SIP_START);
	int loopStart=GetVarInt(instrument,SIP_LOOPSTART);
	int end=GetVarInt(instrument,SIP_END);
	if (end<=0 || end>sampleSize) end=sampleSize;
	if (markerFocus_==SIP_START) {
		if (value<0) value=0;
		if (value>loopStart) value=loopStart;
		start=value;
	} else if (markerFocus_==SIP_LOOPSTART) {
		if (value<start) value=start;
		if (value>end) value=end;
		loopStart=value;
	} else {
		if (value<loopStart) value=loopStart;
		if (value>sampleSize) value=sampleSize;
		end=value;
	}
	v->SetInt(value);
	normalizeWaveMarkers(instrument,markerFocus_);
	isDirty_=true;
}

void InstrumentView::auditionSamplePitch(int offset) {
	if (getInstrumentType()!=IT_SAMPLE) {
		return;
	}
	int i=viewData_->currentInstrument_;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
	SampleInstrument *instrument=(SampleInstrument *)bank->GetInstrument(i);
	if (!instrument || instrument->IsEmpty()) {
		return;
	}
	int root=GetVarInt(instrument,SIP_ROOTNOTE);
	int note=root+offset;
	if (note<0) note=0;
	if (note>127) note=127;
	Variable *loopMode=instrument->FindVariable(SIP_LOOPMODE);
	int originalLoopMode=loopMode?loopMode->GetInt():SILM_ONESHOT;
	if (loopMode) {
		loopMode->SetInt(previewLoop_?SILM_LOOP:SILM_ONESHOT);
	}
	Player::GetInstance()->AuditionInstrument(i,note);
	if (loopMode) {
		loopMode->SetInt(originalLoopMode);
	}
	isDirty_=true;
}

void InstrumentView::toggleSamplePreviewLoop() {
	if (getInstrumentType()!=IT_SAMPLE || !isWaveMarkerPage()) {
		return;
	}
	previewLoop_=!previewLoop_;
	isDirty_=true;
}

void InstrumentView::drawLabBar(int x, int y, int width, int value, int maxValue) {
	if (width<=0) return;
	if (maxValue<=0) maxValue=1;
	if (value<0) value=0;
	if (value>maxValue) value=maxValue;
	int fill=(value*width)/maxValue;
	char buffer[40];
	if (width>30) width=30;
	for (int i=0;i<width;i++) {
		buffer[i]=(i<fill)?'#':'-';
	}
	buffer[width]=0;
	GUITextProperties props;
	SetColor(CD_HILITE1);
	DrawString(x,y,buffer,props);
	SetColor(CD_NORMAL);
}

void InstrumentView::drawMarkerLine(int x, int y, int height, ColorDefinition color, FourCC markerType) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor markerColor=AppWindow::ThemeColor(CD_NORMAL);
	bool active=(color==CD_HILITE1);
	if (color==CD_PLAY) {
		markerColor=AppWindow::ThemeColor(CD_PLAY);
	} else if (color==CD_HILITE2) {
		markerColor=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE2,60);
	}
	int markerWidth=active?3:1;
	GUIRect marker(x-(markerWidth/2),y,x-(markerWidth/2)+markerWidth,y+height);
	imp->SetColor(markerColor);
	imp->DrawRect(marker);
	if (active) {
		if (markerType==SIP_START) {
			GUIRect top(x-4,y,x+5,y+3);
			GUIRect stem(x-2,y+3,x+3,y+6);
			imp->DrawRect(top);
			imp->DrawRect(stem);
		} else if (markerType==SIP_LOOPSTART) {
			int mid=y+(height/2);
			GUIRect left(x-5,mid-2,x-2,mid+3);
			GUIRect centerBox(x-2,mid-4,x+3,mid+5);
			GUIRect right(x+3,mid-2,x+6,mid+3);
			imp->DrawRect(left);
			imp->DrawRect(centerBox);
			imp->DrawRect(right);
		} else {
			GUIRect stem(x-2,y+height-6,x+3,y+height-3);
			GUIRect bottom(x-4,y+height-3,x+5,y+height);
			imp->DrawRect(stem);
			imp->DrawRect(bottom);
		}
	}
	SetColor(CD_NORMAL);
#endif
}

void InstrumentView::drawPixelLabBar(int x, int y, int width, int height, int value,
                                     int maxValue, bool bipolar) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	if (width<=0 || height<=0) return;
	if (maxValue<=0) maxValue=1;
	if (value<0) value=0;
	if (value>maxValue) value=maxValue;

	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor fill=AppWindow::ThemeColor(CD_HILITE2);
	GUIColor fill2=AppWindow::ThemeColor(CD_NORMAL);
	imp->SetColor(frame);
	GUIRect outer(x,y,x+width,y+height);
	imp->DrawRect(outer);
	imp->SetColor(panel);
	GUIRect inner(x+1,y+1,x+width-1,y+height-1);
	imp->DrawRect(inner);

	if (bipolar) {
		int center=x+(width/2);
		imp->SetColor(frame);
		GUIRect centerLine(center,y+1,center+1,y+height-1);
		imp->DrawRect(centerLine);
		int pos=x+1+((value*(width-2))/maxValue);
		int left=pos<center?pos:center;
		int right=pos>center?pos:center;
		imp->SetColor(fill);
		GUIRect filled(left,y+2,right+1,y+height-2);
		imp->DrawRect(filled);
		imp->SetColor(fill2);
		GUIRect cursor(pos-1,y+1,pos+2,y+height-1);
		imp->DrawRect(cursor);
	} else {
		int fillWidth=(value*(width-2))/maxValue;
		if (fillWidth>0) {
			imp->SetColor(fill);
			GUIRect filled(x+1,y+1,x+1+fillWidth,y+height-1);
			imp->DrawRect(filled);
		}
	}
#else
	drawLabBar(x/8,y/8,width/8,value,maxValue);
#endif
}

void InstrumentView::drawLabText(int x, int y, const char *text, GUITextProperties &props) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	GUIPoint pos(x,y);
	w_.DrawString(text,pos,props,true);
#else
	DrawString(x,y,text,props);
#endif
}

void InstrumentView::drawSampleWaveform(SampleInstrument *instrument, int x, int y,
                                        int width, int height, bool showMarkers) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor center=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
	GUIColor trace=AppWindow::ThemeColor(CD_HILITE2);
	GUIColor traceHot=AppWindow::ThemeColor(CD_NORMAL);
	imp->SetColor(frame);
	GUIRect outer(x,y,x+width,y+height);
	imp->DrawRect(outer);
	imp->SetColor(panel);
	GUIRect clear(x+1,y+1,x+width-1,y+height-1);
	imp->DrawRect(clear);

	int sampleIndex=instrument->GetSampleIndex();
	SamplePool *pool=SamplePool::GetInstance();
	SoundSource *source=0;
	if (sampleIndex>=0 && sampleIndex<pool->GetNameListSize()) {
		source=pool->GetSource(sampleIndex);
	}

	int mid=y+(height/2);
	imp->SetColor(center);
	for (int col=0; col<width-2; col+=12) {
		GUIRect centerLine(x+1+col,mid,x+3+col,mid+1);
		imp->DrawRect(centerLine);
	}

	if (!source) {
		GUITextProperties props;
		SetColor(CD_NORMAL);
		DrawString((x+8)/8,(y+height/2-4)/8,"no sample",props);
		return;
	}

	int sampleSize=source->GetSize(-1);
	int channels=source->GetChannelCount(-1);
	short *samples=(short *)source->GetSampleBuffer(-1);
	if (!samples || sampleSize<=0) {
		GUITextProperties props;
		SetColor(CD_NORMAL);
		DrawString((x+8)/8,(y+height/2-4)/8,"sample pending",props);
		return;
	}
	if (channels<=0) channels=1;

	int drawableWidth=width-4;
	int drawableHeight=height-6;
	int peakAbs=1;
	for (int col=0; col<drawableWidth; col++) {
		int start=(col*sampleSize)/drawableWidth;
		int end=((col+1)*sampleSize)/drawableWidth;
		if (end<=start) end=start+1;
		if (end>sampleSize) end=sampleSize;
		for (int i=start; i<end; i++) {
			int sample=samples[i*channels];
			int absSample=sample<0?-sample:sample;
			if (absSample>peakAbs) peakAbs=absSample;
		}
	}
	if (peakAbs<512) peakAbs=512;

	for (int col=0; col<drawableWidth; col++) {
		int start=(col*sampleSize)/drawableWidth;
		int end=((col+1)*sampleSize)/drawableWidth;
		if (end<=start) end=start+1;
		if (end>sampleSize) end=sampleSize;
		int minSample=0;
		int maxSample=0;
		for (int i=start; i<end; i++) {
			int sample=samples[i*channels];
			if (sample<minSample) minSample=sample;
			if (sample>maxSample) maxSample=sample;
		}
		int top=mid-((maxSample*(drawableHeight/2))/peakAbs);
		int bottom=mid-((minSample*(drawableHeight/2))/peakAbs);
		if (top<y+3) top=y+3;
		if (bottom>y+height-4) bottom=y+height-4;
		if (bottom<top) bottom=top;
		imp->SetColor(trace);
		GUIRect wave(x+2+col,top,x+3+col,bottom+1);
		imp->DrawRect(wave);
	}

	if (showMarkers) {
		int start=GetVarInt(instrument,SIP_START);
		int loopStart=GetVarInt(instrument,SIP_LOOPSTART);
		int loopEnd=GetVarInt(instrument,SIP_END);
		if (loopEnd<=0 || loopEnd>sampleSize) loopEnd=sampleSize;
		if (start<0) start=0;
		if (loopStart<0) loopStart=0;
		if (start>sampleSize) start=sampleSize;
		if (loopStart>sampleSize) loopStart=sampleSize;
		int sx=x+2+((start*drawableWidth)/sampleSize);
		int ls=x+2+((loopStart*drawableWidth)/sampleSize);
		int le=x+2+((loopEnd*drawableWidth)/sampleSize);
		drawMarkerLine(sx,y+1,height-2,markerFocus_==SIP_START?CD_HILITE1:CD_PLAY,SIP_START);
		drawMarkerLine(ls,y+1,height-2,markerFocus_==SIP_LOOPSTART?CD_HILITE1:CD_HILITE2,SIP_LOOPSTART);
		drawMarkerLine(le,y+1,height-2,markerFocus_==SIP_END?CD_HILITE1:CD_HILITE2,SIP_END);
	}
#endif
}

void InstrumentView::drawSampleLabVisuals() {
	if (getInstrumentType()!=IT_SAMPLE) {
		return;
	}
	int i=viewData_->currentInstrument_;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
	SampleInstrument *instrument=(SampleInstrument *)bank->GetInstrument(i);
	if (!instrument) {
		return;
	}

	GUITextProperties props;
	char line[40];
	SetColor(CD_HILITE2);
	sprintf(line,"<LB %d/%d %s LB>",labPage_+1,INSTRUMENT_PAGE_COUNT,getLabPageName());
	DrawString((30-(int)strlen(line))/2,2,line,props);
	SetColor(CD_NORMAL);

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
	GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
	imp->SetColor(panel);
	GUIRect clearPanel(0,34,240,128);
	imp->DrawRect(clearPanel);
#endif

	int sampleSize=instrument->GetSampleSize();
	UIIntVarField *focused=(UIIntVarField *)GetFocus();
	FourCC changedMarker=markerFocus_;
	if (focused && (focused->GetVariableID()==SIP_START ||
	                focused->GetVariableID()==SIP_LOOPSTART ||
	                focused->GetVariableID()==SIP_END)) {
		changedMarker=focused->GetVariableID();
	}
	normalizeWaveMarkers(instrument,changedMarker);
	int start=GetVarInt(instrument,SIP_START);
	int loopStart=GetVarInt(instrument,SIP_LOOPSTART);
	int loopEnd=GetVarInt(instrument,SIP_END);
	if (sampleSize<=0) sampleSize=1;
	if (loopEnd<=0 || loopEnd>sampleSize) loopEnd=sampleSize;
	if (start<0) start=0;
	if (loopStart<0) loopStart=0;
	if (start>sampleSize) start=sampleSize;
	if (loopStart>sampleSize) loopStart=sampleSize;

	if (labPage_==0 || labPage_==3) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
		drawSampleWaveform(instrument,10,36,220,52,true);
		sprintf(line,"EDIT %-5s PREV:%s",getWaveMarkerName(),previewLoop_?"LOOP":"ONCE");
		drawLabText((30-(int)strlen(line))/2,12,line,props);
		sprintf(line,"S%05X L%05X E%05X",start,loopStart,loopEnd);
		drawLabText((30-(int)strlen(line))/2,13,line,props);
		if (labPage_==0) {
			// What the recording is (its name is in the list below)
			int index=instrument->GetSampleIndex();
			SamplePool *pool=SamplePool::GetInstance();
			SoundSource *source=(index>=0 && index<pool->GetNameListSize())?pool->GetSource(index):0;
			if (source && source->GetSampleRate(-1)>0) {
				int rate=source->GetSampleRate(-1);
				float seconds=source->GetSize(-1)/(float)rate;
				sprintf(line,"%.2f s  %s  %.1f kHz",seconds,
				        source->GetChannelCount(-1)>1?"stereo":"mono",rate/1000.0f);
				drawLabText((30-(int)strlen(line))/2,14,line,props);
			}
			int suggestedRoot=instrument->GetSuggestedRootNote();
			if (suggestedRoot>=0) {
				sprintf(line,"suggest root %03d Sel",suggestedRoot);
				drawLabText(2,15,line,props);
			}
		}
#else
		char wave[25];
		for (int j=0;j<24;j++) {
			int pattern=(j*7 + j*j) % 11;
			wave[j]=(pattern<2)?'.':((pattern<5)?'-':((pattern<8)?'=':'#'));
		}
		wave[24]=0;
		int sx=(start*23)/sampleSize;
		int ls=(loopStart*23)/sampleSize;
		int le=(loopEnd*23)/sampleSize;
		if (sx<0) sx=0;
		if (sx>23) sx=23;
		if (ls<0) ls=0;
		if (ls>23) ls=23;
		if (le<0) le=0;
		if (le>23) le=23;
		wave[sx]='S';
		wave[ls]='L';
		wave[le]='E';
		SetColor(CD_HILITE1);
		DrawString(3,5,wave,props);
		SetColor(CD_NORMAL);
		DrawString(3,7,"Start auditions phrase",props);
		if (labPage_==0) {
			const char *sampleName=GetVarString(instrument,SIP_SAMPLE);
			char name[25];
			strncpy(name,sampleName,24);
			name[24]=0;
			DrawString(3,9,name,props);
			sprintf(line,"root %03d det %02X sl %02X",GetVarInt(instrument,SIP_ROOTNOTE),GetVarInt(instrument,SIP_FINETUNE),GetVarInt(instrument,SIP_SLICES));
			DrawString(3,11,line,props);
		} else {
			sprintf(line,"mode %d  S%05X",GetVarInt(instrument,SIP_LOOPMODE),start);
			DrawString(3,9,line,props);
			sprintf(line,"L%05X E%05X",loopStart,loopEnd);
			DrawString(3,11,line,props);
		}
#endif
	} else if (labPage_==1) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
		const int barX=44;
		const int barW=152;
		SetColor(CD_NORMAL);
		drawLabText(2,5,"VOL",props);
		drawPixelLabBar(barX,40,barW,14,GetVarInt(instrument,SIP_VOLUME),255);
		SetColor(CD_NORMAL);
		drawLabText(2,8,"PAN",props);
		drawPixelLabBar(barX,64,barW,14,GetVarInt(instrument,SIP_PAN),254,true);
		SetColor(CD_NORMAL);
		drawLabText(2,11,"GRIT",props);
		drawPixelLabBar(barX,88,barW,14,16-GetVarInt(instrument,SIP_CRUSH)+GetVarInt(instrument,SIP_DOWNSMPL)*2,31);
#else
		DrawString(3,5,"VOL",props);
		drawLabBar(8,5,16,GetVarInt(instrument,SIP_VOLUME),255);
		DrawString(3,7,"PAN L-----|-----R",props);
		int pan=(GetVarInt(instrument,SIP_PAN)*11)/254;
		char panLine[13];
		strcpy(panLine,"-----------");
		if (pan<0) pan=0;
		if (pan>10) pan=10;
		panLine[pan]='|';
		panLine[11]=0;
		SetColor(CD_HILITE1);
		DrawString(8,8,panLine,props);
		SetColor(CD_NORMAL);
		DrawString(3,10,"GRIT",props);
		drawLabBar(8,10,16,16-GetVarInt(instrument,SIP_CRUSH)+GetVarInt(instrument,SIP_DOWNSMPL)*2,31);
		DrawString(3,12,"drive/downsample texture",props);
#endif
	} else if (labPage_==2) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
		const int barX=44;
		const int barW=152;
		SetColor(CD_NORMAL);
		drawLabText(2,5,"CUT",props);
		drawPixelLabBar(barX,40,barW,14,GetVarInt(instrument,SIP_FILTCUTOFF),255);
		SetColor(CD_NORMAL);
		drawLabText(2,8,"RES",props);
		drawPixelLabBar(barX,64,barW,14,GetVarInt(instrument,SIP_FILTRESO),255);
		SetColor(CD_NORMAL);
		drawLabText(2,11,"TYPE",props);
		drawPixelLabBar(barX,88,barW,14,GetVarInt(instrument,SIP_FILTMIX),255);
#else
		DrawString(3,5,"CUT",props);
		drawLabBar(8,5,16,GetVarInt(instrument,SIP_FILTCUTOFF),255);
		DrawString(3,7,"RES",props);
		drawLabBar(8,7,16,GetVarInt(instrument,SIP_FILTRESO),255);
		DrawString(3,9,"TYPE low <----> high",props);
		drawLabBar(8,10,16,GetVarInt(instrument,SIP_FILTMIX),255);
		sprintf(line,"mode %d atten %02X",GetVarInt(instrument,SIP_FILTMODE),GetVarInt(instrument,SIP_ATTENUATE));
		DrawString(3,12,line,props);
#endif
	} else if (labPage_==INSTRUMENT_MOD_PAGE) {
		drawModPlot(instrument,10,36,220,52);
		// The focused setting in real units (ms, Hz, semitones)
		UIIntVarField *field=(UIIntVarField *)GetFocus();
		if (field) {
			char l1[40],l2[40],value[40];
			getModFieldHelp(field->GetVariableID(),instrument,l1,l2,value);
			if (value[0]) {
				SetColor(CD_HILITE1);
				drawLabText((30-(int)strlen(value))/2,13,value,props);
				SetColor(CD_NORMAL);
			}
		}
	} else {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
		// The instrument's table: 16 steps down three command columns, a
		// filled cell where a step has a command
		int table=GetVarInt(instrument,SIP_TABLE);
		SetColor(CD_HILITE1);
		if (table<0 || table>=TABLE_COUNT) {
			drawLabText(2,6,"no table yet",props);
			SetColor(CD_NORMAL);
			drawLabText(2,8,"table: pick one below,",props);
			drawLabText(2,9,"A on it makes a new one",props);
		} else {
			sprintf(line,"TABLE %02X  auto %s",table,GetVarInt(instrument,SIP_TABLEAUTO)?"on":"off");
			drawLabText(2,5,line,props);
			Table &t=TableHolder::GetInstance()->GetTable(table);
			FourCC *lanes[3]={t.cmd1_,t.cmd2_,t.cmd3_};
			SDLGUIWindowImp *imp=(SDLGUIWindowImp *)w_.GetImpWindow();
			GUIColor off=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,40);
			GUIColor on=AppWindow::ThemeColor(CD_HILITE2);
			GUIColor beat=AppWindow::ThemeColor(CD_NORMAL);
			const int cell=13;
			const int gx=(240-16*cell)/2;
			for (int lane=0;lane<3;lane++) {
				int gy=56+lane*14;
				for (int step=0;step<TABLE_STEPS;step++) {
					bool used=lanes[lane][step]!=I_CMD_NONE;
					imp->SetColor(used?((step%4)==0?beat:on):off);
					GUIRect r(gx+step*cell,gy,gx+step*cell+cell-2,gy+10);
					imp->DrawRect(r);
				}
			}
			SetColor(CD_NORMAL);
			drawLabText(2,13,"3 columns, 1 step per tick",props);
		}
		SetColor(CD_NORMAL);
#else
		DrawString(3,5,"INST TABLE MOTION",props);
		int table=GetVarInt(instrument,SIP_TABLE);
		sprintf(line,"auto %s table %02X",GetVarInt(instrument,SIP_TABLEAUTO)?"on":"off",table<0?0:table);
		DrawString(3,7,line,props);
		SetColor(CD_HILITE1);
		DrawString(3,9,"[ ][>][ ][>][ ][>][ ]",props);
		SetColor(CD_NORMAL);
		DrawString(3,11,"fb tune/mix + table fx",props);
#endif
	}
}


static void InstrumentListCallback(View &v, ModalView &dialog) {
	if (dialog.GetReturnCode()>0) {
		InstrumentListDialog &list=(InstrumentListDialog &)dialog ;
		((InstrumentView &)v).OpenInstrument(list.GetSelection()) ;
	}
}

void InstrumentView::OpenInstrument(int instrument) {
	viewData_->currentInstrument_=instrument ;
	onInstrumentChange() ;
	isDirty_=true ;
}

void InstrumentView::warpToNext(int offset) {
	int instrument=viewData_->currentInstrument_+offset ;
	if (instrument>=MAX_INSTRUMENT_COUNT) {
		instrument=instrument-MAX_INSTRUMENT_COUNT ;
	} ;
	if (instrument<0) {
		instrument=MAX_INSTRUMENT_COUNT+instrument ;
	} ;
	viewData_->currentInstrument_=instrument ;
	onInstrumentChange() ;
	isDirty_=true ;
} ;

void InstrumentView::switchLabPage(int offset) {
	labPage_+=offset;
	if (labPage_<0) {
		labPage_=INSTRUMENT_PAGE_COUNT-1;
	}
	if (labPage_>=INSTRUMENT_PAGE_COUNT) {
		labPage_=0;
	}
	lastFocusID_=0;
	onInstrumentChange();
	isDirty_=true;
} ;

void InstrumentView::ProcessButtonMask(unsigned short mask,bool pressed) {

	if (!pressed) return ;

	isDirty_=false ;
	syncReplacedInstrument() ;

	if (isSynthLike(getInstrumentType()) && (mask&EPBM_L) &&
	    !(mask&(EPBM_A|EPBM_B|EPBM_R|EPBM_START|EPBM_SELECT))) {
		if (mask&EPBM_LEFT) {
			switchLabPage(-1);
			return;
		}
		if (mask&EPBM_RIGHT) {
			switchLabPage(1);
			return;
		}
	}

	// A+Start: hear this instrument (again: stop), like the M8's EDIT+PLAY.
	// RB+A+Left/Right hear it an octave down / up.
	if (mask==(EPBM_A|EPBM_START) &&
	    (isSynthLike(getInstrumentType()) || getInstrumentType()==IT_SAMPLE)) {
		Player *player=Player::GetInstance();
		if (player->IsRunning() && viewData_->playMode_==PM_AUDITION) {
			player->Stop();
			isDirty_=true;
		} else if (isSynthLike(getInstrumentType())) {
			auditionSynth(0);
		} else {
			auditionSamplePitch(0);
		}
		return;
	}
	// B+A on a knob: back to its default (like the M8's EDIT+OPTION); the
	// sample and table fields keep their own B+A (remove / clear) below
	if (mask==(EPBM_B|EPBM_A)) {
		UIIntVarField *focus=(UIIntVarField *)GetFocus();
		if (focus) {
			Variable &v=focus->GetVariable();
			FourCC id=focus->GetVariableID();
			bool special=(id==SIP_SAMPLE || id==SIP_TABLE || id==MIP_TABLE ||
			              !strcmp(v.GetName(),"preset") || !strcmp(v.GetName(),"type"));
			if (!special) {
				v.ResetToDefault();
				isDirty_=true;
				return;
			}
		}
	}

	if (isSynthLike(getInstrumentType()) && (mask&EPBM_R) && (mask&EPBM_A) &&
	    !(mask&(EPBM_B|EPBM_L|EPBM_START|EPBM_SELECT))) {
		if (mask&EPBM_LEFT) {
			auditionSynth(-12);
			return;
		}
		if (mask&EPBM_RIGHT) {
			auditionSynth(12);
			return;
		}
	}

	if (getInstrumentType()==IT_SAMPLE && (mask&EPBM_L) &&
	    !(mask&(EPBM_A|EPBM_B|EPBM_R|EPBM_START|EPBM_SELECT))) {
		if (isWaveMarkerPage() && (mask&EPBM_UP)) {
			cycleWaveMarker(-1);
			return;
		}
		if (isWaveMarkerPage() && (mask&EPBM_DOWN)) {
			cycleWaveMarker(1);
			return;
		}
		if (mask&EPBM_LEFT) {
			switchLabPage(-1);
			return;
		}
		if (mask&EPBM_RIGHT) {
			switchLabPage(1);
			return;
		}
	}

	if (isWaveMarkerPage() && (mask&EPBM_L) && (mask&EPBM_A) &&
	    !(mask&(EPBM_B|EPBM_R|EPBM_START|EPBM_SELECT))) {
		if (mask&EPBM_LEFT) {
			nudgeWaveMarker(-1);
			return;
		}
		if (mask&EPBM_RIGHT) {
			nudgeWaveMarker(1);
			return;
		}
	}

	if (getInstrumentType()==IT_SAMPLE && (mask&EPBM_R) && (mask&EPBM_A) &&
	    !(mask&(EPBM_B|EPBM_L|EPBM_START|EPBM_SELECT))) {
		if (mask&EPBM_LEFT) {
			auditionSamplePitch(-12);
			return;
		}
		if (mask&EPBM_RIGHT) {
			auditionSamplePitch(12);
			return;
		}
	}

	// On the waveform pages RB+Start toggles the loop preview; elsewhere it
	// plays the song like on every other screen
	if (getInstrumentType()==IT_SAMPLE && isWaveMarkerPage() && mask==(EPBM_R|EPBM_START)) {
		toggleSamplePreviewLoop();
		return;
	}

	if (getInstrumentType()==IT_SAMPLE && mask==EPBM_SELECT) {
		UIIntVarField *field=(UIIntVarField *)GetFocus();
		if (field && field->GetVariableID()==SIP_SAMPLE) {
			Path sampleLib(SamplePool::GetInstance()->GetSampleLib()) ;
			if (FileSystem::GetInstance()->GetFileType(sampleLib.GetPath().c_str())!=FT_DIR) {
				MessageBox *mb=new MessageBox(*this,"Can't access the samplelib",MBBF_OK) ;
				DoModal(mb) ;
			} else { ;
				ImportSampleDialog *isd=new ImportSampleDialog(*this) ;
				DoModal(isd, ImportSampleDialogCallback) ;
			}
			isDirty_=true;
			return;
		}
		if (field && field->GetVariableID()==SIP_ROOTNOTE) {
			int i=viewData_->currentInstrument_;
			InstrumentBank *bank=viewData_->project_->GetInstrumentBank();
			SampleInstrument *instrument=(SampleInstrument *)bank->GetInstrument(i);
			if (instrument && instrument->AcceptSuggestedRootNote()) {
				isDirty_=true;
				return;
			}
			if (instrument && instrument->DetectRootNoteSuggestionFromTrim()>=0) {
				isDirty_=true;
				return;
			}
		}
	}

	if (viewMode_==VM_NEW) {
		if (mask==EPBM_A) {
			UIIntVarField *field=(UIIntVarField *)GetFocus() ;
			Variable &v=field->GetVariable() ;
			switch(v.GetID()) {
				case SIP_SAMPLE:
				 {
                    // Go to import sample as normal
                    // First check if the samplelib exists

					 Path sampleLib(SamplePool::GetInstance()->GetSampleLib()) ;
					 if (FileSystem::GetInstance()->GetFileType(sampleLib.GetPath().c_str())!=FT_DIR) {
						 MessageBox *mb=new MessageBox(*this,"Can't access the samplelib",MBBF_OK) ;
						 DoModal(mb) ;
					 } else { ;
						// Go to import sample

						 ImportSampleDialog *isd=new ImportSampleDialog(*this) ;
						 DoModal(isd, ImportSampleDialogCallback) ;
					}
					break ;
				 }
				case SIP_TABLE:
				 {
					int next=TableHolder::GetInstance()->GetNext() ;
					if (next!=NO_MORE_TABLE) {
						v.SetInt(next) ;
						isDirty_=true ;
					}
					break ;
                }
                case SIP_PRINTFX: {
                    FxPrinter printer(viewData_);
                    isDirty_ = printer.Run();
                    View::SetNotification(printer.GetNotification());
                    break;
                }
                default:
                    break ;
			}
			mask&=(0xFFFF-EPBM_A) ;
		}
	}

	if (viewMode_==VM_CLONE) {
        if ((mask&EPBM_A)&&(mask&EPBM_L)) {
			UIIntVarField *field=(UIIntVarField *)GetFocus() ;
			mask&=(0xFFFF-EPBM_A) ;
			Variable &v=field->GetVariable() ;
			int current=v.GetInt() ;
			if (current==-1) return ;

			int next=TableHolder::GetInstance()->Clone(current) ;
			if (next!=NO_MORE_TABLE) {
				v.SetInt(next) ;
				isDirty_=true ;
			}
		}
		mask&=(0xFFFF-(EPBM_A|EPBM_L)) ;
	} ;

	if (viewMode_==VM_SELECTION) {
	} else {
		viewMode_=VM_NORMAL ;
	}

	// B and RB combos belong to the screen (switch instrument, cut, go to
	// another screen), never to the focused knob
	if (!(mask&(EPBM_B|EPBM_R))) {
		FieldView::ProcessButtonMask(mask) ;
	}

	// The type field swaps the whole instrument: rebuild before anything
	// else touches the (now replaced) fields.
	if (applyTypeChange()) {
		return ;
	}

    Player *player=Player::GetInstance() ;
	// B Modifier

    if (mask & EPBM_B) {
        // B+A cuts; B+arrows switch instrument. Never both from one press.
        if (mask&EPBM_A) mask&=~(EPBM_LEFT|EPBM_RIGHT|EPBM_UP|EPBM_DOWN) ;
        if (mask&EPBM_LEFT) warpToNext(-1) ;
		if (mask&EPBM_RIGHT) warpToNext(+1);
		if (mask&EPBM_DOWN) warpToNext(+16) ;  // down the list, like the Song
		if (mask&EPBM_UP) warpToNext(-16);
		if (mask&EPBM_A) { // Allow cut instrument
		   if (getInstrumentType()==IT_SAMPLE) {
                UIIntVarField *focusField=(UIIntVarField *)GetFocus() ;
                if (focusField && focusField->GetVariableID()==SIP_SAMPLE) {
	               int i=viewData_->currentInstrument_ ;
	               InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	               I_Instrument *instr=bank->GetInstrument(i) ;
					instr->Purge() ;
//                   Variable *v=instr->FindVariable(SIP_SAMPLE) ;
//                   v->SetInt(-1) ;
                   isDirty_=true ;
                }
           }

		   // Check if on table (fields are paged, so match the variable)
		   UIIntVarField *focusField=(UIIntVarField *)GetFocus();
		   if (focusField && (focusField->GetVariableID()==SIP_TABLE ||
		                      focusField->GetVariableID()==MIP_TABLE)) {
	            int i=viewData_->currentInstrument_ ;
	            InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	            I_Instrument *instr=bank->GetInstrument(i) ;
                Variable *v=instr->FindVariable(SIP_TABLE) ;
                v->SetInt(-1) ;
                isDirty_=true ;
		   } ;
        }
        if (mask&EPBM_L) {
            viewMode_=VM_CLONE ;
        } ;
    } else {

        // A modifier

        if (mask == EPBM_A) {
            FourCC varID = ((UIIntVarField *)GetFocus())->GetVariableID();
            if ((varID == SIP_TABLE) || (varID == MIP_TABLE) ||
                (varID == SIP_PRINTFX)) {
                viewMode_ = VM_NEW;
			}
        } else {

            // R Modifier

            if (mask & EPBM_R) {
                if (mask & EPBM_LEFT) {
                    ViewType vt = VT_PHRASE;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_UP) {
                    InstrumentListDialog *list = new InstrumentListDialog(
                        *this, viewData_->currentInstrument_);
                    DoModal(list, InstrumentListCallback);
                    return;
                }

                if (mask & EPBM_DOWN) {

                    // Go to table view

                    ViewType vt = VT_TABLE2;

                    int i = viewData_->currentInstrument_;
                    InstrumentBank *bank =
                        viewData_->project_->GetInstrumentBank();
                    I_Instrument *instr = bank->GetInstrument(i);
                    int table = instr->GetTable();
                    if (table != VAR_OFF) {
                        viewData_->currentTable_ = table;
                    }
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                // if (mask&EPBM_RIGHT) {

                //	// Go to import sample

                //		ViewType vt=VT_IMPORT ;
                //		ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
                //		SetChanged();
                //		NotifyObservers(&ve) ;
                //}

                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, true,
                                          viewData_->chainRow_);
                }
            } else {
				// No modifier
                if (mask == EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, false,
                                          viewData_->chainRow_);
                }
            }
        }
    }

    UIIntVarField *field = (UIIntVarField *)GetFocus();
    if (field) {
	   lastFocusID_=field->GetVariableID() ;
    }

} ;

// A slot can change engine (sample <-> synth) outside this screen, which
// deletes the instrument our fields point at: rebuild from the new one.
void InstrumentView::syncReplacedInstrument() {
	int i=viewData_->currentInstrument_ ;
	if (i!=currentSlot_) {
		return ;
	}
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	if (bank->GetInstrument(i)==current_) {
		return ;
	}
	current_=0 ;
	ClearFocus() ;
	T_SimpleList<UIField>::Empty() ;
	onInstrumentChange() ;
}

void InstrumentView::DrawView() {

	syncReplacedInstrument() ;

	Clear() ;
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Draw title

    char title[32];
    SetColor(CD_NORMAL);
    const char *kind = "";
    switch (getInstrumentType()) {
        case IT_SAMPLE: kind = " SAMPLE"; break;
        case IT_SYNTH: kind = " SYNTH"; break;
        case IT_MACRO: kind = " MACRO"; break;
        case IT_MIDI: kind = " MIDI"; break;
        default: break;
    }
    sprintf(title, "Instrument %2.2X%s", viewData_->currentInstrument_, kind);
    DrawString(pos._x, pos._y, title, props);

    if (getInstrumentType()==IT_SAMPLE) {
        drawSampleLabVisuals();
    } else if (isSynthLike(getInstrumentType())) {
        drawSynthVisuals();
    }
    if (ultraCompactLayout_) {
        SetColor(CD_MUTE);
        DrawString(12,26,"RB+Up: all sounds",props);
        SetColor(CD_NORMAL);
    }

    FieldView::Redraw();
    drawMap() ;
} ;

void InstrumentView::OnFocus() { onInstrumentChange(); }

void InstrumentView::GetGuideTopic(const char *&page, const char *&section) {
	// Samples have a page of their own (packs, trimming, root, slices)
	if (getInstrumentType()==IT_SAMPLE) {
		page="samples";
		section="Sample pages";
	} else if (getInstrumentType()==IT_MACRO) {
		// Its own page; the shared pages are explained there too
		page="macro";
		switch(labPage_) {
			case 0: section="SOUND - shape and knobs"; break;
			case 3: section="MOTION - movement"; break;
			default: section="The other pages"; break;
		}
	} else {
		page="synth";
		section="";
	}
}

void InstrumentView::CustomizeContextOverlay(const char *&name, const char *&where,
                                             const char *&edit, const char *&field,
                                             const char *&cmd1, const char *&cmd2,
                                             const char *&cmd3, const char *&cmd4,
                                             const char *&cmd5, const char *&cmd6,
                                             const char *&cmd7) {
	if (isSynthLike(getInstrumentType())) {
		customizeSynthOverlay(name,where,edit,field,cmd1,cmd2,cmd3,cmd4,cmd5,cmd6,cmd7);
		return;
	}
	if (getInstrumentType()!=IT_SAMPLE) {
		return;
	}
	if (labPage_==0) {
		name="INSTR SOURCE";
	} else if (labPage_==1) {
		name="INSTR SHAPE";
	} else if (labPage_==2) {
		name="INSTR FILTER";
	} else if (labPage_==3) {
		name="INSTR LOOP";
	} else if (labPage_==INSTRUMENT_MOD_PAGE) {
		name="INSTR MOD";
	} else {
		name="INSTR MOTION";
	}
	where="RB+Left Phrase";
	cmd1="Dpad choose field";
	cmd3="A+Dpad edit value";
	cmd4="LB+Left/Right page";
	cmd5="A+Start hear  B+A reset";
	cmd6="RB+Left Phrase";
	cmd7="RB+Select close";
	if (labPage_==0) {
		field="Source: sample/root";
		edit="Sel sample/root";
		cmd1="Dpad choose sample/root";
		cmd2="Sel on sample: import";
		cmd3="Sel on root: suggest/use";
		cmd4="LB+UD pick Start/Loop/End";
		cmd5="LB+A+LR nudge marker";
		cmd6="A+Start hear it";
		cmd7="RB+Start once/loop";
	} else if (labPage_==1) {
		field="Shape: level/pan/grit";
		edit="A+Dpad changes values";
		cmd1="Dpad choose value";
		cmd2="A+LR coarse/fine edit";
		cmd3="A+UD larger changes";
		cmd4="LB+Left/Right page";
		cmd5="A+Start hear  B+A reset";
	} else if (labPage_==2) {
		field="Filter: cutoff/reso";
		edit="Tone shaping";
		cmd1="Dpad choose filter row";
		cmd2="A+Dpad edit value";
		cmd3="Cut/res/type/mode";
		cmd4="LB+Left/Right page";
		cmd5="A+Start hear  B+A reset";
	} else if (labPage_==3) {
		field="Loop: trim window";
		edit="S=start L=loop E=end";
		cmd1="LB+UD choose Start/Loop/End";
		cmd2="LB+A+LR nudge mark";
		cmd3="Dpad to start/lstart/end";
		cmd4="A+Dpad exact values";
		cmd5="Sel root from trim";
		cmd6="A+Start hear it";
		cmd7="RB+Start once/loop";
	} else if (labPage_==INSTRUMENT_MOD_PAGE) {
		field="2 envelopes/LFOs";
		edit="A+Dpad edit value";
		cmd1="type: decay/swell or LFO";
		cmd2="dest: what it moves";
		cmd3="amt: how far (+/-)";
		cmd4="rate: higher = faster";
		cmd5="LB+Left/Right page";
		// The focused setting explained
		UIIntVarField *focused=(UIIntVarField *)GetFocus();
		if (focused && isModField(focused->GetVariableID())) {
			static char help1[40],help2[40],value[40];
			int i=viewData_->currentInstrument_;
			I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(i);
			getModFieldHelp(focused->GetVariableID(),instr,help1,help2,value);
			where=help1;
			edit=help2;
		}
	} else {
		field="Motion: table/fb";
		edit="Automation source";
		cmd1="Dpad choose value";
		cmd2="A+Dpad edit value";
		cmd3="A on table: make/open";
		cmd4="RB+Down inst table";
		cmd5="LB+Left/Right page";
	}
}

void InstrumentView::Update(Observable &o,I_ObservableData *d) {
	onInstrumentChange() ;
	isDirty_=true;
}
