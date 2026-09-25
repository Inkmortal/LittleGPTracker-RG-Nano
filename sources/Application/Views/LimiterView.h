#ifndef _LIMITER_VIEW_H_
#define _LIMITER_VIEW_H_

#include "BaseClasses/FieldView.h"
#include "ViewData.h"

// The master limiter (like the M8's LIM and its scope): drive, ceiling,
// attack (look-ahead) and release, a live gain-reduction meter and a scope
// of the mix level (from the bottom) and the limiting (from the top).
// RB+Right from EQ.
class LimiterView: public FieldView {
public:
	LimiterView(GUIWindow &w,ViewData *data) ;
	virtual ~LimiterView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType,unsigned int) ;
	virtual void OnFocus() {} ;

protected:
	virtual void drawGraphics() ;

private:
	int param(int index) ;
	void drawReadout() ;
} ;
#endif
