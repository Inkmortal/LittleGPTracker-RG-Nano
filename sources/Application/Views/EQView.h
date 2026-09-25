#ifndef _EQ_VIEW_H_
#define _EQ_VIEW_H_

#include "BaseClasses/FieldView.h"
#include "ViewData.h"

// The master EQ: low shelf, mid bell, high shelf, each with gain and
// frequency, over a drawing of the resulting curve. RB+Right from FX.
class EQView: public FieldView {
public:
	EQView(GUIWindow &w,ViewData *data) ;
	virtual ~EQView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType,unsigned int) {} ;
	virtual void OnFocus() {} ;

protected:
	virtual void drawGraphics() ;

private:
	void params(int out[6]) ;
} ;
#endif
