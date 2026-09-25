#ifndef _IMPORT_SAMPLE_DIALOG_H_
#define _IMPORT_SAMPLE_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Foundation/T_SimpleList.h"
#include "System/FileSystem/FileSystem.h"
#include <string>

class ImportSampleDialog:public ModalView {
public:
	ImportSampleDialog(View &view) ;
	virtual ~ImportSampleDialog() ;

	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) ;
	virtual void OnFocus() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void GetGuideTopic(const char *&page,const char *&section) {
		page="samples" ; section="Turn a slot into a sampler" ;
	}
	virtual void CustomizeContextOverlay(const char *&name, const char *&where,
										 const char *&edit, const char *&field,
										 const char *&cmd1, const char *&cmd2,
										 const char *&cmd3, const char *&cmd4,
										 const char *&cmd5, const char *&cmd6,
										 const char *&cmd7) ;

protected:
	void setCurrentFolder(Path *path) ;
	void warpToNextSample(int dir) ;
	void import(Path &element) ;
	void preview(Path &element) ;
	void endPreview() ;
	void setImportStatus(int note) ;
	void drawSelectedInfo(Path *element, int total) ;
	long getFileSizeKb(Path &element) ;
private:
	Path *getImportElement();
	bool isSampleLibRoot();
	void setCurrent(Path *element, unsigned short mask);
	T_SimpleList<Path> sampleList_ ;
	int currentSample_ ;
	int topIndex_ ;
	int toInstr_ ;
	int selected_ ;
	char importStatus_[40] ;
	static bool initStatic_ ;
	static Path sampleLib_ ;
	static Path currentPath_ ;

} ;


#endif

