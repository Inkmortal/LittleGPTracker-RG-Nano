#ifndef _INSTRUMENT_VIEW_H_
#define _INSTRUMENT_VIEW_H_

#include "Application/FX/FxPrinter.h"
#include "BaseClasses/FieldView.h"
#include "Foundation/Observable.h"
#include "ViewData.h"

// View-owned "type" field (sample/synth) shown first on page 1
#define INSTRUMENT_TYPE_FIELD MAKE_FOURCC('I','T','Y','P')
// Pages per instrument (sample and synth both have seven: MOD is the 5th,
// EQ the last)
#define INSTRUMENT_PAGE_COUNT 7
#define INSTRUMENT_MOD_PAGE 4
#define INSTRUMENT_EQ_PAGE 6
// View-owned "slot" field (1..4) on the MOD page
#define INSTRUMENT_MOD_SLOT_FIELD MAKE_FOURCC('I','M','S','L')

class InstrumentView: public FieldView, public I_Observer {
public:
	InstrumentView(GUIWindow &w,ViewData *data) ;
	virtual ~InstrumentView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	// Passes updates on to an open dialog (the instrument list shows what
	// is playing)
	virtual void OnPlayerUpdate(PlayerEventType type,unsigned int tick) {
		View::OnPlayerUpdate(type,tick) ;
	} ;
	virtual void OnFocus() ;
	virtual void CustomizeContextOverlay(const char *&name, const char *&where,
	                                     const char *&edit, const char *&field,
	                                     const char *&cmd1, const char *&cmd2,
	                                     const char *&cmd3, const char *&cmd4,
	                                     const char *&cmd5, const char *&cmd6,
	                                     const char *&cmd7) ;

protected:
	void warpToNext(int offset) ;
	void switchLabPage(int offset) ;
	void onInstrumentChange() ;
	void syncReplacedInstrument() ;
	void fillSampleParameters() ;
	void fillSampleSourcePage(class SampleInstrument *instrument, GUIPoint position) ;
	void fillSampleShapePage(class SampleInstrument *instrument, GUIPoint position) ;
	void fillSampleFilterPage(class SampleInstrument *instrument, GUIPoint position) ;
	void fillSampleLoopPage(class SampleInstrument *instrument, GUIPoint position) ;
	void fillSampleMotionPage(class SampleInstrument *instrument, GUIPoint position) ;
	void fillMidiParameters() ;
	void fillSynthParameters() ;
	void addTypeField(GUIPoint &position) ;
	bool applyTypeChange() ;
	void drawSynthVisuals() ;
	const char *getSynthPageName() ;
	void getSynthFieldHelp(FourCC id, I_Instrument *s, char *line1, char *line2, char *value) ;
	void auditionSynth(int offset) ;
	void fillModPage(I_Instrument *instr, GUIPoint position) ;
	bool isModField(FourCC id) ;
	void getModFieldHelp(FourCC id, I_Instrument *instr, char *line1, char *line2, char *value) ;
	void drawModPage(I_Instrument *instr) ;
	void drawModCurve(I_Instrument *instr, int slot, int x, int y, int w, int h, bool big) ;
	// MOD page keys and edits: slot switching, type defaults, B+A
	bool processModKeys(unsigned short mask) ;
	bool syncModPage(int instrumentBefore, int slotBefore, int typeBefore) ;
	bool resetModField() ;
	void refreshStaleModFields() ;
	void selectModSlot(int slot) ;
	int currentModType() ;
	void customizeModOverlay(const char *&field, const char *&where, const char *&edit,
	                         const char *&cmd1, const char *&cmd2, const char *&cmd3,
	                         const char *&cmd4, const char *&cmd5, const char *&cmd6,
	                         const char *&cmd7) ;
	void fillEQPage(I_Instrument *instr, GUIPoint position) ;
	bool isEQField(FourCC id) ;
	void getEQFieldHelp(FourCC id, I_Instrument *instr, char *line1, char *line2, char *value) ;
	void drawEQPlot(I_Instrument *instr, int bx, int by, int bw, int bh) ;
	void customizeSynthOverlay(const char *&name, const char *&where,
	                           const char *&edit, const char *&field,
	                           const char *&cmd1, const char *&cmd2,
	                           const char *&cmd3, const char *&cmd4,
	                           const char *&cmd5, const char *&cmd6,
	                           const char *&cmd7) ;
	InstrumentType getInstrumentType() ;
	// Synth engines (FM4 / HYPER / WAV): their own SOUND page, help and
	// picture (InstrumentViewEngines.cpp)
	void fillEngineSoundPage(class SynthInstrument *s, GUIPoint position, int engine) ;
	bool getEngineFieldHelp(FourCC id, I_Instrument *s, char *line1, char *line2, char *value) ;
	void drawEngineSoundVisual(class SynthInstrument *s, int bx, int by, int bw, int bh) ;
	void customizeEngineOverlay(int engine, const char *&name, const char *&field,
	                            const char *&cmd1, const char *&cmd2, const char *&cmd3) ;
	const char *getEngineGuideSection() ;
	// The engine knob changed (here, by undo or by a sim script): rebuild
	void syncSynthEngine() ;
public:
	virtual void GetGuideTopic(const char *&page, const char *&section) ;
	virtual void CustomizeHowToSteps(const char **lines) ;
	void OpenInstrument(int instrument) ;
protected:
	void drawSampleLabVisuals() ;
	void drawLabText(int x, int y, const char *text, GUITextProperties &props) ;
	void drawLabBar(int x, int y, int width, int value, int maxValue) ;
	void drawPixelLabBar(int x, int y, int width, int height, int value, int maxValue,
	                     bool bipolar=false) ;
	void drawSampleWaveform(class SampleInstrument *instrument, int x, int y,
	                        int width, int height, bool showMarkers) ;
	void drawMarkerLine(int x, int y, int height, ColorDefinition color, FourCC marker) ;
	// Under the waveform: the way a note travels for the play mode (first
	// pass, then the loop), arrows pointing the direction
	void drawPlayPath(class SampleInstrument *instrument, int x, int y, int width) ;
	void openSampleEditor() ;
	void normalizeWaveMarkers(class SampleInstrument *instrument, FourCC changedMarker) ;
	void cycleWaveMarker(int offset) ;
	void nudgeWaveMarker(int offset, int multiplier=1) ;
	void auditionSamplePitch(int offset) ;
	void toggleSamplePreviewLoop() ;
	const char *getWaveMarkerName() ;
	const char *getWaveMarkerShortName() ;
	bool isWaveMarkerPage() ;
	const char *getLabPageName() ;
	void Update(Observable &o,I_ObservableData *d) ;

private:
	Project *project_ ;
	FourCC lastFocusID_ ;
	I_Instrument *current_ ;
	int labPage_ ;
	FourCC markerFocus_ ;
	bool previewLoop_ ;
	int currentSlot_ ;
	Variable *typeVar_ ;
	int shownEngine_ ;   // synth engine the fields were built for (-1: none)
	// MOD page: the slot shown, and the type its fields were built for
	Variable *modSlotVar_ ;
	int modFieldsType_ ;
	int modFieldsSlot_ ;
} ;
#endif
