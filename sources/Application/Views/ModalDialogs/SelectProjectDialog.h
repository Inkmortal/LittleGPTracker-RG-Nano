#ifndef _SELECT_PROJECT_DIALOG_H_
#define _SELECT_PROJECT_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Errors/Result.h"

class SelectProjectDialog:public ModalView {
public:
	SelectProjectDialog(View &view) ;
	~SelectProjectDialog() ;

	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) ;
	virtual void OnFocus() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void CustomizeContextOverlay(const char *&name, const char *&where,
										 const char *&edit, const char *&field,
										 const char *&cmd1, const char *&cmd2,
										 const char *&cmd3, const char *&cmd4,
										 const char *&cmd5, const char *&cmd6,
										 const char *&cmd7) ;

	Result OnNewProject(std::string &name) ;
    Result OnDeleteProject(const Path &projectPath);

    // Removes a folder and everything in it
    static void DeleteFolder(const Path &dirPath);

    Path GetSelection();
    Path GetCurrentProjectPath();

  protected:
    void warpToNextProject(int amount) ;
	void setCurrentFolder(Path &path) ;
	void runAction() ;
	void askDelete() ;

private:
  T_SimpleList<Path> content_;
  int topIndex_;
  int currentProject_;
  int selected_;
  Path currentPath_;
  Path selection_;
  static Path lastFolder_;
  static int lastProject_;
} ;

#endif
