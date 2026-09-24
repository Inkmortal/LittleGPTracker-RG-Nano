#ifndef _MESSAGE_BOX_H_
#define _MESSAGE_BOX_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>

#ifdef MessageBox
#undef MessageBox
#endif

enum MessageBoxList {
	MBL_OK=0,
	MBL_YES,
	MBL_CANCEL,
	MBL_NO,
	MBL_LAST
} ; 

enum MessageBoxButtonFlag {
	MBBF_OK=1,
	MBBF_YES=2,
	MBBF_CANCEL=4,
	MBBF_NO=8
} ;

class MessageBox:public ModalView {
public:
	MessageBox(View &view,const char *message,int btnFlags=MBBF_OK) ;
	virtual ~MessageBox() ;

	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) ;
	virtual void OnFocus() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	// Start on this button (default: the last one, e.g. No)
	void SelectButton(int button) ;
	virtual void CustomizeContextOverlay(const char *&name, const char *&where,
										 const char *&edit, const char *&field,
										 const char *&cmd1, const char *&cmd2,
										 const char *&cmd3, const char *&cmd4,
										 const char *&cmd5, const char *&cmd6,
										 const char *&cmd7) ;
private:
	std::string message_ ;	
	int button_[4] ;
	int buttonCount_ ;
	int selected_ ;
} ;
#endif
