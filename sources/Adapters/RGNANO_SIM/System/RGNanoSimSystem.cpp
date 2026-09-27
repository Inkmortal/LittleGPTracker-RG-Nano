#include "RGNanoSimSystem.h"
#include "Adapters/Dummy/Midi/DummyMidi.h"
#include "Adapters/SDL/GUI/GUIFactory.h"
#include "Adapters/SDL/GUI/SDLEventManager.h"
#include "Adapters/SDL/Audio/SDLAudio.h"
#include "Adapters/SDL/Process/SDLProcess.h"
#include "Adapters/SDL/Timer/SDLTimer.h"
#include "RGNanoSimMemory.h"
#include "System/Console/CrashLog.h"
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

// Heartbeat "mem": the heap plus the device's non-heap part, i.e. what the
// RG Nano would report as the app's RSS for the same work
static int deviceEquivalentKB() {
	return (int)(RGNanoSimMemory::LiveKB()+RGNANOSIM_DEVICE_NONHEAP_KB) ;
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

	// The device's heap budget (docs/RGNANO_SIM.md): allocations past it
	// fail here as they do on the RG Nano
	unsigned int deviceKB=RGNANOSIM_DEVICE_PROCESS_KB ;
	const char *budget=config->GetValue("RGNANOSIM_DEVICE_PROCESS_KB") ;
	if (budget) {
		deviceKB=(unsigned int)atoi(budget) ;
	}
	RGNanoSimMemory::SetDeviceBudget(deviceKB,RGNANOSIM_DEVICE_NONHEAP_KB) ;
	CrashLog::SetMemoryProbe(deviceEquivalentKB) ;
	Trace::Log("RGNANO_SIM","heap limit %uKB (device process %uKB, non-heap %uKB), in use at boot %uKB",
	           RGNanoSimMemory::LimitKB(),deviceKB,RGNANOSIM_DEVICE_NONHEAP_KB,RGNanoSimMemory::LiveKB()) ;

	// The device's timer, audio driver and threads (RGNANOSystem.cpp), so
	// the sim runs the same threads: SDL timer thread for key repeat, SDL
	// audio callback plus the render thread it wakes, player updates
	// queued from that thread to the UI thread
	I_GUIWindowFactory::Install(new GUIFactory()) ;
	TimerService::GetInstance()->Install(new SDLTimerService()) ;

	AudioSettings hint ;
	hint.bufferSize_=1024 ;
	hint.preBufferCount_=8 ;
	Audio::Install(new SDLAudio(hint)) ;

	MidiService::Install(new DummyMidi()) ;
	SysProcessFactory::Install(new SDLProcessFactory()) ;

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
