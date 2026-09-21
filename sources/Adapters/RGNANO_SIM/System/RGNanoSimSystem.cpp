#include "RGNanoSimSystem.h"
#include "Adapters/Dummy/Midi/DummyMidi.h"
#include "Adapters/SDL/GUI/GUIFactory.h"
#include "Adapters/SDL/GUI/SDLEventManager.h"
#include "Adapters/W32/Audio/W32Audio.h"
#include "Adapters/W32/Process/W32Process.h"
#include "Adapters/W32/Timer/W32Timer.h"
#include "Adapters/W32FileSystem/W32FileSystem.h"
#include "Application/Model/Config.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include "System/Console/Logger.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

EventManager *RGNanoSimSystem::eventManager_ = NULL ;

// Headless runs (scripts, renders): SDL draws into a window that is never
// shown, so nothing appears on screen or takes keyboard focus. Screenshots,
// screen-text checks and audio all keep working.
static HWND createHiddenHostWindow() {
	WNDCLASSA wc ;
	memset(&wc,0,sizeof(wc)) ;
	wc.lpfnWndProc=DefWindowProcA ;
	wc.hInstance=GetModuleHandle(NULL) ;
	wc.lpszClassName="RGNanoSimHeadless" ;
	RegisterClassA(&wc) ;
	return CreateWindowExA(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,"RGNanoSimHeadless","rgnano-sim",
	                       WS_POPUP,0,0,1,1,NULL,NULL,wc.hInstance,NULL) ;
}

int RGNanoSimSystem::MainLoop() {
	eventManager_->InstallMappings() ;
	return eventManager_->MainLoop() ;
}

void RGNanoSimSystem::Boot(int argc,char **argv) {
	// Older audit tools ask for SDL's dummy driver; this SDL build only has
	// windib, so treat that request as headless mode instead.
	const char *requestedDriver=getenv("SDL_VIDEODRIVER") ;
	bool dummyRequested=(requestedDriver && !strcmp(requestedDriver,"dummy")) ;
	putenv("SDL_VIDEODRIVER=windib") ;

	System::Install(new RGNanoSimSystem()) ;
	FileSystem::Install(new W32FileSystem()) ;

	HMODULE module = GetModuleHandle(NULL);
	char tempPath[MAX_PATH];
	GetModuleFileName(module,tempPath,MAX_PATH);
	int n = (int)strlen(tempPath)-1;
	while ((n>0)&&(tempPath[n] !='\\')) {
		n--;
	}
	if (n<3) {
		n=3;
	}
	tempPath[n]=0;

	Path::SetAlias("bin",tempPath) ;
	Path::SetAlias("root","bin:.") ;
	FileLogger *logger = new FileLogger(Path("bin:rgnano-sim.log"));
	Result loggerInit = logger->Init();
	loggerInit.Succeeded();
	Trace::GetInstance()->SetLogger(*logger);
	Trace::Log("RGNANO_SIM","Boot");

	Config *config=Config::GetInstance() ;
	config->ProcessArguments(argc,argv) ;

	I_GUIWindowFactory::Install(new GUIFactory()) ;
	TimerService::GetInstance()->Install(new W32TimerService()) ;

	AudioSettings hints ;
	hints.audioAPI_="MMSYSTEM" ;
	hints.audioDevice_="" ;
	hints.bufferSize_=512 ;
	hints.preBufferCount_=10;
	Audio::Install(new W32Audio(hints)) ;

	MidiService::Install(new DummyMidi()) ;
	SysProcessFactory::Install(new W32ProcessFactory()) ;

	const char *headless=config->GetValue("RGNANOSIM_HEADLESS") ;
	if (dummyRequested || (headless && !strcmp(headless,"YES"))) {
		HWND host=createHiddenHostWindow() ;
		static char windowId[64] ;
		sprintf(windowId,"SDL_WINDOWID=%lu",(unsigned long)host) ;
		putenv(windowId) ;
		Trace::Log("RGNANO_SIM","headless: rendering into hidden window") ;
	}

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_TIMER) < 0) {
		return;
	}
	SDL_EnableUNICODE(1);
	SDL_ShowCursor(SDL_DISABLE);
	atexit(SDL_Quit);

	eventManager_=I_GUIWindowFactory::GetInstance()->GetEventManager() ;
	eventManager_->Init() ;
}

void RGNanoSimSystem::Shutdown() {
	delete Audio::GetInstance() ;
}

unsigned long RGNanoSimSystem::GetClock() {
	return (clock()*1000)/CLOCKS_PER_SEC ;
}

void RGNanoSimSystem::Sleep(int millisec) {
	if (millisec>0) {
		::Sleep(millisec) ;
	}
}

void *RGNanoSimSystem::Malloc(unsigned size) {
	return malloc(size) ;
}

void RGNanoSimSystem::Free(void *ptr) {
	free(ptr) ;
}

void RGNanoSimSystem::Memset(void *addr,char val,int size) {
	memset(addr,val,size) ;
}

void *RGNanoSimSystem::Memcpy(void *s1, const void *s2, int n) {
	return memcpy(s1,s2,n) ;
}

void RGNanoSimSystem::PostQuitMessage() {
	SDLEventManager::GetInstance()->PostQuitMessage() ;
}

unsigned int RGNanoSimSystem::GetMemoryUsage() {
	return 0 ;
}

std::string RGNanoSimSystem::SGetLastErrorString() {
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL);

	std::string error = (char *)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return error;
}
