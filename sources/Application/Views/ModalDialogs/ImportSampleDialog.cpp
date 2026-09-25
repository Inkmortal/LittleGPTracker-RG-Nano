#include "ImportSampleDialog.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SampleInstrument.h"
#include "RecordSampleDialog.h"

#define LIST_SIZE 12
#define LIST_WIDTH 28

bool ImportSampleDialog::initStatic_=false ;
Path ImportSampleDialog::sampleLib_("") ;
Path ImportSampleDialog::currentPath_("") ;

static const char *buttonText[4]= {
	"Listen",
	"Import",
	"Record",
	"Exit"
} ;

ImportSampleDialog::ImportSampleDialog(View &view):ModalView(view) {
	if (!initStatic_) {
		const char *slpath=SamplePool::GetInstance()->GetSampleLib() ;
		sampleLib_=Path(slpath) ;
		currentPath_=Path(slpath) ;
		initStatic_=true ;
	}
	selected_=0 ;
	importStatus_[0]=0 ;
} ;

ImportSampleDialog::~ImportSampleDialog() {
} ;

void ImportSampleDialog::DrawView() {

	SetWindow(LIST_WIDTH,LIST_SIZE+6) ;

	GUITextProperties props ;

// Draw title

//	char title[40] ;

	SetColor(CD_NORMAL) ;

//	sprintf(title,"Sample Import from %s",currentPath_.GetName()) ;
//	w_.DrawString(title,pos,props) ;

// Draw samples

	int x=1 ;
	int y=1 ;

	if (currentSample_<topIndex_) {
		topIndex_=currentSample_ ;
	} ;
	if (currentSample_>=topIndex_+LIST_SIZE) {
		topIndex_=currentSample_ ;
	} ;

	IteratorPtr<Path> it(sampleList_.GetIterator()) ;
	int count=0 ;
	char buffer[256] ;
	for(it->Begin();!it->IsDone();it->Next()) {
		if ((count>=topIndex_)&&(count<topIndex_+LIST_SIZE)) {
			Path &current=it->CurrentItem() ;
			const std::string p=current.GetName() ;

			if (count==currentSample_) {
				SetColor(CD_HILITE2) ;
				props.invert_=true ;
			} else {
				SetColor(CD_NORMAL) ;
				props.invert_=false ;
			}
			if (!current.IsDirectory()) {
				strcpy(buffer,p.c_str()) ;
			} else {
				buffer[0]='[' ;
				strcpy(buffer+1,p.c_str()) ;
				strcat(buffer,"]") ;
			}
			buffer[LIST_WIDTH-1]=0 ;
			DrawString(x,y,buffer,props) ;
			y+=1 ;
		}
		count++ ;
	} ;

	Path *selectedElement=getImportElement();
	drawSelectedInfo(selectedElement,sampleList_.Size());

	y=LIST_SIZE+5 ;

	SetColor(CD_NORMAL) ;

	const int buttonX[4] = {1, 8, 15, 22};
	for (int i=0;i<4;i++) {
		const char *text=buttonText[i] ;
		x=buttonX[i] ;
		props.invert_=(i==selected_)?true:false ;
		DrawString(x,y,text,props) ;
	}	
} ;

long ImportSampleDialog::getFileSizeKb(Path &element) {
	if (!element.IsFile()) {
		return 0;
	}
	I_File *file=FileSystem::GetInstance()->Open(element.GetPath().c_str(),"r");
	if (!file) {
		return 0;
	}
	file->Seek(0,SEEK_END);
	long size=file->Tell();
	file->Close();
	delete file;
	if (size<=0) {
		return 0;
	}
	return (size+1023)/1024;
}

void ImportSampleDialog::drawSelectedInfo(Path *element, int total) {
	GUITextProperties props;
	char line[64];
	int y=LIST_SIZE+1;

	SetColor(CD_BORDER);
	props.invert_=false;
	DrawString(1,y,"--------------------------",props);
	y+=1;

	SetColor(CD_NORMAL);
	if (!element) {
		DrawString(1,y,"No sample selected",props);
		return;
	}

	const char *kind=element->IsDirectory()?"DIR":"WAV";
	int position=currentSample_+1;
	if (position<1) position=1;
	if (total<1) total=1;
	if (element->IsDirectory()) {
		sprintf(line,"%s %02d/%02d open with A",kind,position,total);
	} else {
		long kb=getFileSizeKb(*element);
		sprintf(line,"%s %02d/%02d %ldK",kind,position,total,kb);
	}
	line[LIST_WIDTH-1]=0;
	DrawString(1,y,line,props);
	y+=1;

	std::string name=element->GetName();
	if (!element->IsDirectory() && SamplePool::GetInstance()->IsImported(name)) {
		name+=" *in project";
	}
	if (importStatus_[0] && !element->IsDirectory()) {
		name=importStatus_;
	}
	strncpy(line,name.c_str(),LIST_WIDTH-1);
	line[LIST_WIDTH-1]=0;
	DrawString(1,y,line,props);
}

void ImportSampleDialog::warpToNextSample(int direction) {

	currentSample_+=direction ;
	int size=sampleList_.Size() ;
	if (currentSample_ < 0) currentSample_ = 0;
	if (currentSample_ >= size) currentSample_ = size - 1;
	endPreview();
	isDirty_=true ;
}

void ImportSampleDialog::OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) {
} ;

void ImportSampleDialog::OnFocus() {
	Path current(currentPath_) ;
	setCurrentFolder(&current) ;
	toInstr_=viewData_->currentInstrument_ ;
} ;

void ImportSampleDialog::CustomizeContextOverlay(
	const char *&name, const char *&where, const char *&edit,
	const char *&field, const char *&cmd1, const char *&cmd2,
	const char *&cmd3, const char *&cmd4, const char *&cmd5,
	const char *&cmd6, const char *&cmd7) {
	name="SAMPLES";
	where="Listen Import Record Exit";
	edit="A action Start preview";
	field="Browse samples";
	cmd1="Up/Down choose file";
	cmd2="B+Up/Dn page list";
	cmd3="Left/Right action";
	cmd4="A listen/import/exit";
	cmd5="Start+Up/Dn preview";
	cmd6="Start+Right import";
	cmd7="B leave  RB+Sel helper";
}

void ImportSampleDialog::preview(Path &element) {
	Player::GetInstance()->StartStreaming(element) ;
}

void ImportSampleDialog::endPreview() {
	Player::GetInstance()->StopStreaming() ;
}

void ImportSampleDialog::setImportStatus(int note) {
	if (note>=0) {
		sprintf(importStatus_,"suggest root %03d",note);
	} else {
		strcpy(importStatus_,"imported root manual");
	}
}

void ImportSampleDialog::import(Path &element) {

#ifdef PLATFORM_RGNANO_SIM
	Trace::Log("ImportSampleDialog","import %s",element.GetPath().c_str());
#endif
	SamplePool *pool=SamplePool::GetInstance() ;
	int sampleID=pool->ImportSample(element) ;
	if (sampleID>=0) {
		I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(toInstr_) ;
		if (instr->GetType()==IT_SAMPLE) {
			SampleInstrument *sinstr=(SampleInstrument *)instr ;
			sinstr->AssignSample(sampleID) ;
			int rootNote=sinstr->DetectRootNoteSuggestion();
			setImportStatus(rootNote);
			toInstr_=viewData_->project_->GetInstrumentBank()->GetNext() ;
		};
	} else {
		Trace::Error("failed to import sample") ;
	};
	isDirty_=true ;
} ;

void ImportSampleDialog::ProcessButtonMask(unsigned short mask,bool pressed) {

	// B tapped alone leaves (like every other dialog); B+Up/Down pages. So
	// leaving waits for B to be let go without an arrow in between.
	if (backTapped(mask,pressed)) {
		endPreview() ;
		EndModal(0) ;
		return ;
	}
	if (!pressed) return ;

	if (mask&EPBM_B) {  
		if (mask&EPBM_UP) warpToNextSample(-LIST_SIZE) ;
		if (mask&EPBM_DOWN) warpToNextSample(LIST_SIZE) ;
	} else if (mask==EPBM_A) {
		// (Browsing while listening is Start+Up/Down)
		Path *element = getImportElement();
		if (!element) {
			Trace::Error("ImportSampleDialog no element for A mask %u",mask);
			return;
		}
#ifdef PLATFORM_RGNANO_SIM
		Trace::Log("ImportSampleDialog","A mask=%u selected=%d element=%s dir=%d",mask,selected_,element->GetPath().c_str(),element->IsDirectory()?1:0);
#endif
		setCurrent(element, mask);

		switch(selected_) {
			case 0: // preview
				if(!element->IsDirectory()) { // Don't browse preview folders
					preview(*element);
				}
				break ;
			case 1: // import
				if(!element->IsDirectory()) { // Don't browse import folders
					import(*element);
				}
				break ;
			case 2: // record
				{
					endPreview(); // Stop playback before recording
					RecordSampleDialog *rsd=new RecordSampleDialog(*this);
					DoModal(rsd);
				}
				break ;
			case 3: // Exit ;
				endPreview(); // Stop playback when exiting
				EndModal(0) ;
				break ;
		}
	} else if (mask&EPBM_START) { // START Modifier
		if (mask&EPBM_UP) warpToNextSample(-1);
		if (mask&EPBM_DOWN) warpToNextSample(1);
		Path *element = getImportElement();
		if (!element) {
			Trace::Error("ImportSampleDialog no element for START mask %u",mask);
			return;
		}
#ifdef PLATFORM_RGNANO_SIM
		Trace::Log("ImportSampleDialog","START mask=%u selected=%d element=%s dir=%d",mask,selected_,element->GetPath().c_str(),element->IsDirectory()?1:0);
#endif
		setCurrent(element, mask);
		if(!element->IsDirectory()) {
			preview(*element);
		}
		if (mask&EPBM_RIGHT) { // Load sample
			if (!element->IsDirectory()) {
				endPreview();
				import(*element);
			}
		}
		if (mask&EPBM_LEFT) { // Navigate up
			if (isSampleLibRoot()) {
			} else {
				Path parent = element->GetParent().GetParent();
				setCurrentFolder(&parent);
				isDirty_=true;
			}
		}
	} else { // No modifier
		if (mask==EPBM_UP) warpToNextSample(-1);
		if (mask==EPBM_DOWN) warpToNextSample(1);
		if (mask==EPBM_LEFT) {
			selected_-=1;
			if (selected_<0) selected_+=4;
			isDirty_=true;
		}
		if (mask==EPBM_RIGHT) {
			selected_=(selected_+1)%4;
			isDirty_=true;
		}
	}
} ;

bool ImportSampleDialog::isSampleLibRoot()
{
    // return sampleLib_.GetPath().find(currentPath_.GetPath()) != std::string::npos; // Causes issues in Win, Miyoo
	return currentPath_.GetPath()==sampleLib_.GetPath();
};

Path* ImportSampleDialog::getImportElement() {
	IteratorPtr<Path> it(sampleList_.GetIterator());
	int count = 0;
	for(it->Begin(); !it->IsDone(); it->Next()) {
		if (count++ == currentSample_) {
			return &it->CurrentItem();
		}
	}
	return 0;
}

void ImportSampleDialog::setCurrent(Path *element, unsigned short mask) {
	if (selected_ != 2 && element->IsDirectory() && // Folders
		!(mask&EPBM_UP||mask&EPBM_DOWN)) { // Don't browse preview folders
			if (element->GetName()=="..") {
				if (isSampleLibRoot()) {
				} else {
					Path parent = element->GetParent().GetParent();
					setCurrentFolder(&parent);
				}
		} else {
			setCurrentFolder(element);
		}
		isDirty_ = true;
		return;
	}
}

void ImportSampleDialog::setCurrentFolder(Path *path) {

	Path formerPath(currentPath_) ;

	topIndex_=0 ;
	currentSample_=0 ;

	currentPath_=Path(*path) ;
	sampleList_.Empty() ;
	if (path) {
		int count=0 ;
		I_Dir *dir=FileSystem::GetInstance()->Open(path->GetPath().c_str()) ;	
		if (dir) {
			dir->GetContent("*") ;
			dir->Sort() ;
			IteratorPtr<Path>it(dir->GetIterator()) ;
			for (it->Begin();!it->IsDone();it->Next()) {
				Path &current=it->CurrentItem() ;
		 		if (current.IsDirectory()) {
					if (current.GetName().substr(0,1)!="." || current.GetName()=="..") {
						Path *sample=new Path(current) ;
						sampleList_.Insert(sample) ;
						if (!formerPath.Compare(current)) {
							currentSample_=count ;
						}
						count++ ;
					}
				}
			}
			for (it->Begin();!it->IsDone();it->Next()) {
				Path &current=it->CurrentItem() ;
		 		if (!current.IsDirectory()) {
					if (current.Matches("*.wav") && current.GetName()[0]!='.') {
						Path *sample=new Path(current) ;
						sampleList_.Insert(sample) ;
					}
				};
			}
		}
	}
#ifdef PLATFORM_RGNANO_SIM
	Trace::Log("ImportSampleDialog","folder %s entries=%d current=%d",currentPath_.GetPath().c_str(),sampleList_.Size(),currentSample_);
#endif
} ;
