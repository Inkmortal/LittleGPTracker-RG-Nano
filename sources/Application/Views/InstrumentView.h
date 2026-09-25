#ifndef _INSTRUMENT_VIEW_H_
#define _INSTRUMENT_VIEW_H_

#include "Application/FX/FxPrinter.h"
#include "BaseClasses/FieldView.h"
#include "Foundation/Observable.h"
#include "ViewData.h"

// View-owned "type" field (sample/synth) shown first on page 1
#define INSTRUMENT_TYPE_FIELD MAKE_FOURCC('I','T','Y','P')
// Pages per instrument (sample and synth both have six, MOD is the 5th)
#define INSTRUMENT_PAGE_COUNT 6
#define INSTRUMENT_MOD_PAGE 4

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
	void drawModPlot(I_Instrument *instr, int bx, int by, int bw, int bh) ;
	void customizeSynthOverlay(const char *&name, const char *&where,
	                           const char *&edit, const char *&field,
	                           const char *&cmd1, const char *&cmd2,
	                           const char *&cmd3, const char *&cmd4,
	                           const char *&cmd5, const char *&cmd6,
	                           const char *&cmd7) ;
	InstrumentType getInstrumentType() ;
public:
	virtual void GetGuideTopic(const char *&page, const char *&section) ;
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
} ;
#endif
