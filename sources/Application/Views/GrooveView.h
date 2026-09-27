#ifndef _GROOVE_VIEW_H_
#define _GROOVE_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"

class GrooveView: public View {
public:
	GrooveView(GUIWindow &w,ViewData *viewData) ;
	~GrooveView() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int tick=0) ;
	virtual void OnFocus() ;
	// X copies the whole groove, Y pastes it here, LB+Y duplicates this
	// groove into the next unused one
	virtual void CopyAtCursor() ;
	virtual void PasteAtCursor(bool fresh) ;

protected:
	void updateCursorValue(int val,bool sync=false) ;
	void updateCursor(int dir) ;
	void initCursorValue() ;
	void clearCursorValue() ;
	void warpGroove(int dir) ;
	void processNormalButtonMask(unsigned short mask) ;
	void processSelectionButtonMask(unsigned short mask) ;
	void drawTimingRuler(int playEntry) ;

private:
	int position_ ;
	int lastPosition_ ;
	unsigned char clipboard_[16] ;
	bool haveClipboard_ ;
} ;
#endif