#ifndef _SCALE_VIEW_H_
#define _SCALE_VIEW_H_

#include "BaseClasses/FieldView.h"
#include "ViewData.h"

// The song's Key and Scale over a piano octave, like the M8's Scale view:
// the notes of the scale are lit, the key (root) in amber. On the keyboard
// A puts a note in or out of the scale, which turns it into the song's own
// Custom scale. RB+Right from Project.
class ScaleView: public FieldView {
public:
	ScaleView(GUIWindow &w,ViewData *data) ;
	virtual ~ScaleView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType,unsigned int) {} ;
	virtual void OnFocus() ;

	// For the simulator: the cursor is on the keyboard, at this note (C=0)
	bool OnKeyboard() { return onKeys_ ; } ;
	int KeyCursor() { return keyCursor_ ; } ;

protected:
	virtual void drawGraphics() ;

private:
	// Put the note (0 = C .. 11 = B) in or out of the scale
	void setNote(int note,bool in) ;
	void hearNote(int note) ;
	int mask() ;  // the scale's notes, bit n = n semitones above the key

	bool onKeys_ ;   // cursor on the keyboard instead of Key / Scale
	int keyCursor_ ; // 0 = C .. 11 = B
} ;
#endif
