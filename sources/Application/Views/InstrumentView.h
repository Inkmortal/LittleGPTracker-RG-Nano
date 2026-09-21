#ifndef _INSTRUMENT_VIEW_H_
#define _INSTRUMENT_VIEW_H_

#include "Application/FX/FxPrinter.h"
#include "BaseClasses/FieldView.h"
#include "Foundation/Observable.h"
#include "ViewData.h"

// View-owned "type" field (sample/synth) shown first on page 1
#define INSTRUMENT_TYPE_FIELD MAKE_FOURCC('I','T','Y','P')

class InstrumentView: public FieldView, public I_Observer {
public:
	InstrumentView(GUIWindow &w,ViewData *data) ;
	virtual ~InstrumentView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType,unsigned int) {} ;
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
	void customizeSynthOverlay(const char *&name, const char *&where,
	                           const char *&edit, const char *&field,
	                           const char *&cmd1, const char *&cmd2,
	                           const char *&cmd3, const char *&cmd4,
	                           const char *&cmd5, const char *&cmd6,
	                           const char *&cmd7) ;
	InstrumentType getInstrumentType() ;
	void drawSampleLabVisuals() ;
	void drawLabText(int x, int y, const char *text, GUITextProperties &props) ;
	void drawLabBar(int x, int y, int width, int value, int maxValue) ;
	void drawPixelLabBar(int x, int y, int width, int height, int value, int maxValue,
	                     bool bipolar=false) ;
	void drawSampleWaveform(class SampleInstrument *instrument, int x, int y,
	                        int width, int height, bool showMarkers) ;
	void drawMarkerLine(int x, int y, int height, ColorDefinition color, FourCC marker) ;
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
