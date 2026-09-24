#ifndef _FX_VIEW_H_
#define _FX_VIEW_H_

#include "BaseClasses/FieldView.h"
#include "ViewData.h"

// Send effects shared by every instrument: chorus, echo and reverb, each
// with its knobs and a picture of what they do. Reached from the Mixer.
class FXView: public FieldView {
public:
	FXView(GUIWindow &w,ViewData *data) ;
	virtual ~FXView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType,unsigned int) {} ;
	virtual void OnFocus() {} ;

protected:
	virtual void drawGraphics() ;

private:
	int paramValue(FourCC id) ;
	int sendCount(FourCC sampleId,FourCC synthId) ;
	void drawSection(int row,const char *name,const char *units,int users) ;
	void drawChorusPlot(int y,int h) ;
	void drawEchoPlot(int y,int h) ;
	void drawReverbPlot(int y,int h) ;
} ;
#endif
