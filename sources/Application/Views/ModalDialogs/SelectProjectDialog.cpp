
#include "SelectProjectDialog.h"
#include "NewProjectDialog.h"
#include "System/Console/Trace.h"
#include "Application/Views/ModalDialogs/MessageBox.h"
#include "Application/Model/Project.h"
#include "GuideDialog.h"

#include <algorithm>

#define LIST_WIDTH 26
#define LIST_SIZE 16
#define LIST_Y 2
#define WINDOW_HEIGHT (LIST_Y + LIST_SIZE + 5)
#define BUTTON_Y (LIST_Y + LIST_SIZE + 1)

// Quitting lives on the MENU/Power key, which leaves room to space these out
enum ProjectAction { PA_OPEN = 0, PA_NEW, PA_DELETE, PA_HELP, PA_COUNT };
static const char *buttonText[PA_COUNT] = {"Open", "New", "Delete", "Help"};
static const int buttonX[PA_COUNT] = {0, 7, 13, 22};
static const char *buttonHint[PA_COUNT] = {
    "A: open this song", "A: make a new song", "A: delete this song",
    "A: the full guide"};

static bool isProjectFolder(const std::string &name) {
    std::string prefix = name.substr(0, 4);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
    return prefix == "lgpt";
}

Path SelectProjectDialog::lastFolder_("root:") ;
int SelectProjectDialog::lastProject_ = 0 ;

static void NewProjectCallback(View &v,ModalView &dialog) {

	NewProjectDialog &npd=(NewProjectDialog &)dialog ;
	if (dialog.GetReturnCode()>0) {
		std::string selected=npd.GetName() ;
		SelectProjectDialog &spd=(SelectProjectDialog&)v ;
		Result result = spd.OnNewProject(selected) ;
        if (result.Failed()) {
            Trace::Error(result.GetDescription().c_str());
        }
    }
}

static void DeleteProjectCallback(View &v, ModalView &dialog) {
    SelectProjectDialog &spd = (SelectProjectDialog&) v;
	if (dialog.GetReturnCode() == MBL_YES) {
        Path projectPath = spd.GetCurrentProjectPath();
        Result result = spd.OnDeleteProject(projectPath);
		if (result.Failed()) {
			Trace::Error(result.GetDescription().c_str());
		}
	}
}

// Recursive helper to delete directory and all contents
void SelectProjectDialog::DeleteFolder(const Path &dirPath) {
	FileSystem *fs = FileSystem::GetInstance();
	FileType type = fs->GetFileType(dirPath.GetPath().c_str());
	
	if (type == FT_DIR) {
		I_Dir *dir = fs->Open(dirPath.GetPath().c_str());
		if (dir) {
			dir->GetContent("*");
			
			// Collect all items first to avoid iterator invalidation
			T_SimpleList<Path> itemsToDelete(false);
			IteratorPtr<Path> it(dir->GetIterator());
			
			for (it->Begin(); !it->IsDone(); it->Next()) {
				Path itemCopy = it->CurrentItem();
				std::string name = itemCopy.GetName();
				
				// Skip . and .. entries
				if (name != "." && name != "..") {
					Path *ptrCopy = new Path(itemCopy);
					itemsToDelete.Insert(ptrCopy);
				}
			}
			
			delete dir;
			
			// Now delete all collected items
			IteratorPtr<Path> deleteIt(itemsToDelete.GetIterator());
			for (deleteIt->Begin(); !deleteIt->IsDone(); deleteIt->Next()) {
				const Path &item = deleteIt->CurrentItem();
				DeleteFolder(item);
			}
		}
	}

    fs->Delete(dirPath.GetPath().c_str());
}

SelectProjectDialog::SelectProjectDialog(View &view)
    : ModalView(view), content_(true) {}

SelectProjectDialog::~SelectProjectDialog() {
}

void SelectProjectDialog::DrawView() {

	SetWindow(LIST_WIDTH,WINDOW_HEIGHT) ;

	GUITextProperties props ;
    View::EnableNotification();

	SetColor(CD_HILITE1) ;
	DrawString((LIST_WIDTH-10)/2,0,"YOUR SONGS",props) ;

    if (currentProject_ < topIndex_) {
        topIndex_ = currentProject_;
    };
    if (currentProject_>=topIndex_+LIST_SIZE) {
		topIndex_=currentProject_-LIST_SIZE+1 ;
	} ;

	int y=LIST_Y ;
	if (content_.Size()==0) {
		SetColor(CD_NORMAL) ;
		DrawString(1,y,"No songs yet.",props) ;
		DrawString(1,y+1,"Pick New to start one.",props) ;
	}

	IteratorPtr<Path> it(content_.GetIterator()) ;
	int count=0 ;
	char buffer[LIST_WIDTH+1] ;
	for(it->Begin();!it->IsDone();it->Next()) {
		if ((count>=topIndex_)&&(count<topIndex_+LIST_SIZE)) {
			std::string p=it->CurrentItem().GetName() ;
			if (isProjectFolder(p) && p.size()>4) {
				// hide the lgpt_ prefix
				int namestart = isalnum(p[4]) ? 4 : 5;
				p=" "+p.substr(namestart) ;
			} else {
				p="["+p+"]" ;
			}
			bool on=(count==currentProject_) ;
			SetColor(on?CD_HILITE2:CD_NORMAL) ;
			props.invert_=on ;
			// pad the selected row so the bar spans the list
			snprintf(buffer,sizeof(buffer),"%-*s",LIST_WIDTH,p.c_str()) ;
			buffer[LIST_WIDTH]=0 ;
			if (!on) buffer[p.size()<LIST_WIDTH?p.size():LIST_WIDTH]=0 ;
			DrawString(0,y,buffer,props) ;
			y+=1 ;
		}
		count++ ;
	} ;
	props.invert_=false ;

	for (int i=0;i<PA_COUNT;i++) {
		bool on=(i==selected_) ;
		SetColor(on?CD_CURSOR:CD_HILITE1) ;
		props.invert_=on ;
		DrawString(buttonX[i],BUTTON_Y,buttonText[i],props) ;
	}
	props.invert_=false ;

	SetColor(CD_MUTE) ;
	DrawString(0,BUTTON_Y+2,"                          ",props) ;
	DrawString(0,BUTTON_Y+2,buttonHint[selected_],props) ;
	DrawString(0,BUTTON_Y+3,"L/R pick  RB+SEL helper",props) ;

	// Version string below the border
	char buildString[80];
	sprintf(buildString, "Piggy build %s.%s.%s", PROJECT_NUMBER, PROJECT_RELEASE, BUILD_COUNT);
	SetColor(CD_MUTE) ;
	DrawString((LIST_WIDTH - (int)strlen(buildString)) / 2, WINDOW_HEIGHT + 2, buildString, props);
	SetColor(CD_NORMAL) ;
};

void SelectProjectDialog::OnPlayerUpdate(PlayerEventType,
                                         unsigned int currentTick) {};

void SelectProjectDialog::OnFocus() {

	setCurrentFolder(lastFolder_) ;
    currentProject_ = lastProject_;
};

void SelectProjectDialog::GetGuideTopic(const char *&page, const char *&section) {
	page="first-song";
	section="1. The start screen";
}

void SelectProjectDialog::CustomizeContextOverlay(
	const char *&name, const char *&where, const char *&edit,
	const char *&field, const char *&cmd1, const char *&cmd2,
	const char *&cmd3, const char *&cmd4, const char *&cmd5,
	const char *&cmd6, const char *&cmd7) {
	name="PROJECTS";
	where="Open New Delete Help";
	edit="A runs the button";
	field="Choose/create song";
	cmd1="Up/Down choose song";
	cmd2="B+Up/Dn page list";
	cmd3="Left/Right button";
	cmd4="A run button";
	cmd5="Delete asks first";
	cmd6="Help: how to use app";
	cmd7="RB+Select helper";
}

void SelectProjectDialog::askDelete() {
    Path current = GetCurrentProjectPath();
    std::string name = current.GetName();
    if (content_.Size() == 0 || !isProjectFolder(name)) {
        View::SetNotification("Pick a song to delete", 0);
        return;
    }
    std::string shown = name.size() > 5 ? name.substr(isalnum(name[4]) ? 4 : 5) : name;
    std::string message = "Delete " + shown + " ?";
    MessageBox *mb = new MessageBox(*this, message.c_str(), MBBF_YES | MBBF_NO);
    DoModal(mb, DeleteProjectCallback);
}

void SelectProjectDialog::runAction() {
    switch (selected_) {
    case PA_OPEN: {
        if (content_.Size() == 0) {
            View::SetNotification("No songs yet: pick New", 0);
            break;
        }
        Path current = GetCurrentProjectPath();
        if (isProjectFolder(current.GetName())) {
            selection_ = current;
            lastFolder_ = currentPath_;
            lastProject_ = currentProject_;
            EndModal(1);
        } else if (current.GetName() == "..") {
            Path parent = currentPath_.GetParent();
            setCurrentFolder(parent);
        } else {
            setCurrentFolder(current);
        }
        break;
    }
    case PA_NEW: {
        NewProjectDialog *npd = new NewProjectDialog(*this, currentPath_);
        DoModal(npd, NewProjectCallback);
        break;
    }
    case PA_DELETE:
        askDelete();
        break;
    case PA_HELP:
        DoModal(new GuideDialog(*this));
        break;
    }
}

void SelectProjectDialog::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;

    if (mask & EPBM_B) {
        // A+B is the quick delete shortcut
        if (mask & EPBM_A) {
            askDelete();
            return;
        }
        if (mask & EPBM_UP)
            warpToNextProject(-LIST_SIZE);
        if (mask & EPBM_DOWN)
            warpToNextProject(LIST_SIZE);
        return;
    }
    if (mask == EPBM_A) {
        runAction();
        return;
    }
    if (mask == EPBM_UP) warpToNextProject(-1);
    if (mask == EPBM_DOWN) warpToNextProject(1);
    if (mask == EPBM_LEFT) {
        selected_ = (selected_ + PA_COUNT - 1) % PA_COUNT;
        isDirty_ = true;
    }
    if (mask == EPBM_RIGHT) {
        selected_ = (selected_ + 1) % PA_COUNT;
        isDirty_ = true;
    }
};

void SelectProjectDialog::warpToNextProject(int amount) {

    int offset = currentProject_ - topIndex_;
    int size = content_.Size();
    if (size == 0)
        return;
    currentProject_+=amount ;
	if (currentProject_<0) currentProject_+=size ;
	if (currentProject_>=size) currentProject_-=size ;

	if ((amount>1)||(amount<-1)) {
		topIndex_=currentProject_-offset ;
		if (topIndex_<0) {
			topIndex_=0 ;
		} ;
	}
    isDirty_ = true;
}

Path SelectProjectDialog::GetSelection() {
	return selection_ ;
}

Result SelectProjectDialog::OnNewProject(std::string &name) {

    Path path = currentPath_.Descend(name);
    if (path.Exists()) {
        Trace::Log("SelectProjectDialog:OnNewProj","path already exists %s", path.GetPath().c_str());
		std::string res("Name " + name + " busy");
		View::SetNotification(res.c_str(), 0);
        return Result(res);
    }
    Trace::Log("TMP","creating project at %s",path.GetPath().c_str());
	selection_ = path ;
	Result result = FileSystem::GetInstance()->MakeDir(path.GetPath().c_str()) ;
	RETURN_IF_FAILED(result, ("Failed to create project dir for '%s", path.GetPath().c_str()));

	path = path.Descend("samples");
	Trace::Log("TMP","creating samples dir at %s",path.GetPath().c_str());
	result = FileSystem::GetInstance()->MakeDir(path.GetPath().c_str()) ;
	RETURN_IF_FAILED(result, ("Failed to create samples dir for '%s'", path.GetPath().c_str()));

	EndModal(1) ;
  return Result::NoError;
} ;

Result SelectProjectDialog::OnDeleteProject(const Path &projectPath) {

    Trace::Log("SelectProjectDialog:OnDelProj","deleting project at %s", projectPath.GetPath().c_str());
	
	// Make a non-const copy to check existence
	Path pathCopy = projectPath;
	
	// Check if project exists before deletion
    if (!pathCopy.Exists()) {
        std::string errMsg = "Project not found";
        View::SetNotification(errMsg.c_str(), 0);
        return Result("Project not found");
    }

    // Recursively delete the project directory and all contents
	DeleteFolder(projectPath);
	
	// Project deleted successfully, refresh the project list
	std::string successMsg = "Project deleted: " + pathCopy.GetName();
	View::SetNotification(successMsg.c_str(), 0);
	
	// Refresh current folder to update the list while preserving position
	int savedProject = currentProject_;
	int savedTopIndex = topIndex_;
	int savedButton = selected_;
	Path currentPathCopy = currentPath_;
	setCurrentFolder(currentPathCopy);
	
	// Restore position (adjust if necessary if we're at the end of list)
	int listSize = content_.Size();
	if (savedProject >= listSize && listSize > 0) {
		currentProject_ = listSize - 1;
	} else if (listSize > 0) {
		currentProject_ = savedProject;
	}
    topIndex_ = savedTopIndex;
    if (listSize > 0)
        selected_ = savedButton;
    isDirty_ = true;
		
	return Result::NoError;
};

//copy-paste-mutilate'd from ImportSampleDialog
void SelectProjectDialog::setCurrentFolder(Path &path) {

	//get ready
	selected_=0 ;
	currentPath_=path ;
	content_.Empty() ;
	
	// Let's read all the directory in the root

	bool atRoot=currentPath_.GetPath()==Path("root:").GetPath() ;
	I_Dir *dir=FileSystem::GetInstance()->Open(currentPath_.GetPath().c_str()) ;

  if (dir) 
  {

		// Get all lgpt something

		dir->GetContent("*");
    dir->Sort();
      
		IteratorPtr<Path> it(dir->GetIterator()) ;
		for(it->Begin();!it->IsDone();it->Next())
    {
			Path &path=it->CurrentItem() ;

			if (path.IsDirectory())
      {
				std::string name=path.GetName() ;
				// ".." only below the songs folder: there is nothing above it to open
				bool parent = name == "..";
				if ((name[0] != '.' && !parent) || (parent && !atRoot))
        {
					Path *p=new Path(path) ;
					content_.Insert(p) ;
				}
      }
		}
		delete (dir) ;
  }

	//reset & redraw screen
	topIndex_=0 ;
	currentProject_=0 ;
	if (content_.Size()==0) selected_=PA_NEW ;
    isDirty_ = true;
}

Path SelectProjectDialog::GetCurrentProjectPath() {
	int count = 0;
	IteratorPtr<Path> it(content_.GetIterator());
	for (it->Begin(); !it->IsDone(); it->Next()) {
		if (count == currentProject_) {
			return it->CurrentItem();
		}
		count++;
	}
	return Path();
}
