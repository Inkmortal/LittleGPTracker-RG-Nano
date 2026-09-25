
#include "SDLEventManager.h"
#include "Application/Application.h"
#include "Application/AppWindow.h"
#include "Application/Mixer/SendFX.h"
#include "Application/Mixer/MasterLimiter.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Services/Audio/AudioDriver.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Groove.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Scale.h"
#include "Application/Model/Table.h"
#include "Application/Player/Player.h"
#include "Application/Player/TablePlayback.h"
#include "Application/Persistency/PersistencyService.h"
#include "System/FileSystem/FileSystem.h"
#include "UIFramework/BasicDatas/FontConfig.h"
#include "UIFramework/BasicDatas/GUIEvent.h"
#include "SDLGUIWindowImp.h"
#include "Application/Model/Config.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"
#include "Application/Views/BaseClasses/ViewEvent.h"
#include "Application/Views/ModalDialogs/SelectProjectDialog.h"
#include "Application/Utils/char.h"
#include <fstream>
#ifdef PLATFORM_RGNANO
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#endif
#include <sstream>
#include <algorithm>
#include <stdlib.h>
#include <string.h>

bool SDLEventManager::finished_=false ;
bool SDLEventManager::dumpEvent_=false ;
bool SDLEventManager::showPowerMenu_=false ;
int SDLEventManager::powerMenuSelection_=0;
bool SDLEventManager::showExitConfirm_=false ;
int SDLEventManager::exitConfirmSelection_=0;
bool SDLEventManager::showDebugScreen_=false ;
int SDLEventManager::debugScreenSelection_=0;
bool SDLEventManager::menuHelpOverlay_=false ;
int SDLEventManager::menuHelpPage_=0;
bool SDLEventManager::menuInputHeld_[SDLK_LAST]={false};
int SDLEventManager::powerMenuKey_=SDLK_POWER;
int SDLEventManager::rShoulderKey_=SDLK_n;
int SDLEventManager::selectKey_=SDLK_q;
unsigned long SDLEventManager::replayLastEventMs_=0;

static void DrawSimOverlayText(SDL_Surface *screen, const char *text, int x, int y, Uint32 color, int scale)
{
	if (!screen || !text) return;
	if (scale < 1) scale = 1;
	for (int i=0;text[i];i++) {
		unsigned int fontID=(unsigned char)text[i];
		if (fontID < FONT_COUNT) {
			for (int row=0;row<8;row++) {
				for (int col=0;col<8;col++) {
					if (font[fontID*8 + row*FONT_WIDTH + col] == 0) {
						SDL_Rect pixel = { Sint16(x + i*8*scale + col*scale), Sint16(y + row*scale), Uint16(scale), Uint16(scale) };
						SDL_FillRect(screen,&pixel,color);
					}
				}
			}
		}
	}
}

#ifdef PLATFORM_RGNANO_SIM
static ViewData *GetSimViewData();
#endif
static const char *powerMenuSelectedLabel(int selection);
static void readSystemLevels();

SDLEventManager::SDLEventManager() 
{
#ifdef PLATFORM_RGNANO_SIM
	simCommandIndex_=0;
	simNextCommandTime_=0;
	simPendingReleaseKey_=0;
	simGoalSteps_=0;
	simMouseKey_=0;
	simScriptActive_=false;
	simScriptFailed_=false;
#endif
}

SDLEventManager::~SDLEventManager() 
{
}

bool SDLEventManager::Init() 
{
	EventManager::Init() ;
	
	if ( SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_TIMER) < 0 )
  {
		return false;
	}
  
	SDL_EnableUNICODE(1);
	SDL_ShowCursor(SDL_DISABLE);
#ifdef PLATFORM_RGNANO_SIM
	const char *mute=Config::GetInstance()->GetValue("RGNANOSIM_MUTE") ;
	if (mute && !strcmp(mute,"YES")) {
		AudioDriver::SetSimAudioMuted(true) ;
	}
	const char *skin=Config::GetInstance()->GetValue("RGNANOSIM_SKIN") ;
	if ((skin)&&(!strcmp(skin,"YES"))) {
		SDL_ShowCursor(SDL_ENABLE);
	}
#endif
	
	atexit(SDL_Quit) ;
	
  SDL_InitSubSystem(SDL_INIT_JOYSTICK);

	int joyCount=SDL_NumJoysticks() ;
	joyCount=(joyCount>MAX_JOY_COUNT)?MAX_JOY_COUNT:joyCount ;

	keyboardCS_=new KeyboardControllerSource("keyboard") ;
	const char *dumpIt=Config::GetInstance()->GetValue("DUMPEVENT") ;
	if ((dumpIt)&&(!strcmp(dumpIt,"YES")))
  {
		dumpEvent_=true ;
	}

#ifdef PLATFORM_RGNANO_SIM
	LoadSimScript();
#endif

	for (int i=0;i<MAX_JOY_COUNT;i++) 
  {
		joystick_[i]=0 ;
		buttonCS_[i]=0 ;
		joystickCS_[i]=0 ;
	}
    
	for (int i=0;i<joyCount;i++) 
  {
		char sourceName[128] ;
		joystick_[i]=SDL_JoystickOpen(i) ;
    Trace::Log("EVENT","joystick[%d]=%x",i,joystick_[i]) ;
		Trace::Log("EVENT","Number of axis:%d",SDL_JoystickNumAxes(joystick_[i])) ;
		Trace::Log("EVENT","Number of buttons:%d",SDL_JoystickNumButtons(joystick_[i])) ;
		Trace::Log("EVENT","Number of hats:%d",SDL_JoystickNumHats(joystick_[i])) ;
		sprintf(sourceName,"buttonJoy%d",i) ;
		buttonCS_[i]=new ButtonControllerSource(sourceName) ;
		sprintf(sourceName,"axisJoy%d",i) ;
		joystickCS_[i]=new JoystickControllerSource(sourceName) ;
		sprintf(sourceName,"hatJoy%d",i) ;
		hatCS_[i]=new HatControllerSource(sourceName) ;
	}
  
	for (int i=0;i<SDLK_LAST;i++) 
  {
		keyname_[i]=SDL_GetKeyName((SDLKey)i) ;
	} 

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
	const char *powerKey=Config::GetInstance()->GetValue("KEY_POWER");
	if (powerKey && strlen(powerKey)>0) {
		int key=GetKeyCode(powerKey);
		if (key>0) {
			powerMenuKey_=key;
		} else {
			Trace::Error("EVENT invalid KEY_POWER=%s",powerKey);
		}
	}
#endif
	const char *rKey=Config::GetInstance()->GetValue("KEY_RSHOULDER");
	if (rKey && strlen(rKey)>0) {
		int key=GetKeyCode(rKey);
		if (key>0) {
			rShoulderKey_=key;
		}
	}
	const char *selectKey=Config::GetInstance()->GetValue("KEY_SELECT");
	if (selectKey && strlen(selectKey)>0) {
		int key=GetKeyCode(selectKey);
		if (key>0) {
			selectKey_=key;
		}
	}
  
	return true ;
} 

static bool powerMenuHasSong();

#ifdef PLATFORM_RGNANO
// FunKey OS "powerdown schedule" sends SIGUSR1 to the recorded app pid and
// powers off shortly after unless the app takes over with "powerdown handle".
static volatile sig_atomic_t shutdownRequested_=0;

static void onShutdownSignal(int) {
	shutdownRequested_=1;
}

static void installShutdownHandler() {
	struct sigaction sa;
	memset(&sa,0,sizeof(sa));
	sa.sa_handler=onShutdownSignal;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1,&sa,0);
	// Launch scripts normally record the pid; OPKs are started directly
	char cmd[64];
	snprintf(cmd,sizeof(cmd),"pid record %d >/dev/null 2>&1",(int)getpid());
	system(cmd);
}

// Every 10 s of playback, append the audio load to root:lgpt-perf.log on the
// SD card so dropouts on the device can be read back from a PC.
// First line of a small text file, or "" (used for /proc and /sys probes)
static std::string readSystemLine(const char *path) {
	I_File *f=FileSystem::GetInstance()->Open(path,(char *)"r");
	if (!f) return "";
	char text[192];
	int n=f->Read(text,1,sizeof(text)-1);
	f->Close();
	delete f;
	if (n<=0) return "";
	text[n]=0;
	std::string line(text);
	size_t nl=line.find('\n');
	if (nl!=std::string::npos) line=line.substr(0,nl);
	return line;
}

// One line per launch in root:lgpt-perf.log, so the build that actually ran
// can be read back from the SD card (the device clock is not reliable).
static void logSessionStart() {
	std::string build="unknown";
	I_File *id=FileSystem::GetInstance()->Open(Path("bin:build-id.txt").GetPath().c_str(),(char *)"r");
	if (id) {
		char text[64];
		int n=id->Read(text,1,sizeof(text)-1);
		id->Close();
		delete id;
		if (n>0) {
			text[n]=0;
			build=text;
			while (!build.empty() && (build[build.size()-1]=='\n' || build[build.size()-1]=='\r')) {
				build.erase(build.size()-1);
			}
		}
	}
	Path log=Path("root:").Descend("lgpt-perf.log");
	I_File *f=FileSystem::GetInstance()->Open(log.GetPath().c_str(),(char *)"a");
	if (!f) return;
	f->Printf("=== app start, build %s ===\n",build.c_str());
	f->Close();
	delete f;
}

// While a song plays, append what only the device knows to
// root:lgpt-perf.log: our own render load, ALSA's dropout counter and the
// CPU clock. Read it from a PC by plugging the Nano in as a USB drive.
static void logAudioLoad() {
	static Uint32 last=0;
	Uint32 now=SDL_GetTicks();
	if (now-last<3000) return;
	last=now;
	Player *player=Player::GetInstance();
	if (!player) return;
	// Logged while stopped too: a voice still sounding after Stop shows up
	// here as a channel that is still playing
	Path log=Path("root:").Descend("lgpt-perf.log");
	I_File *f=FileSystem::GetInstance()->Open(log.GetPath().c_str(),(char *)"a");
	if (!f) return;
	std::string khz=readSystemLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
	std::string governor=readSystemLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
	// "state: RUNNING ... xruns: N" in the ALSA playback status
	std::string xruns;
	I_File *status=FileSystem::GetInstance()->Open("/proc/asound/card0/pcm0p/sub0/status",(char *)"r");
	if (status) {
		char text[1024];
		int n=status->Read(text,1,sizeof(text)-1);
		status->Close();
		delete status;
		if (n>0) {
			text[n]=0;
			const char *found=strstr(text,"xruns");
			if (found) {
				xruns=found;
				size_t nl=xruns.find('\n');
				if (nl!=std::string::npos) xruns=xruns.substr(0,nl);
			}
		}
	}
	// One digit per channel: still making sound or not
	char channels[SONG_CHANNEL_COUNT+1];
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		channels[i]=player->IsChannelPlaying(i)?'1':'0';
	}
	channels[SONG_CHANNEL_COUNT]=0;
	// out = what the app is sending to the speaker right now; fx = reverb/
	// echo still ringing. Sound heard while out=0 comes from below the app.
	// voices: per channel "i<nn>" instrument attached, "t<nn>" release tail
	std::string voices;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		char one[16];
		int inst=player->GetChannelInstrumentIndex(i);
		int tail=player->GetChannelTailIndex(i);
		if (inst<0 && tail<0) {
			snprintf(one,sizeof(one),"%d:- ",i);
		} else {
			snprintf(one,sizeof(one),"%d:i%02X/t%02X",i,inst<0?0xFF:inst,tail<0?0xFF:tail);
			voices+=one;
			// a synth tail: its envelope stage (3 = release) and level
			int stage=0;
			float level=0.0f;
			if (player->GetChannelTailVoice(i,stage,level)) {
				char more[40];
				snprintf(more,sizeof(more),"(s%d %s%.4f)",stage,level!=level?"NaN ":"",level!=level?0.0f:level);
				voices+=more;
			}
			one[0]=' ';
			one[1]=0;
		}
		voices+=one;
	}
	char broken[32];
	snprintf(broken,sizeof(broken)," broken=%d",SynthInstrument::BrokenVoiceCount()+MacroInstrument::BrokenVoiceCount());
	voices+=broken;
	bool fxInput=false;
	int fxTail=0,fxFade=0;
	SendFX::GetInstance()->GetDebugState(fxInput,fxTail,fxFade);
	f->Printf("t=%us %s ch=%s out=%d%% fx=%d(in%d tail%d fade%d) voices=[%s] load=%d%% peak=%d%% underruns=%lu cpu=%s(%s) %s\n",
		(unsigned)(now/1000),player->IsRunning()?"play":"stop",channels,
		MixerService::GetInstance()->GetMasterPeakPercent(),
		SendFX::GetInstance()->IsActive()?1:0,fxInput?1:0,fxTail,fxFade,voices.c_str(),
		AudioDriver::GetRenderLoadPercent(),
		AudioDriver::TakeRenderLoadPeak(),AudioDriver::GetUnderrunCount(),
		khz.empty()?"?":khz.c_str(),governor.empty()?"?":governor.c_str(),
		xruns.empty()?"xruns ?":xruns.c_str());
	f->Close();
	delete f;
}

void SDLEventManager::HandleShutdownRequest()
{
	shutdownRequested_=0;
	// Cancel the firmware's own timer; we power off once the song is saved
	system("powerdown handle >/dev/null 2>&1");
	Trace::Log("EVENT","Shutdown requested: saving song");
	if (powerMenuHasSong()) {
		PersistencyService::GetInstance()->Save();
	}
	sync();
	system("pid erase >/dev/null 2>&1");
	system("powerdown now >/dev/null 2>&1");
	PostQuitMessage();
}
#endif

void SDLEventManager::RefreshOverlays(bool wasOpen)
{
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (!appWindow) return;
	bool open=showPowerMenu_ || showDebugScreen_;
	// Closing: the overlay covered the whole screen, repaint every cell
	appWindow->RepaintNow(wasOpen && !open);
}

int SDLEventManager::MainLoop()
{
	GUIWindow *appWindow=Application::GetInstance()->GetWindow() ;
	SDLGUIWindowImp *sdlWindow=(SDLGUIWindowImp *)appWindow->GetImpWindow() ;
#ifdef PLATFORM_RGNANO
	installShutdownHandler();
	logSessionStart();
#endif

	while (!finished_)
	{
		SDL_Event event;
#if defined(PLATFORM_RGNANO_SIM)
		bool hasEvent = (SDL_PollEvent(&event) != 0);
#elif defined(PLATFORM_RGNANO)
		// Same 10 ms wait SDL_WaitEvent uses, but a shutdown signal can
		// interrupt it
		bool hasEvent = false;
		while (!finished_ && !(hasEvent = (SDL_PollEvent(&event) != 0))) {
			if (shutdownRequested_) {
				HandleShutdownRequest();
				break;
			}
			logAudioLoad();
			SDL_Delay(10);
		}
#else
		bool hasEvent = (SDL_WaitEvent(&event) != 0);
#endif
		if (hasEvent)
    {
		bool overlayWasOpen = showPowerMenu_ || showDebugScreen_;
#ifdef PLATFORM_RGNANO_SIM
			if (HandleSimMouse(sdlWindow,event)) {
				continue;
			}
#endif
			switch (event.type) {
				case SDL_KEYDOWN:
					if (dumpEvent_)
          {
						Trace::Log("EVENT","key(%s):%d",keyname_[event.key.keysym.sym],1) ;
						LogReplayKey(event.key.keysym.sym,true);
					}
					if (event.key.keysym.sym>=0 && event.key.keysym.sym<SDLK_LAST && menuInputHeld_[event.key.keysym.sym]) {
						break;
					}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
					if (event.key.keysym.sym == powerMenuKey_) {
						menuInputHeld_[event.key.keysym.sym]=true;
						showPowerMenu_ = !showPowerMenu_;
						if (showPowerMenu_) {
							readSystemLevels();
							powerMenuSelection_ = 0;  // Reset to first option
							showExitConfirm_ = false;
							menuHelpOverlay_ = false;
							menuHelpPage_ = 0;
						}
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
						if (dumpEvent_) {
							AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
							if (appWindow) {
								appWindow->LogDebugState(showPowerMenu_ ? "after-power-menu-open" : "after-power-menu-close", true);
							}
						}
#endif
						RefreshOverlays(overlayWasOpen);
						break;
					}
#endif

					// If menu is active, handle menu input
					if (showPowerMenu_) {
						if (event.key.keysym.sym>=0 && event.key.keysym.sym<SDLK_LAST) {
							if (menuInputHeld_[event.key.keysym.sym]) {
								break;
							}
							menuInputHeld_[event.key.keysym.sym]=true;
						}
						if (HandleMenuHelpInput(event.key.keysym.sym)) {
							RefreshOverlays(overlayWasOpen);
							break;
						}
						HandlePowerMenuInput(event.key.keysym.sym);
						RefreshOverlays(overlayWasOpen);
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
						if (dumpEvent_) {
							AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
							if (appWindow) {
								appWindow->LogDebugState("after-power-menu-input", true);
							}
						}
#endif
						break;  // Don't pass to game when menu is active
					}

					// If debug screen is active, handle debug input
					if (showDebugScreen_) {
						if (event.key.keysym.sym>=0 && event.key.keysym.sym<SDLK_LAST) {
							if (menuInputHeld_[event.key.keysym.sym]) {
								break;
							}
							menuInputHeld_[event.key.keysym.sym]=true;
						}
						if (HandleMenuHelpInput(event.key.keysym.sym)) {
							RefreshOverlays(overlayWasOpen);
							break;
						}
						HandleDebugScreenInput(event.key.keysym.sym);
						RefreshOverlays(overlayWasOpen);
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
						if (dumpEvent_) {
							AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
							if (appWindow) {
								appWindow->LogDebugState("after-debug-menu-input", true);
							}
						}
#endif
						break;  // Don't pass to game when debug screen is active
					}

					keyboardCS_->SetKey((int)event.key.keysym.sym,true) ;
#ifdef PLATFORM_RGNANO_SIM
					if (sdlWindow->IsRGNanoSkinEnabled()) {
						sdlWindow->SetRGNanoButtonPressed((int)event.key.keysym.sym,true);
					}
#endif
					break ;

				case SDL_KEYUP:
					if (dumpEvent_)
          {
						Trace::Log("EVENT","key(%s):%d",keyname_[event.key.keysym.sym],0) ;
						LogReplayKey(event.key.keysym.sym,false);
					}
					if (event.key.keysym.sym>=0 && event.key.keysym.sym<SDLK_LAST && menuInputHeld_[event.key.keysym.sym]) {
						menuInputHeld_[event.key.keysym.sym]=false;
						break;
					}
					keyboardCS_->SetKey((int)event.key.keysym.sym,false) ;
#ifdef PLATFORM_RGNANO_SIM
					if (sdlWindow->IsRGNanoSkinEnabled()) {
						sdlWindow->SetRGNanoButtonPressed((int)event.key.keysym.sym,false);
					}
#endif
					break ;


				case SDL_JOYBUTTONDOWN:
					buttonCS_[event.jbutton.which]->SetButton(event.jbutton.button,true) ;
					break ;
				case SDL_JOYBUTTONUP:
					if (dumpEvent_) {
						Trace::Log("EVENT","but(%d):%d",event.button.which,event.jbutton.button) ;
					}
					buttonCS_[event.jbutton.which]->SetButton(event.jbutton.button,false) ;
					break ;
				case SDL_JOYAXISMOTION:
					if (dumpEvent_) {
						Trace::Log("EVENT","joy(%d)::%d=%d",event.jaxis.which,event.jaxis.axis,event.jaxis.value) ;
					}
					joystickCS_[event.jaxis.which]->SetAxis(event.jaxis.axis,float(event.jaxis.value)/32767.0f) ;
					break ;
				case SDL_JOYHATMOTION:
					if (dumpEvent_)
          {
						for (int i=0;i<4;i++)
            {
							int mask = 1<<i ;
							if (event.jhat.value&mask)
              {
								Trace::Log("EVENT","hat(%d)::%d::%d",event.jhat.which,event.jhat.hat,i) ;
							}
						}
					}
					hatCS_[event.jhat.which]->SetHat(event.jhat.hat,event.jhat.value) ;
					break ;
				case SDL_JOYBALLMOTION:
					if (dumpEvent_)
          {
						Trace::Log("EVENT","ball(%d)::%d=(%d,%d)",event.jball.which,event.jball.ball,event.jball.xrel,event.jball.yrel) ;
					}
					break ;
		}

			switch (event.type) 
			{

				case SDL_QUIT:
#ifdef PLATFORM_RGNANO_SIM
					if (!simCommands_.empty() && !simScriptFailed_ && simCommandIndex_ < simCommands_.size()) {
						Trace::Log("RGNANO_SIM","user_abort window close while script command %d/%d was active",simCommandIndex_,(int)simCommands_.size());
						LogSimState("user_abort",true);
					}
#endif
					sdlWindow->ProcessQuit() ;
					break ;
				case SDL_VIDEOEXPOSE:
					sdlWindow->ProcessExpose() ;

					break ;
				case SDL_USEREVENT:
					sdlWindow->ProcessUserEvent(event) ;
					break ;
			}
		}
#ifdef PLATFORM_RGNANO_SIM
		ProcessSimScript(sdlWindow);
		SDL_Delay(10);
#endif
	}
#ifdef PLATFORM_RGNANO_SIM
	return simScriptFailed_ ? 1 : 0;
#else
	return 0 ;
#endif
} ;

#ifdef PLATFORM_RGNANO_SIM
#include "Services/Audio/AudioDriver.h"

bool SDLEventManager::AppendSimRoute(const std::string &routeName, const char *scriptPath, int lineNumber)
{
	std::vector<std::string> lines;

	if (routeName=="combo.r.up") {
		lines.push_back("down n"); lines.push_back("press u 80"); lines.push_back("up n");
	} else if (routeName=="combo.r.down") {
		lines.push_back("down n"); lines.push_back("press d 80"); lines.push_back("up n");
	} else if (routeName=="combo.r.left") {
		lines.push_back("down n"); lines.push_back("press l 80"); lines.push_back("up n");
	} else if (routeName=="combo.r.right") {
		lines.push_back("down n"); lines.push_back("press r 80"); lines.push_back("up n");
	} else if (routeName=="combo.r.start") {
		lines.push_back("down n"); lines.push_back("press s 80"); lines.push_back("up n");
	} else if (routeName=="combo.l.up") {
		lines.push_back("down m"); lines.push_back("press u 80"); lines.push_back("up m");
	} else if (routeName=="combo.l.down") {
		lines.push_back("down m"); lines.push_back("press d 80"); lines.push_back("up m");
	} else if (routeName=="combo.l.left") {
		lines.push_back("down m"); lines.push_back("press l 80"); lines.push_back("up m");
	} else if (routeName=="combo.l.right") {
		lines.push_back("down m"); lines.push_back("press r 80"); lines.push_back("up m");
	} else if (routeName=="combo.l.start") {
		lines.push_back("down m"); lines.push_back("press s 80"); lines.push_back("up m");
	} else if (routeName=="combo.a.up") {
		lines.push_back("down a"); lines.push_back("press u 80"); lines.push_back("up a");
	} else if (routeName=="combo.a.down") {
		lines.push_back("down a"); lines.push_back("press d 80"); lines.push_back("up a");
	} else if (routeName=="combo.a.left") {
		lines.push_back("down a"); lines.push_back("press l 80"); lines.push_back("up a");
	} else if (routeName=="combo.a.right") {
		lines.push_back("down a"); lines.push_back("press r 80"); lines.push_back("up a");
	} else if (routeName=="combo.b.up") {
		lines.push_back("down b"); lines.push_back("press u 80"); lines.push_back("up b");
	} else if (routeName=="combo.b.down") {
		lines.push_back("down b"); lines.push_back("press d 80"); lines.push_back("up b");
	} else if (routeName=="combo.b.left") {
		lines.push_back("down b"); lines.push_back("press l 80"); lines.push_back("up b");
	} else if (routeName=="combo.b.right") {
		lines.push_back("down b"); lines.push_back("press r 80"); lines.push_back("up b");
	} else if (routeName=="boot.new_project_random") {
		lines.push_back("wait 500");
		lines.push_back("press r 80");
		lines.push_back("press a 80");
		// The name dialog opens on OK with a free random name
		lines.push_back("wait 200");
		lines.push_back("press a 80");
		lines.push_back("wait 1000");
	} else if (routeName=="project.to_song") {
		lines.push_back("route combo.r.down");
	} else if (routeName=="song.to_project") {
		lines.push_back("route combo.r.up");
	} else if (routeName=="song.to_mixer") {
		lines.push_back("route combo.r.down");
	} else if (routeName=="song.to_chain") {
		lines.push_back("route combo.r.right");
	} else if (routeName=="mixer.to_song") {
		lines.push_back("route combo.r.up");
	} else if (routeName=="chain.to_song") {
		lines.push_back("route combo.r.left");
	} else if (routeName=="chain.to_phrase") {
		lines.push_back("route combo.r.right");
	} else if (routeName=="phrase.to_chain") {
		lines.push_back("route combo.r.left");
	} else if (routeName=="phrase.to_instrument") {
		lines.push_back("route combo.r.right");
	} else if (routeName=="phrase.to_table") {
		lines.push_back("route combo.r.down");
	} else if (routeName=="phrase.to_groove") {
		lines.push_back("route combo.r.up");
	} else if (routeName=="groove.to_phrase") {
		lines.push_back("route combo.r.down");
	} else if (routeName=="instrument.to_phrase") {
		lines.push_back("route combo.r.left");
	} else if (routeName=="instrument.to_table") {
		lines.push_back("route combo.r.down");
	} else if (routeName=="table.to_parent") {
		lines.push_back("route combo.r.up");
	} else if (routeName=="table.to_instrument_table") {
		lines.push_back("route combo.r.right");
	} else if (routeName=="instrument_table.to_table") {
		lines.push_back("route combo.r.left");
	} else if (routeName=="instrument.make_sample") {
		lines.push_back("set type sample");
		lines.push_back("focus sample");
	} else if (routeName=="instrument.open_sample_import") {
		lines.push_back("press k 80");
		lines.push_back("wait 300");
	} else if (routeName=="sample_import.to_import") {
		lines.push_back("press r 80");
	} else if (routeName=="sample_import.to_first_file") {
		lines.push_back("press d 80");
		lines.push_back("wait 120");
	} else if (routeName=="sample_import.import_selected") {
		lines.push_back("press r 80");
		lines.push_back("press a 80");
		lines.push_back("wait 500");
	} else if (routeName=="sample_import.quick_import_selected") {
		lines.push_back("down s");
		lines.push_back("press r 80");
		lines.push_back("up s");
		lines.push_back("wait 500");
	} else if (routeName=="sample_import.exit") {
		lines.push_back("press r 80");
		lines.push_back("press r 80");
		lines.push_back("press r 80");
		lines.push_back("press a 80");
		lines.push_back("wait 200");
	} else {
		Trace::Error("RGNANO_SIM unknown route %s at %s:%d",routeName.c_str(),scriptPath,lineNumber);
		return false;
	}

	Trace::Log("RGNANO_SIM","route %s expanded to %d commands",routeName.c_str(),(int)lines.size());
	for (size_t i=0;i<lines.size();i++) {
		if (!AddSimScriptLine(lines[i],scriptPath,lineNumber)) {
			return false;
		}
	}
	return true;
}

bool SDLEventManager::AddSimScriptLine(const std::string &line, const char *scriptPath, int lineNumber)
{
	if (line.empty() || line[0]=='#') {
		return true;
	}
	std::istringstream iss(line);
	SimCommand command;
	command.arg="";
	command.arg2="";
	command.arg3="";
	command.value=0;
	command.value2=0;
	iss >> command.op;
	if (command.op.empty()) {
		return true;
	}
	if (command.op=="route") {
		iss >> command.arg;
		if (command.arg.empty()) {
			Trace::Error("RGNANO_SIM route missing name at %s:%d",scriptPath,lineNumber);
			return false;
		}
		return AppendSimRoute(command.arg,scriptPath,lineNumber);
	}
	if (command.op=="wait") {
		iss >> command.value;
	} else if (command.op=="press") {
		iss >> command.arg >> command.value;
		if (command.value<=0) {
			command.value=80;
		}
	} else if (command.op=="down" || command.op=="up" || command.op=="screenshot" || command.op=="screenshot_app" || command.op=="log" || command.op=="expect_file" || command.op=="sim_make_project" || command.op=="sim_remove_project" || command.op=="expect_project_exists" || command.op=="expect_no_project" || command.op=="expect_project_sample" || command.op=="expect_view" || command.op=="expect_player_running" || command.op=="expect_play_mode" || command.op=="sim_set_note_names" || command.op=="expect_sample_trim_order" || command.op=="sim_dump_song" || command.op=="expect_song_dump") {
		iss >> command.arg;
	} else if (command.op=="expect_screen_text" || command.op=="expect_selected_text" || command.op=="expect_streaming_sample" || command.op=="dump_state") {
		std::getline(iss,command.arg);
		if (!command.arg.empty() && command.arg[0]==' ') {
			command.arg.erase(0,1);
		}
	} else if (command.op=="click") {
		iss >> command.value >> command.value2 >> command.arg;
		if (command.arg.empty()) {
			command.arg="80";
		}
	} else if (command.op=="expect_log" || command.op=="expect_screens_differ") {
		iss >> command.arg;
		std::getline(iss,command.arg2);
		if (!command.arg2.empty() && command.arg2[0]==' ') {
			command.arg2.erase(0,1);
		}
	} else if (command.op=="expect_no_error" || command.op=="expect_skin_frame_clean" || command.op=="reset_audio_stats" || command.op=="end_audio_capture" || command.op=="sim_save_project" || command.op=="quit") {
	} else if (command.op=="expect_colors" || command.op=="expect_audio_activity" || command.op=="expect_audio_silence" || command.op=="expect_audio_peak_max" || command.op=="expect_audio_capture_bytes" || command.op=="expect_tempo" || command.op=="expect_render_mode" || command.op=="sim_set_tempo" || command.op=="sim_set_render_mode" || command.op=="sim_set_scale" || command.op=="sim_set_key") {
		iss >> command.value;
	} else if (command.op=="expect_project_file_bytes") {
		iss >> command.arg >> command.value;
	} else if (command.op=="start_audio_capture") {
		iss >> command.arg;
	} else if (command.op=="expect_song_chain" || command.op=="expect_chain_phrase") {
		iss >> command.value >> command.value2 >> command.arg;
	} else if (command.op=="expect_phrase_command" || command.op=="expect_phrase_param") {
		iss >> command.value >> command.value2 >> command.arg >> command.arg2;
	} else if (command.op=="expect_groove" || command.op=="expect_table_active" || command.op=="expect_limiter_gr") {
		iss >> command.value >> command.value2;
	} else if (command.op=="expect_phrase_row_count" || command.op=="expect_size") {
		iss >> command.value >> command.value2;
	} else if (command.op=="expect_instrument_sample" || command.op=="expect_playing_channel" || command.op=="expect_channel_row" || command.op=="sim_import_sample_to_instrument" || command.op=="expect_instrument_root" || command.op=="expect_instrument_root_suggestion" || command.op=="sim_detect_trim_root") {
		iss >> command.value >> command.arg;
	} else if (command.op=="sim_set_sample_trim") {
		iss >> command.value >> command.value2 >> command.arg;
	} else if (command.op=="expect_sample_trim") {
		iss >> command.value >> command.value2 >> command.arg >> command.arg2;
	} else if (command.op=="sim_set_song_chain") {
		iss >> command.value >> command.value2 >> command.arg;
	} else if (command.op=="sim_set_chain_phrase" || command.op=="sim_set_phrase_note") {
		iss >> command.value >> command.value2 >> command.arg >> command.arg2;
	} else if (command.op=="goto" || command.op=="page" || command.op=="instrument" || command.op=="row") {
		iss >> command.arg;
	} else if (command.op=="focus") {
		std::getline(iss,command.arg);
		if (!command.arg.empty() && command.arg[0]==' ') command.arg.erase(0,1);
	} else if (command.op=="set") {
		iss >> command.arg >> command.arg2;
		// Parameter names with spaces are written with '_' (mod1_type)
		std::replace(command.arg.begin(),command.arg.end(),'_',' ');
	} else if (command.op=="sim_set_synth" || command.op=="sim_set_macro" || command.op=="expect_instrument_type" || command.op=="expect_instrument_name") {
		iss >> command.value >> command.arg;
	} else if (command.op=="sim_set_instrument_param" || command.op=="expect_instrument_param") {
		iss >> command.value >> command.arg >> command.arg2;
		std::replace(command.arg.begin(),command.arg.end(),'_',' ');
	} else if (command.op=="sim_load_instrument_param") {
		// the value is the rest of the line, spaces and all, as a song
		// file holds it (old names like "ping pong")
		iss >> command.value >> command.arg;
		std::replace(command.arg.begin(),command.arg.end(),'_',' ');
		std::getline(iss,command.arg2);
		if (!command.arg2.empty() && command.arg2[0]==' ') command.arg2.erase(0,1);
	} else if (command.op=="expect_sample_stats") {
		iss >> command.arg >> command.arg2 >> command.value >> command.value2;
	} else if (command.op=="sim_set_phrase_command" || command.op=="sim_set_table_command") {
		iss >> command.value >> command.value2 >> command.arg >> command.arg2 >> command.arg3;
	} else if (command.op=="wait_player") {
		iss >> command.arg >> command.value >> command.value2;
	} else if (command.op=="expect_player") {
		// expect_player <name> <channel> <value or >=value>  (Player::GetSimValue)
		iss >> command.arg >> command.value >> command.arg2;
	} else if (command.op=="expect_notes_repeat") {
		// expect_notes_repeat <channel> <period> yes|no
		iss >> command.value >> command.value2 >> command.arg;
	} else if (command.op=="expect_project_param" || command.op=="sim_set_project_param") {
		// <name_with_underscores> <value>
		iss >> command.arg >> command.arg2;
		std::replace(command.arg.begin(),command.arg.end(),'_',' ');
	} else if (command.op=="expect_bookmark" || command.op=="expect_track_muted") {
		// <row or channel> yes|no
		iss >> command.value >> command.arg;
	} else if (command.op=="expect_phrase_note") {
		// <phrase> <row> <midi note, or -- for none>
		iss >> command.value >> command.value2 >> command.arg;
	} else if (command.op=="expect_phrase_in_scale") {
		// <phrase> <minimum number of notes>: every note fits the Key/Scale
		iss >> command.value >> command.value2;
	} else if (command.op=="expect_mixer_level" || command.op=="sim_set_mixer_level") {
		// <level index> <hex value>
		iss >> command.value >> command.arg;
	}
	simCommands_.push_back(command);
	return true;
}

void SDLEventManager::LoadSimScript()
{
	const char *scriptPath=Config::GetInstance()->GetValue("RGNANOSIM_SCRIPT");
	if (!scriptPath) {
		return;
	}

	std::string resolvedScriptPath(scriptPath);
	std::ifstream file(resolvedScriptPath.c_str());
	if (!file.is_open()) {
		std::string withExtension=resolvedScriptPath + ".rgsim";
		file.open(withExtension.c_str());
		if (file.is_open()) {
			resolvedScriptPath=withExtension;
		}
	}
	if (!file.is_open()) {
		Trace::Error("RGNANO_SIM failed to open script %s", scriptPath);
		return;
	}

	std::string line;
	int lineNumber=0;
	while (std::getline(file,line)) {
		lineNumber++;
		if (!AddSimScriptLine(line,resolvedScriptPath.c_str(),lineNumber)) {
			simScriptFailed_=true;
			break;
		}
	}

	simCommandIndex_=0;
	simNextCommandTime_=System::GetInstance()->GetClock()+250;
	simPendingReleaseKey_=0;
	simScriptActive_=!simCommands_.empty();
	Trace::Log("RGNANO_SIM","Loaded %d script commands from %s",(int)simCommands_.size(),resolvedScriptPath.c_str());
}

void SDLEventManager::ProcessSimScript(SDLGUIWindowImp *window)
{
	if (!simScriptActive_) {
		return;
	}

	unsigned long now=System::GetInstance()->GetClock();
	if (now<simNextCommandTime_) {
		return;
	}

	if (simPendingReleaseKey_>0) {
		SetSimKey(window,simPendingReleaseKey_,false);
		Trace::Log("RGNANO_SIM","auto up key:%d",simPendingReleaseKey_);
		simPendingReleaseKey_=0;
		simCommandIndex_++;
		simNextCommandTime_=now+40;
		return;
	}

	if (simCommandIndex_>=simCommands_.size()) {
		simScriptActive_=false;
		Trace::Log("RGNANO_SIM","Script complete");
		return;
	}

	SimCommand &command=simCommands_[simCommandIndex_];
	std::string stateLabel="before #";
	std::ostringstream commandLabel;
	commandLabel << simCommandIndex_ << " " << command.op;
	if (!command.arg.empty()) {
		commandLabel << " " << command.arg;
	}
	stateLabel += commandLabel.str();
	LogSimState(stateLabel.c_str(),false);

	if (command.op=="wait") {
		simCommandIndex_++;
		simNextCommandTime_=now+(unsigned long)command.value;
		LogSimState("after wait scheduled",false);
		return;
	}

	if (!simGoalRelease_.empty()) {
		for (int i=(int)simGoalRelease_.size()-1;i>=0;i--) {
			SetSimKey(window,simGoalRelease_[i],false);
		}
		simGoalRelease_.clear();
		simNextCommandTime_=now+60;
		return;
	}

	if (command.op=="goto" || command.op=="focus" || command.op=="set" ||
	    command.op=="page" || command.op=="instrument" || command.op=="row") {
		int result=StepSimGoal(window,command);
		if (result<0) {
			FailSimScript("goal command failed");
			return;
		}
		if (result==0) {
			Trace::Log("RGNANO_SIM","%s %s %s => reached in %d steps",command.op.c_str(),
			           command.arg.c_str(),command.arg2.c_str(),simGoalSteps_);
			simGoalSteps_=0;
			simGoalLastState_="";
			simCommandIndex_++;
			simNextCommandTime_=now+40;
		}
		return;
	}

	if (command.op=="wait_player") {
		// wait_player <name> <channel> <value>: until the player value
		// reaches it (a pass, a note count), polling every 20 ms, 10 s max
		int actual=Player::GetInstance()->GetSimValue(command.arg,command.value);
		if (actual>=command.value2) {
			Trace::Log("RGNANO_SIM","wait_player %s ch=%d reached %d (wanted %d) after %d polls",
			           command.arg.c_str(),command.value,actual,command.value2,simGoalSteps_);
			simGoalSteps_=0;
			simCommandIndex_++;
			simNextCommandTime_=now+1;
			return;
		}
		if (++simGoalSteps_>500) {
			simGoalSteps_=0;
			Trace::Error("RGNANO_SIM wait_player %s ch=%d stuck at %d (wanted %d)",
			             command.arg.c_str(),command.value,actual,command.value2);
			FailSimScript("wait_player timed out");
			return;
		}
		simNextCommandTime_=now+20;
		return;
	}

	if (command.op=="press") {
		int key=GetKeyCode(command.arg.c_str());
		if (key>0) {
			SetSimKey(window,key,true);
			simPendingReleaseKey_=key;
			simNextCommandTime_=now+(unsigned long)command.value;
			Trace::Log("RGNANO_SIM","press %s",command.arg.c_str());
			return;
		}
		FailSimScript("unknown key in press command");
		return;
	} else if (command.op=="down") {
		int key=GetKeyCode(command.arg.c_str());
		if (key>0) {
			SetSimKey(window,key,true);
		} else {
			FailSimScript("unknown key in down command");
			return;
		}
	} else if (command.op=="up") {
		int key=GetKeyCode(command.arg.c_str());
		if (key>0) {
			SetSimKey(window,key,false);
		} else {
			FailSimScript("unknown key in up command");
			return;
		}
	} else if (command.op=="click") {
		int key=GetSimButtonAt(command.value,command.value2);
		if (key>0) {
			SetSimKey(window,key,true);
			simPendingReleaseKey_=key;
			simNextCommandTime_=now+(unsigned long)atoi(command.arg.c_str());
			Trace::Log("RGNANO_SIM","click %d %d",command.value,command.value2);
			return;
		}
		FailSimScript("click missed simulated button");
		return;
	} else if (command.op=="screenshot") {
		SaveSimScreenshot(window,command.arg);
	} else if (command.op=="screenshot_app") {
		SaveSimAppScreenshot(window,command.arg);
	} else if (command.op=="quit") {
		PostQuitMessage();
	} else if (command.op=="expect_file") {
		if (!ExpectSimFile(command.arg)) {
			FailSimScript("file assertion failed");
			return;
		}
	} else if (command.op=="sim_make_project") {
		// Test fixture: an empty song folder in the project list
		Path dir=Path("root:").Descend(command.arg);
		// Results must be checked, or the debug build asserts
		Result folder=FileSystem::GetInstance()->MakeDir(dir.GetPath().c_str());
		Result samples=FileSystem::GetInstance()->MakeDir(dir.Descend("samples").GetPath().c_str());
		bool made=folder.Succeeded() && samples.Succeeded() && dir.Exists();
		Trace::Log("RGNANO_SIM","sim_make_project %s => %s",dir.GetPath().c_str(),made?"ok":"failed");
		if (!made) {
			FailSimScript("could not make project folder");
			return;
		}
	} else if (command.op=="sim_remove_project") {
		// Test setup: clear a folder left behind by an earlier failed run
		Path dir=Path("root:").Descend(command.arg);
		if (dir.Exists()) {
			SelectProjectDialog::DeleteFolder(dir);
		}
		Trace::Log("RGNANO_SIM","sim_remove_project %s => %s",dir.GetPath().c_str(),dir.Exists()?"still there":"gone");
	} else if (command.op=="expect_project_exists" || command.op=="expect_no_project") {
		Path dir=Path("root:").Descend(command.arg);
		bool exists=dir.Exists();
		bool wanted=(command.op=="expect_project_exists");
		Trace::Log("RGNANO_SIM","%s %s => %s",command.op.c_str(),dir.GetPath().c_str(),exists?"exists":"missing");
		if (exists!=wanted) {
			FailSimScript("project folder assertion failed");
			return;
		}
	} else if (command.op=="expect_project_file_bytes") {
		if (!ExpectSimProjectFileBytes(command.arg,command.value)) {
			FailSimScript("project file byte assertion failed");
			return;
		}
	} else if (command.op=="expect_project_sample") {
		if (!ExpectSimProjectSample(command.arg)) {
			FailSimScript("project sample assertion failed");
			return;
		}
	} else if (command.op=="expect_log") {
		std::string needle=command.arg;
		if (!command.arg2.empty()) {
			needle+=" "+command.arg2;
		}
		if (!ExpectSimLog(needle)) {
			FailSimScript("log assertion failed");
			return;
		}
	} else if (command.op=="expect_no_error") {
		if (!ExpectSimNoError()) {
			FailSimScript("log error assertion failed");
			return;
		}
	} else if (command.op=="expect_screens_differ") {
		if (!ExpectSimScreensDiffer(command.arg,command.arg2)) {
			FailSimScript("screen difference assertion failed");
			return;
		}
	} else if (command.op=="reset_audio_stats") {
		AudioDriver::ResetSimAudioStats();
		Trace::Log("RGNANO_SIM","reset_audio_stats");
	} else if (command.op=="start_audio_capture") {
		if (!AudioDriver::BeginSimAudioCapture(command.arg.c_str())) {
			FailSimScript("audio capture start failed");
			return;
		}
	} else if (command.op=="end_audio_capture") {
		AudioDriver::EndSimAudioCapture();
	} else if (command.op=="expect_view") {
		if (!ExpectSimView(command.arg)) {
			FailSimScript("view assertion failed");
			return;
		}
	} else if (command.op=="expect_selected_text") {
		if (!ExpectSimSelectedText(command.arg)) {
			FailSimScript("selected text assertion failed");
			return;
		}
	} else if (command.op=="expect_player_running") {
		if (!ExpectSimPlayerRunning(command.arg)) {
			FailSimScript("player running assertion failed");
			return;
		}
	} else if (command.op=="expect_play_mode") {
		if (!ExpectSimPlayMode(command.arg)) {
			FailSimScript("play mode assertion failed");
			return;
		}
	} else if (command.op=="expect_streaming_sample") {
		if (!ExpectSimStreamingSample(command.arg)) {
			FailSimScript("streaming sample assertion failed");
			return;
		}
	} else if (command.op=="expect_playing_channel") {
		if (!ExpectSimPlayingChannel(command.value,command.arg)) {
			FailSimScript("playing channel assertion failed");
			return;
		}
	} else if (command.op=="expect_channel_row") {
		if (!ExpectSimChannelRow(command.value,command.arg)) {
			FailSimScript("channel row assertion failed");
			return;
		}
	} else if (command.op=="expect_audio_activity") {
		if (!ExpectSimAudioActivity(command.value)) {
			FailSimScript("audio activity assertion failed");
			return;
		}
	} else if (command.op=="expect_audio_silence") {
		if (!ExpectSimAudioSilence(command.value)) {
			FailSimScript("audio silence assertion failed");
			return;
		}
	} else if (command.op=="expect_audio_peak_max") {
		if (!ExpectSimAudioPeakMax(command.value)) {
			FailSimScript("audio peak assertion failed");
			return;
		}
	} else if (command.op=="expect_audio_capture_bytes") {
		if (!ExpectSimAudioCaptureBytes(command.value)) {
			FailSimScript("audio capture assertion failed");
			return;
		}
	} else if (command.op=="expect_screen_text") {
		if (!ExpectSimScreenText(command.arg)) {
			FailSimScript("screen text assertion failed");
			return;
		}
	} else if (command.op=="expect_song_chain") {
		if (!ExpectSimSongChain(command.value,command.value2,command.arg)) {
			FailSimScript("song chain assertion failed");
			return;
		}
	} else if (command.op=="expect_chain_phrase") {
		if (!ExpectSimChainPhrase(command.value,command.value2,command.arg)) {
			FailSimScript("chain phrase assertion failed");
			return;
		}
	} else if (command.op=="expect_phrase_command") {
		if (!ExpectSimPhraseCommand(command.value,command.value2,atoi(command.arg.c_str()),command.arg2)) {
			FailSimScript("phrase command assertion failed");
			return;
		}
	} else if (command.op=="expect_phrase_param") {
		if (!ExpectSimPhraseParam(command.value,command.value2,atoi(command.arg.c_str()),command.arg2)) {
			FailSimScript("phrase param assertion failed");
			return;
		}
	} else if (command.op=="expect_groove") {
		if (!ExpectSimGroove(command.value,command.value2)) {
			FailSimScript("groove assertion failed");
			return;
		}
	} else if (command.op=="expect_table_active") {
		if (!ExpectSimTableActive(command.value,command.value2)) {
			FailSimScript("table active assertion failed");
			return;
		}
	} else if (command.op=="expect_phrase_row_count") {
		if (!ExpectSimPhraseRowCount(command.value,command.value2)) {
			FailSimScript("phrase row count assertion failed");
			return;
		}
	} else if (command.op=="expect_tempo") {
		if (!ExpectSimTempo(command.value)) {
			FailSimScript("tempo assertion failed");
			return;
		}
	} else if (command.op=="expect_limiter_gr") {
		if (!ExpectSimLimiterGr(command.value,command.value2)) {
			FailSimScript("limiter gain reduction assertion failed");
			return;
		}
	} else if (command.op=="expect_render_mode") {
		if (!ExpectSimRenderMode(command.value)) {
			FailSimScript("render mode assertion failed");
			return;
		}
	} else if (command.op=="expect_instrument_sample") {
		if (!ExpectSimInstrumentSample(command.value,command.arg)) {
			FailSimScript("instrument sample assertion failed");
			return;
		}
	} else if (command.op=="expect_instrument_root") {
		if (!ExpectSimInstrumentRoot(command.value,atoi(command.arg.c_str()))) {
			FailSimScript("instrument root assertion failed");
			return;
		}
	} else if (command.op=="expect_instrument_root_suggestion") {
		if (!ExpectSimInstrumentRootSuggestion(command.value,atoi(command.arg.c_str()))) {
			FailSimScript("instrument root suggestion assertion failed");
			return;
		}
	} else if (command.op=="expect_sample_trim") {
		if (!ExpectSimSampleTrim(command.value,command.value2,atoi(command.arg.c_str()),atoi(command.arg2.c_str()))) {
			FailSimScript("sample trim assertion failed");
			return;
		}
	} else if (command.op=="expect_sample_trim_order") {
		if (!ExpectSimSampleTrimOrder(command.value)) {
			FailSimScript("sample trim order assertion failed");
			return;
		}
	} else if (command.op=="sim_set_tempo") {
		if (!SimSetTempo(command.value)) {
			FailSimScript("tempo setup failed");
			return;
		}
	} else if (command.op=="sim_set_render_mode") {
		if (!SimSetRenderMode(command.value)) {
			FailSimScript("render mode setup failed");
			return;
		}
	} else if (command.op=="sim_set_note_names") {
		if (!SimSetNoteNames(command.arg)) {
			FailSimScript("note name setup failed");
			return;
		}
	} else if (command.op=="sim_set_scale") {
		if (!SimSetScale(command.value)) {
			FailSimScript("scale setup failed");
			return;
		}
	} else if (command.op=="sim_set_key") {
		if (!SimSetScaleKey(command.value)) {
			FailSimScript("scale key setup failed");
			return;
		}
	} else if (command.op=="sim_import_sample_to_instrument") {
		if (!SimImportSampleToInstrument(command.value,command.arg)) {
			FailSimScript("sample import setup failed");
			return;
		}
	} else if (command.op=="sim_set_sample_trim") {
		if (!SimSetSampleTrim(command.value,command.value2,atoi(command.arg.c_str()))) {
			FailSimScript("sample trim setup failed");
			return;
		}
	} else if (command.op=="sim_detect_trim_root") {
		if (!SimDetectTrimRoot(command.value,atoi(command.arg.c_str()))) {
			FailSimScript("trim root detection failed");
			return;
		}
	} else if (command.op=="sim_set_song_chain") {
		if (!SimSetSongChain(command.value,command.value2,atoi(command.arg.c_str()))) {
			FailSimScript("song setup failed");
			return;
		}
	} else if (command.op=="sim_set_chain_phrase") {
		if (!SimSetChainPhrase(command.value,command.value2,atoi(command.arg.c_str()),atoi(command.arg2.c_str()))) {
			FailSimScript("chain setup failed");
			return;
		}
	} else if (command.op=="sim_set_phrase_note") {
		if (!SimSetPhraseNote(command.value,command.value2,atoi(command.arg.c_str()),atoi(command.arg2.c_str()))) {
			FailSimScript("phrase setup failed");
			return;
		}
	} else if (command.op=="sim_set_synth") {
		if (!SimSetSynth(command.value,command.arg)) {
			FailSimScript("synth setup failed");
			return;
		}
	} else if (command.op=="sim_set_macro") {
		if (!SimSetMacro(command.value,command.arg)) {
			FailSimScript("macro setup failed");
			return;
		}
	} else if (command.op=="sim_set_instrument_param") {
		if (!SimSetInstrumentParam(command.value,command.arg,command.arg2)) {
			FailSimScript("instrument param setup failed");
			return;
		}
	} else if (command.op=="sim_load_instrument_param") {
		if (!SimLoadInstrumentParam(command.value,command.arg,command.arg2)) {
			FailSimScript("instrument param load failed");
			return;
		}
	} else if (command.op=="expect_sample_stats") {
		if (!ExpectSimSampleStats(command.arg,command.arg2,command.value,command.value2)) {
			FailSimScript("sample stats assertion failed");
			return;
		}
	} else if (command.op=="expect_instrument_type") {
		if (!ExpectSimInstrumentType(command.value,command.arg)) {
			FailSimScript("instrument type assertion failed");
			return;
		}
	} else if (command.op=="expect_instrument_name") {
		if (!ExpectSimInstrumentName(command.value,command.arg)) {
			FailSimScript("instrument name assertion failed");
			return;
		}
	} else if (command.op=="expect_instrument_param") {
		if (!ExpectSimInstrumentParam(command.value,command.arg,command.arg2)) {
			FailSimScript("instrument param assertion failed");
			return;
		}
	} else if (command.op=="expect_player" || command.op=="expect_notes_repeat" ||
	           command.op=="expect_project_param" || command.op=="sim_set_project_param" ||
	           command.op=="expect_bookmark" || command.op=="expect_track_muted" ||
	           command.op=="expect_phrase_note" || command.op=="expect_mixer_level" ||
	           command.op=="sim_set_mixer_level" || command.op=="expect_phrase_in_scale") {
		if (!RunSimSequencerCheck(command.op,command.arg,command.arg2,command.value,command.value2)) {
			FailSimScript("sequencer check failed");
			return;
		}
	} else if (command.op=="sim_set_phrase_command") {
		if (!SimSetPhraseCommand(command.value,command.value2,atoi(command.arg.c_str()),command.arg2,command.arg3)) {
			FailSimScript("phrase command setup failed");
			return;
		}
	} else if (command.op=="sim_set_table_command") {
		if (!SimSetTableCommand(command.value,command.value2,atoi(command.arg.c_str()),command.arg2,command.arg3)) {
			FailSimScript("table command setup failed");
			return;
		}
	} else if (command.op=="sim_save_project") {
		if (!SimSaveProject()) {
			FailSimScript("project save failed");
			return;
		}
	} else if (command.op=="sim_dump_song") {
		if (!SimDumpSong(command.arg)) {
			FailSimScript("song dump failed");
			return;
		}
	} else if (command.op=="expect_song_dump") {
		if (!ExpectSimSongDump(command.arg)) {
			FailSimScript("song differs from the dump");
			return;
		}
	} else if (command.op=="expect_colors") {
		if (!ExpectSimScreenColors(window,command.value)) {
			FailSimScript("screen color assertion failed");
			return;
		}
	} else if (command.op=="expect_size") {
		if (!ExpectSimScreenSize(window,command.value,command.value2)) {
			FailSimScript("screen size assertion failed");
			return;
		}
	} else if (command.op=="expect_skin_frame_clean") {
		if (!ExpectSimSkinFrameClean(window)) {
			FailSimScript("skin frame assertion failed");
			return;
		}
	} else if (command.op=="log") {
		Trace::Log("RGNANO_SIM","%s",command.arg.c_str());
	} else if (command.op=="dump_state") {
		LogSimState(command.arg.empty() ? "dump_state" : command.arg.c_str(),true);
	} else {
		FailSimScript("unknown script command");
		return;
	}

	simCommandIndex_++;
	simNextCommandTime_=now+40;
	LogSimState("after command",false);
}

bool SDLEventManager::HandleSimMouse(SDLGUIWindowImp *window, SDL_Event &event)
{
	if (!window || !window->IsRGNanoSkinEnabled()) {
		return false;
	}
	if (event.type==SDL_MOUSEBUTTONDOWN) {
		int key=GetSimButtonAt(event.button.x,event.button.y);
		if (key>0) {
			keyboardCS_->SetKey(key,true);
			window->SetRGNanoButtonPressed(key,true);
			simMouseKey_=key;
			Trace::Log("RGNANO_SIM","mouse down key:%d",key);
			return true;
		}
	} else if (event.type==SDL_MOUSEBUTTONUP) {
		if (simMouseKey_>0) {
			keyboardCS_->SetKey(simMouseKey_,false);
			window->SetRGNanoButtonPressed(simMouseKey_,false);
			Trace::Log("RGNANO_SIM","mouse up key:%d",simMouseKey_);
			simMouseKey_=0;
			return true;
		}
	}
	return false;
}

void SDLEventManager::SetSimKey(SDLGUIWindowImp *window, int key, bool pressed)
{
	SDL_Event event;
	memset(&event,0,sizeof(event));
	event.type=pressed ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.type=event.type;
	event.key.state=pressed ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.sym=(SDLKey)key;
	if (SDL_PushEvent(&event)!=0) {
		Trace::Error("RGNANO_SIM failed to push key event:%d",key);
	}
}

int SDLEventManager::GetSimButtonAt(int x, int y)
{
	struct ButtonRect {
		int x;
		int y;
		int w;
		int h;
		int key;
	};
	ButtonRect buttons[] = {
		{76,374,28,28,SDLK_u},
		{76,434,28,28,SDLK_d},
		{46,404,28,28,SDLK_l},
		{106,404,28,28,SDLK_r},
		{250,444,30,30,SDLK_a},
		{286,408,30,30,SDLK_b},
		{214,408,30,30,SDLK_x},
		{250,372,30,30,SDLK_y},
		{88,498,62,18,SDLK_s},
		{210,498,62,18,SDLK_q},
		{12,12,88,22,SDLK_m},
		{12,12,22,42,SDLK_m},
		{260,12,88,22,SDLK_n},
		{326,12,22,42,SDLK_n}
	};
	for (int i=0;i<(int)(sizeof(buttons)/sizeof(buttons[0]));i++) {
		if (x>=buttons[i].x && x<buttons[i].x+buttons[i].w &&
			y>=buttons[i].y && y<buttons[i].y+buttons[i].h) {
			return buttons[i].key;
		}
	}
	return 0;
}

void SDLEventManager::FailSimScript(const char *message)
{
	LogSimState("failure",true);
	simScriptFailed_=true;
	Trace::Error("RGNANO_SIM %s",message);
	PostQuitMessage();
}

void SDLEventManager::SaveSimScreenshot(SDLGUIWindowImp *window, const std::string &path)
{
	if (!window || path.empty()) {
		return;
	}
	window->Flush();
	RenderPowerMenu(window->GetSurface(),window);
	RenderDebugScreen(window->GetSurface(),window);
	if (SDL_SaveBMP(window->GetSurface(),path.c_str())==0) {
		Trace::Log("RGNANO_SIM","Saved screenshot %s",path.c_str());
	} else {
		Trace::Error("RGNANO_SIM failed screenshot %s",path.c_str());
	}
}

void SDLEventManager::SaveSimAppScreenshot(SDLGUIWindowImp *window, const std::string &path)
{
	if (!window || path.empty()) {
		return;
	}
	window->Flush();
	RenderPowerMenu(window->GetSurface(),window);
	RenderDebugScreen(window->GetSurface(),window);

	SDL_Surface *surface=window->GetSurface();
	if (!surface) {
		Trace::Error("RGNANO_SIM failed app screenshot %s: no surface",path.c_str());
		return;
	}
	int scale=window->GetScale();
	SDL_Rect src;
	src.x=window->IsRGNanoSkinEnabled()?window->GetAppAnchorX():0;
	src.y=window->IsRGNanoSkinEnabled()?window->GetAppAnchorY():0;
	src.w=240*scale;
	src.h=240*scale;
	SDL_Surface *app=SDL_CreateRGBSurface(SDL_SWSURFACE,src.w,src.h,surface->format->BitsPerPixel,
		surface->format->Rmask,surface->format->Gmask,surface->format->Bmask,surface->format->Amask);
	if (!app) {
		Trace::Error("RGNANO_SIM failed app screenshot %s: cannot allocate crop",path.c_str());
		return;
	}
	SDL_Rect dst;
	dst.x=0;
	dst.y=0;
	dst.w=src.w;
	dst.h=src.h;
	SDL_BlitSurface(surface,&src,app,&dst);
	if (SDL_SaveBMP(app,path.c_str())==0) {
		Trace::Log("RGNANO_SIM","Saved app screenshot %s",path.c_str());
	} else {
		Trace::Error("RGNANO_SIM failed app screenshot %s",path.c_str());
	}
	SDL_FreeSurface(app);
}

bool SDLEventManager::ExpectSimFile(const std::string &path)
{
	if (path.empty()) {
		Trace::Error("RGNANO_SIM expect_file missing path");
		return false;
	}
	std::ifstream file(path.c_str(),std::ios::binary);
	bool exists=file.good();
	Trace::Log("RGNANO_SIM","expect_file %s => %s",path.c_str(),exists?"exists":"missing");
	return exists;
}

bool SDLEventManager::ExpectSimProjectFileBytes(const std::string &path, int minBytes)
{
	if (path.empty() || minBytes<0) {
		Trace::Error("RGNANO_SIM expect_project_file_bytes invalid args path=%s minBytes=%d",path.c_str(),minBytes);
		return false;
	}
	std::string aliasPath="project:";
	aliasPath+=path;
	Path projectPath(aliasPath.c_str());
	std::ifstream file(projectPath.GetPath().c_str(),std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		Trace::Log("RGNANO_SIM","expect_project_file_bytes %s => missing (%s)",path.c_str(),projectPath.GetPath().c_str());
		return false;
	}
	std::streamoff size=file.tellg();
	bool matches=size>=minBytes;
	Trace::Log("RGNANO_SIM","expect_project_file_bytes %s size=%ld min=%d => %s",projectPath.GetPath().c_str(),(long)size,minBytes,matches?"ok":"too small");
	return matches;
}

bool SDLEventManager::ExpectSimProjectSample(const std::string &sampleName)
{
	if (sampleName.empty()) {
		Trace::Error("RGNANO_SIM expect_project_sample missing sample name");
		return false;
	}
	std::string aliasPath="samples:";
	aliasPath+=sampleName;
	Path samplePath(aliasPath.c_str());
	Path resolvedPath(samplePath.GetPath());
	bool exists=resolvedPath.Exists();
	Trace::Log("RGNANO_SIM","expect_project_sample %s => %s (%s)",sampleName.c_str(),exists?"exists":"missing",resolvedPath.GetPath().c_str());
	return exists;
}

static std::string GetSimLogPath()
{
	const char *logPath=Config::GetInstance()->GetValue("RGNANOSIM_LOG");
	if (!logPath) {
		return "rgnano-sim.log";
	}
	std::string path(logPath);
	std::ifstream file(path.c_str());
	if (file.is_open()) {
		return path;
	}
	std::string withExtension=path + ".log";
	std::ifstream extensionFile(withExtension.c_str());
	if (extensionFile.is_open()) {
		return withExtension;
	}
	return path;
}

bool SDLEventManager::ExpectSimLog(const std::string &needle)
{
	std::string logPath=GetSimLogPath();
	std::ifstream file(logPath.c_str());
	if (!file.is_open()) {
		Trace::Error("RGNANO_SIM expect_log failed to open %s",logPath.c_str());
		return false;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string haystack=buffer.str();
	bool found=haystack.find(needle)!=std::string::npos;
	Trace::Log("RGNANO_SIM","expect_log %s in %s => %s",needle.c_str(),logPath.c_str(),found?"found":"missing");
	return found;
}

bool SDLEventManager::ExpectSimNoError()
{
	std::string logPath=GetSimLogPath();
	std::ifstream file(logPath.c_str());
	if (!file.is_open()) {
		Trace::Error("RGNANO_SIM expect_no_error failed to open %s",logPath.c_str());
		return false;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string haystack=buffer.str();
	bool clean=haystack.find("[*ERROR*]")==std::string::npos && haystack.find("ERROR") == std::string::npos;
	Trace::Log("RGNANO_SIM","expect_no_error in %s => %s",logPath.c_str(),clean?"clean":"error found");
	return clean;
}

bool SDLEventManager::ExpectSimScreensDiffer(const std::string &firstPath, const std::string &secondPath)
{
	if (firstPath.empty() || secondPath.empty()) {
		Trace::Error("RGNANO_SIM expect_screens_differ missing path");
		return false;
	}
	std::ifstream first(firstPath.c_str(),std::ios::binary);
	std::ifstream second(secondPath.c_str(),std::ios::binary);
	if (!first.is_open() || !second.is_open()) {
		Trace::Error("RGNANO_SIM expect_screens_differ failed to open %s or %s",firstPath.c_str(),secondPath.c_str());
		return false;
	}
	std::vector<char> firstBytes((std::istreambuf_iterator<char>(first)),std::istreambuf_iterator<char>());
	std::vector<char> secondBytes((std::istreambuf_iterator<char>(second)),std::istreambuf_iterator<char>());
	bool differs=firstBytes.size()!=secondBytes.size();
	if (!differs) {
		for (size_t i=0;i<firstBytes.size();i++) {
			if (firstBytes[i]!=secondBytes[i]) {
				differs=true;
				break;
			}
		}
	}
	Trace::Log("RGNANO_SIM","expect_screens_differ %s %s => %s",firstPath.c_str(),secondPath.c_str(),differs?"different":"same");
	if (!differs) {
		Trace::Error("RGNANO_SIM expected screenshots to differ");
	}
	return differs;
}

void SDLEventManager::PressSimCombo(SDLGUIWindowImp *window, int modifier, int key)
{
	if (modifier>0) {
		SetSimKey(window,modifier,true);
		simGoalRelease_.push_back(modifier);
	}
	SetSimKey(window,key,true);
	simGoalRelease_.push_back(key);
	simNextCommandTime_=System::GetInstance()->GetClock()+80;
	simGoalSteps_++;
}

// Shortest R+Dpad route between screens (source-derived view graph)
static const char *simNextHop(const std::string &from, const std::string &to, int *dirKey)
{
	struct Edge { const char *from; int key; const char *to; };
	static const Edge edges[]={
		{"song",SDLK_u,"project"},{"song",SDLK_d,"mixer"},{"song",SDLK_r,"chain"},
		{"project",SDLK_d,"song"},{"mixer",SDLK_u,"song"},{"mixer",SDLK_d,"fx"},{"fx",SDLK_u,"mixer"},{"fx",SDLK_r,"eq"},{"eq",SDLK_l,"fx"},{"eq",SDLK_r,"limit"},{"limit",SDLK_l,"eq"},{"project",SDLK_r,"scale"},{"scale",SDLK_l,"project"},
		{"chain",SDLK_l,"song"},{"chain",SDLK_r,"phrase"},
		{"phrase",SDLK_l,"chain"},{"phrase",SDLK_r,"instrument"},
		{"phrase",SDLK_d,"table"},{"phrase",SDLK_u,"groove"},
		{"groove",SDLK_d,"phrase"},{"table",SDLK_u,"phrase"},
		{"instrument",SDLK_l,"phrase"},{"instrument",SDLK_d,"table"},
	};
	const int count=sizeof(edges)/sizeof(Edge);
	const char *nodes[]={"song","project","mixer","chain","phrase","instrument","table","groove","fx","eq","limit","scale"};
	const int nodeCount=12;
	int prev[12];
	int via[12];
	bool seen[12];
	int start=-1;
	int goal=-1;
	for (int i=0;i<nodeCount;i++) {
		seen[i]=false;
		prev[i]=-1;
		via[i]=0;
		if (from==nodes[i]) start=i;
		if (to==nodes[i]) goal=i;
	}
	if (start<0 || goal<0) return 0;
	int queue[12];
	int head=0;
	int tail=0;
	queue[tail++]=start;
	seen[start]=true;
	while (head<tail) {
		int n=queue[head++];
		for (int e=0;e<count;e++) {
			if (strcmp(edges[e].from,nodes[n])) continue;
			int m=-1;
			for (int k=0;k<nodeCount;k++) {
				if (!strcmp(edges[e].to,nodes[k])) m=k;
			}
			if (m<0 || seen[m]) continue;
			seen[m]=true;
			prev[m]=n;
			via[m]=edges[e].key;
			queue[tail++]=m;
		}
	}
	if (!seen[goal]) return 0;
	int step=goal;
	while (prev[step]!=start) step=prev[step];
	*dirKey=via[step];
	return nodes[step];
}

static std::string simLower(const std::string &s)
{
	std::string out=s;
	for (size_t i=0;i<out.size();i++) out[i]=(char)tolower((unsigned char)out[i]);
	return out;
}

// Goal commands: 0 = reached, 1 = pressed one combo (call again), -1 = failed.
// Each call reads the live app state, so scripts say what they want rather
// than how many times to press a button.
int SDLEventManager::StepSimGoal(SDLGUIWindowImp *window, SimCommand &command)
{
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (!appWindow) return -1;
	std::string view=appWindow->GetCurrentViewName();
	std::string state=view+"|"+appWindow->GetSimSelectionSummary();
	Variable *focused=appWindow->GetSimFocusedVariable();
	if (focused) state+=std::string("|")+focused->GetString();
	ViewData *viewData=GetSimViewData();
	if (viewData) {
		char buf[16];
		sprintf(buf,"|i%d",viewData->currentInstrument_);
		state+=buf;
	}
	if (simGoalSteps_>0 && state==simGoalLastState_) {
		Trace::Error("RGNANO_SIM %s %s: last input changed nothing (view=%s focus=%s)",
		             command.op.c_str(),command.arg.c_str(),view.c_str(),appWindow->GetSimFocusedText().c_str());
		return -1;
	}
	if (simGoalSteps_>300) {
		Trace::Error("RGNANO_SIM %s %s: gave up after %d steps",command.op.c_str(),command.arg.c_str(),simGoalSteps_);
		return -1;
	}
	simGoalLastState_=state;

	if (command.op=="goto") {
		if (view==command.arg) return 0;
		int key=0;
		if (!simNextHop(view,command.arg,&key)) {
			Trace::Error("RGNANO_SIM goto %s: no route from %s",command.arg.c_str(),view.c_str());
			return -1;
		}
		PressSimCombo(window,SDLK_n,key);
		return 1;
	}

	if (command.op=="instrument") {
		if (!viewData) return -1;
		if (view!="instrument") {
			Trace::Error("RGNANO_SIM instrument %s: goto instrument first",command.arg.c_str());
			return -1;
		}
		int target=(int)strtol(command.arg.c_str(),0,16);
		int current=viewData->currentInstrument_;
		if (current==target) return 0;
		int delta=target-current;
		if (delta>=16) PressSimCombo(window,SDLK_b,SDLK_d);
		else if (delta<=-16) PressSimCombo(window,SDLK_b,SDLK_u);
		else PressSimCombo(window,SDLK_b,delta>0?SDLK_r:SDLK_l);
		return 1;
	}

	if (command.op=="row") {
		// Cursor row (hex) on the Song, Chain or Phrase grid
		if (!viewData) return -1;
		int target=(int)strtol(command.arg.c_str(),0,16);
		int current=-1;
		if (view=="song") current=viewData->songOffset_+viewData->songY_;
		else if (view=="chain") current=viewData->chainRow_;
		else if (view=="phrase") current=viewData->phraseCurPos_;
		if (current<0) {
			Trace::Error("RGNANO_SIM row %s: not supported on %s",command.arg.c_str(),view.c_str());
			return -1;
		}
		if (current==target) return 0;
		PressSimCombo(window,0,target>current?SDLK_d:SDLK_u);
		return 1;
	}

	if (command.op=="page") {
		// Instrument pages show "<LB n/7 NAME LB>"
		std::string wanted=std::string(" ")+command.arg+" LB>";
		if (appWindow->ScreenContains(wanted.c_str())) return 0;
		PressSimCombo(window,SDLK_m,SDLK_r);
		return 1;
	}

	// focus / set: move the field cursor until the highlighted row starts with the label
	std::string label=simLower(command.arg);
	std::string text=simLower(appWindow->GetSimFocusedText());
	bool onLabel=text.compare(0,label.size(),label)==0 &&
	             (text.size()==label.size() || text[label.size()]==' ' || text[label.size()]==':');
	if (!onLabel) {
		if (simGoalSteps_>60) {
			Trace::Error("RGNANO_SIM %s %s: no field with that label on %s",command.op.c_str(),command.arg.c_str(),view.c_str());
			return -1;
		}
		PressSimCombo(window,0,SDLK_d);
		return 1;
	}
	if (command.op=="focus") return 0;

	if (!focused) {
		Trace::Error("RGNANO_SIM set %s: focused field has no value",command.arg.c_str());
		return -1;
	}
	std::string target=command.arg2;
	if (focused->GetType()==Variable::CHAR_LIST) {
		// List entries with spaces are written with '_' (fm_bell)
		std::replace(target.begin(),target.end(),'_',' ');
		if (simLower(focused->GetString())==simLower(target)) return 0;
		// Lists do not wrap: step toward the wanted entry
		int wantedIndex=-1;
		char **list=focused->GetListPointer();
		for (int k=0;list && k<focused->GetListSize();k++) {
			if (list[k] && simLower(list[k])==simLower(target)) wantedIndex=k;
		}
		if (wantedIndex<0) {
			Trace::Error("RGNANO_SIM set %s: '%s' is not an option",command.arg.c_str(),target.c_str());
			return -1;
		}
		PressSimCombo(window,SDLK_a,wantedIndex>focused->GetInt()?SDLK_r:SDLK_l);
		return 1;
	}
	if (focused->GetType()==Variable::BOOL) {
		if (simLower(focused->GetString())==simLower(target)) return 0;
		PressSimCombo(window,SDLK_a,focused->GetBool()?SDLK_l:SDLK_r);
		return 1;
	}
	int wanted=(int)strtol(target.c_str(),0,0);
	int current=focused->GetInt();
	if (current==wanted) return 0;
	int delta=wanted-current;
	if (delta>=16) PressSimCombo(window,SDLK_a,SDLK_u);
	else if (delta<=-16) PressSimCombo(window,SDLK_a,SDLK_d);
	else PressSimCombo(window,SDLK_a,delta>0?SDLK_r:SDLK_l);
	return 1;
}

bool SDLEventManager::ExpectSimView(const std::string &viewName)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	const char *current=appWindow ? appWindow->GetCurrentViewName() : "unknown";
	bool matches=viewName==current;
	Trace::Log("RGNANO_SIM","expect_view %s => %s",viewName.c_str(),current);
	if (!matches) {
		Trace::Error("RGNANO_SIM expected view %s but found %s",viewName.c_str(),current);
	}
	return matches;
}

bool SDLEventManager::ExpectSimSelectedText(const std::string &needle)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	std::string selected=appWindow ? appWindow->GetSimSelectionSummary() : "";
	if (showPowerMenu_) {
		selected += showExitConfirm_ ?
			(exitConfirmSelection_==0 ? "; powerConfirm=Yes" : "; powerConfirm=No") :
			(std::string("; powerMenu=")+powerMenuSelectedLabel(powerMenuSelection_));
	}
	if (showDebugScreen_) {
		const char *debugItems[]={"Audio Input Monitor","System Info","Exit Debug"};
		selected += "; debug=";
		selected += debugItems[debugScreenSelection_];
	}
	if (menuHelpOverlay_) {
		selected += "; helper=";
		selected += showDebugScreen_ ? "DEBUG" : (showExitConfirm_ ? "EXIT?" : "POWER");
		selected += menuHelpPage_ == 0 ? ":MAP" : ":COMMANDS";
	}
	bool found=!needle.empty() && selected.find(needle)!=std::string::npos;
	Trace::Log("RGNANO_SIM","expect_selected_text %s in %s => %s",needle.c_str(),selected.c_str(),found?"found":"missing");
	if (!found) {
		Trace::Error("RGNANO_SIM expected selected text %s",needle.c_str());
	}
	return found;
}

static const char *SimExpectedPlayModeName(PlayMode mode)
{
	switch (mode) {
		case PM_SONG: return "song";
		case PM_CHAIN: return "chain";
		case PM_PHRASE: return "phrase";
		case PM_LIVE: return "live";
		case PM_AUDITION: return "audition";
		default: return "unknown";
	}
}

bool SDLEventManager::ExpectSimPlayerRunning(const std::string &expected)
{
	Player *player=Player::GetInstance();
	bool actual=player && player->IsRunning();
	bool wantRunning=expected.empty() || expected=="yes" || expected=="true" || expected=="1" || expected=="running";
	bool wantStopped=expected=="no" || expected=="false" || expected=="0" || expected=="stopped";
	if (!wantRunning && !wantStopped) {
		Trace::Error("RGNANO_SIM expect_player_running invalid expected=%s",expected.c_str());
		return false;
	}
	bool matches=actual==wantRunning;
	Trace::Log("RGNANO_SIM","expect_player_running actual=%s expected=%s => %s",actual?"yes":"no",wantRunning?"yes":"no",matches?"match":"mismatch");
	if (!matches && player) {
		Trace::Log("RGNANO_SIM_PLAYER","%s",player->GetSimDebugSummary().c_str());
	}
	return matches;
}

bool SDLEventManager::ExpectSimPlayMode(const std::string &modeName)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || modeName.empty()) {
		Trace::Error("RGNANO_SIM expect_play_mode invalid state or mode=%s",modeName.c_str());
		return false;
	}
	const char *actual=SimExpectedPlayModeName(viewData->playMode_);
	bool matches=modeName==actual;
	Trace::Log("RGNANO_SIM","expect_play_mode actual=%s expected=%s => %s",actual,modeName.c_str(),matches?"match":"mismatch");
	if (!matches) {
		Player *player=Player::GetInstance();
		if (player) {
			Trace::Log("RGNANO_SIM_PLAYER","%s",player->GetSimDebugSummary().c_str());
		}
	}
	return matches;
}

bool SDLEventManager::ExpectSimStreamingSample(const std::string &needle)
{
	Player *player=Player::GetInstance();
	if (!player) {
		Trace::Error("RGNANO_SIM expect_streaming_sample has no player");
		return false;
	}
	std::string path=player->GetSimStreamingPath();
	bool matches=player->IsSimStreaming() && !needle.empty() && path.find(needle)!=std::string::npos;
	Trace::Log("RGNANO_SIM","expect_streaming_sample actual=%s streaming=%s expected=%s => %s",path.c_str(),player->IsSimStreaming()?"yes":"no",needle.c_str(),matches?"match":"mismatch");
	if (!matches) {
		Trace::Log("RGNANO_SIM_PLAYER","%s",player->GetSimDebugSummary().c_str());
	}
	return matches;
}

bool SDLEventManager::ExpectSimPlayingChannel(int channel, const std::string &instrument)
{
	Player *player=Player::GetInstance();
	if (!player || channel<0 || channel>=SONG_CHANNEL_COUNT) {
		Trace::Error("RGNANO_SIM expect_playing_channel invalid channel=%d",channel);
		return false;
	}
	bool playing=player->IsChannelPlaying(channel);
	std::string actualInstrument=player->GetPlayedInstrument(channel);
	bool matches=playing && (instrument.empty() || actualInstrument.find(instrument)!=std::string::npos);
	Trace::Log("RGNANO_SIM","expect_playing_channel channel=%d playing=%s inst=%s expectedInst=%s => %s",channel,playing?"yes":"no",actualInstrument.c_str(),instrument.c_str(),matches?"match":"mismatch");
	if (!matches) {
		Trace::Log("RGNANO_SIM_PLAYER","%s",player->GetSimDebugSummary().c_str());
	}
	return matches;
}

// The song row a track is playing (hex), e.g. after a live-mode cue
bool SDLEventManager::ExpectSimChannelRow(int channel, const std::string &hexRow)
{
	Player *player=Player::GetInstance();
	ViewData *viewData=GetSimViewData();
	if (!player || !viewData || channel<0 || channel>=SONG_CHANNEL_COUNT) {
		Trace::Error("RGNANO_SIM expect_channel_row invalid channel=%d",channel);
		return false;
	}
	int wanted=(int)strtol(hexRow.c_str(),0,16);
	bool playing=player->IsChannelPlaying(channel);
	int actual=viewData->songPlayPos_[channel];
	bool matches=playing && actual==wanted;
	Trace::Log("RGNANO_SIM","expect_channel_row channel=%d playing=%s row=%02X expected=%02X => %s",channel,playing?"yes":"no",actual,wanted,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimAudioActivity(int minPeak)
{
	if (minPeak<=0) {
		minPeak=64;
	}
	int peak=AudioDriver::GetSimAudioPeak();
	unsigned long nonSilentBytes=AudioDriver::GetSimAudioNonSilentBytes();
	bool active=peak>=minPeak && nonSilentBytes>0;
	Trace::Log("RGNANO_SIM","expect_audio_activity peak=%d nonSilentBytes=%lu minPeak=%d => %s",peak,nonSilentBytes,minPeak,active?"active":"silent");
	if (!active) {
		Trace::Error("RGNANO_SIM audio activity assertion failed");
	}
	return active;
}

bool SDLEventManager::ExpectSimAudioSilence(int maxPeak)
{
	if (maxPeak<0) {
		maxPeak=0;
	}
	int peak=AudioDriver::GetSimAudioPeak();
	unsigned long nonSilentBytes=AudioDriver::GetSimAudioNonSilentBytes();
	bool silent=peak<=maxPeak && nonSilentBytes==0;
	Trace::Log("RGNANO_SIM","expect_audio_silence peak=%d nonSilentBytes=%lu maxPeak=%d => %s",peak,nonSilentBytes,maxPeak,silent?"silent":"active");
	if (!silent) {
		Trace::Error("RGNANO_SIM audio silence assertion failed");
	}
	return silent;
}

// Sound is playing, but its loudest sample since reset_audio_stats stays
// at or under maxPeak (e.g. the limiter's ceiling, an EQ cut)
bool SDLEventManager::ExpectSimAudioPeakMax(int maxPeak)
{
	int peak=AudioDriver::GetSimAudioPeak();
	unsigned long nonSilentBytes=AudioDriver::GetSimAudioNonSilentBytes();
	bool ok=peak<=maxPeak && nonSilentBytes>0;
	Trace::Log("RGNANO_SIM","expect_audio_peak_max peak=%d nonSilentBytes=%lu maxPeak=%d => %s",peak,nonSilentBytes,maxPeak,ok?"match":"mismatch");
	if (!ok) {
		Trace::Error("RGNANO_SIM audio peak assertion failed");
	}
	return ok;
}

bool SDLEventManager::ExpectSimAudioCaptureBytes(int minBytes)
{
	if (minBytes<=0) {
		minBytes=44;
	}
	unsigned long bytes=AudioDriver::GetSimAudioCaptureBytes();
	bool captured=bytes>=(unsigned long)minBytes;
	Trace::Log("RGNANO_SIM","expect_audio_capture_bytes bytes=%lu minBytes=%d => %s",bytes,minBytes,captured?"captured":"short");
	if (!captured) {
		Trace::Error("RGNANO_SIM audio capture assertion failed");
	}
	return captured;
}

bool SDLEventManager::ExpectSimScreenText(const std::string &needle)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	bool found=appWindow && appWindow->ScreenContains(needle.c_str());
	Trace::Log("RGNANO_SIM","expect_screen_text %s => %s",needle.c_str(),found?"found":"missing");
	if (!found) {
		Trace::Error("RGNANO_SIM expected screen text %s",needle.c_str());
	}
	return found;
}

static bool ParseSimExpectedByte(const std::string &expected, unsigned char actual)
{
	if (expected=="any") {
		return actual!=0xFF;
	}
	char *end=0;
	long value=strtol(expected.c_str(),&end,16);
	return end && *end==0 && value>=0 && value<=0xFF && actual==(unsigned char)value;
}

static bool ParseSimHexWord(const std::string &text, ushort *out)
{
	if (!out || text.empty()) {
		return false;
	}
	char *end=0;
	long value=strtol(text.c_str(),&end,16);
	if (!end || *end!=0 || value<0 || value>0xFFFF) {
		return false;
	}
	*out=(ushort)value;
	return true;
}

static bool ParseSimCommandName(const std::string &name, FourCC *out)
{
	if (!out || name.empty()) {
		return false;
	}
	for (int i=0;i<CommandList::GetCount();i++) {
		FourCC command=CommandList::GetAt(i);
		char buffer[5];
		fourCC2char(command,buffer);
		buffer[4]=0;
		// "NTH " can be written NTH or NTH_ in a script
		std::string padded=name;
		std::replace(padded.begin(),padded.end(),'_',' ');
		while (padded.size()<4) padded+=' ';
		if (name==buffer || padded==buffer) {
			*out=command;
			return true;
		}
	}
	return false;
}

bool SDLEventManager::ExpectSimSongChain(int row, int channel, const std::string &expected)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	ViewData *viewData=appWindow ? appWindow->GetViewData() : 0;
	if (!viewData || !viewData->song_ || row<0 || row>=SONG_ROW_COUNT || channel<0 || channel>=SONG_CHANNEL_COUNT) {
		Trace::Error("RGNANO_SIM expect_song_chain invalid args row=%d channel=%d",row,channel);
		return false;
	}
	unsigned char actual=*(viewData->song_->data_ + channel + SONG_CHANNEL_COUNT * row);
	bool matches=ParseSimExpectedByte(expected,actual);
	Trace::Log("RGNANO_SIM","expect_song_chain row=%d channel=%d actual=%02X expected=%s => %s",row,channel,actual,expected.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimChainPhrase(int chain, int row, const std::string &expected)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	ViewData *viewData=appWindow ? appWindow->GetViewData() : 0;
	if (!viewData || !viewData->song_ || chain<0 || chain>=CHAIN_COUNT || row<0 || row>=16) {
		Trace::Error("RGNANO_SIM expect_chain_phrase invalid args chain=%d row=%d",chain,row);
		return false;
	}
	unsigned char actual=*(viewData->song_->chain_->data_ + 16 * chain + row);
	bool matches=ParseSimExpectedByte(expected,actual);
	Trace::Log("RGNANO_SIM","expect_chain_phrase chain=%d row=%d actual=%02X expected=%s => %s",chain,row,actual,expected.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimPhraseRowCount(int phrase, int minRows)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	ViewData *viewData=appWindow ? appWindow->GetViewData() : 0;
	if (!viewData || !viewData->song_ || phrase<0 || phrase>=PHRASE_COUNT || minRows<0) {
		Trace::Error("RGNANO_SIM expect_phrase_row_count invalid args phrase=%d minRows=%d",phrase,minRows);
		return false;
	}
	int count=0;
	for (int row=0; row<16; row++) {
		unsigned char note=*(viewData->song_->phrase_->note_ + 16 * phrase + row);
		unsigned char instr=*(viewData->song_->phrase_->instr_ + 16 * phrase + row);
		if (note!=0xFF && instr!=0xFF) {
			count++;
		}
	}
	bool matches=count>=minRows;
	Trace::Log("RGNANO_SIM","expect_phrase_row_count phrase=%d count=%d min=%d => %s",phrase,count,minRows,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimPhraseCommand(int phrase, int row, int slot, const std::string &expected)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->song_ || phrase<0 || phrase>=PHRASE_COUNT || row<0 || row>=16 || slot<1 || slot>2 || expected.empty()) {
		Trace::Error("RGNANO_SIM expect_phrase_command invalid args phrase=%d row=%d slot=%d expected=%s",phrase,row,slot,expected.c_str());
		return false;
	}
	FourCC actual=(slot==1)
		? *(viewData->song_->phrase_->cmd1_ + 16 * phrase + row)
		: *(viewData->song_->phrase_->cmd2_ + 16 * phrase + row);
	char buffer[5];
	fourCC2char(actual,buffer);
	buffer[4]=0;
	bool matches=expected==buffer;
	Trace::Log("RGNANO_SIM","expect_phrase_command phrase=%02X row=%d slot=%d actual=%s expected=%s => %s",phrase,row,slot,buffer,expected.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimPhraseParam(int phrase, int row, int slot, const std::string &expected)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->song_ || phrase<0 || phrase>=PHRASE_COUNT || row<0 || row>=16 || slot<1 || slot>2 || expected.empty()) {
		Trace::Error("RGNANO_SIM expect_phrase_param invalid args phrase=%d row=%d slot=%d expected=%s",phrase,row,slot,expected.c_str());
		return false;
	}
	ushort actual=(slot==1)
		? *(viewData->song_->phrase_->param1_ + 16 * phrase + row)
		: *(viewData->song_->phrase_->param2_ + 16 * phrase + row);
	char buffer[5];
	hexshort2char(actual,buffer);
	buffer[4]=0;
	bool matches=expected==buffer;
	Trace::Log("RGNANO_SIM","expect_phrase_param phrase=%02X row=%d slot=%d actual=%s expected=%s => %s",phrase,row,slot,buffer,expected.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimGroove(int channel, int groove)
{
	if (channel<0 || channel>=SONG_CHANNEL_COUNT || groove<0) {
		Trace::Error("RGNANO_SIM expect_groove invalid args channel=%d groove=%d",channel,groove);
		return false;
	}
	int actual=-1;
	int position=-1;
	Groove::GetInstance()->GetChannelData(channel,&actual,&position);
	bool matches=actual==groove;
	Trace::Log("RGNANO_SIM","expect_groove channel=%d actual=%d position=%d expected=%d => %s",channel,actual,position,groove,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimTableActive(int channel, int table)
{
	if (channel<0 || channel>=SONG_CHANNEL_COUNT || table<0 || table>=TABLE_COUNT) {
		Trace::Error("RGNANO_SIM expect_table_active invalid args channel=%d table=%d",channel,table);
		return false;
	}
	TablePlayback &playback=TablePlayback::GetTablePlayback(channel);
	Table *actual=playback.GetTable();
	Table *expected=&TableHolder::GetInstance()->GetTable(table);
	bool matches=actual==expected;
	Trace::Log("RGNANO_SIM","expect_table_active channel=%d active=%s expected=%02X => %s",channel,actual?"yes":"no",table,matches?"match":"mismatch");
	return matches;
}

static ViewData *GetSimViewData()
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	return appWindow ? appWindow->GetViewData() : 0;
}

bool SDLEventManager::ExpectSimTempo(int bpm)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_) {
		Trace::Error("RGNANO_SIM expect_tempo has no project");
		return false;
	}
	Variable *tempo=viewData->project_->FindVariable(VAR_TEMPO);
	if (!tempo) {
		Trace::Error("RGNANO_SIM expect_tempo missing tempo variable");
		return false;
	}
	int actual=tempo->GetInt();
	bool matches=(actual==bpm);
	Trace::Log("RGNANO_SIM","expect_tempo actual=%d expected=%d => %s",actual,bpm,matches?"match":"mismatch");
	return matches;
}

// The most gain reduction the master limiter applied over its meter
// history (the last few seconds), in tenths of a dB, within min..max
bool SDLEventManager::ExpectSimLimiterGr(int minTenths,int maxTenths)
{
	MasterLimiter *lim=MasterLimiter::GetInstance();
	float most=0.0f;
	for (int i=0;i<LIMITER_HISTORY;i++) {
		float gr=lim->GetHistoryGrDb(i);
		if (gr>most) most=gr;
	}
	int tenths=(int)(most*10.0f+0.5f);
	bool matches=(tenths>=minTenths && tenths<=maxTenths);
	Trace::Log("RGNANO_SIM","expect_limiter_gr enabled=%d max=%.1fdB now=%.1fdB in=%.1fdB out=%.1fdB range=%d..%d tenths => %s",
	           lim->Enabled()?1:0,most,lim->GetGainReductionDb(),lim->GetInputPeakDb(),lim->GetOutputPeakDb(),
	           minTenths,maxTenths,matches?"match":"mismatch");
	return matches;
}

static SampleInstrument *GetSimSampleInstrument(InstrumentBank *bank, int instrument)
{
	I_Instrument *instr=bank->GetInstrument(instrument);
	if (!instr || instr->GetType()!=IT_SAMPLE) {
		Trace::Error("RGNANO_SIM instrument %02X is not a sample instrument",instrument);
		return 0;
	}
	return (SampleInstrument *)instr;
}

bool SDLEventManager::ExpectSimInstrumentSample(int instrument, const std::string &sampleName)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT || sampleName.empty()) {
		Trace::Error("RGNANO_SIM expect_instrument_sample invalid args instrument=%d sample=%s",instrument,sampleName.c_str());
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	const char *actual=sampleInstrument->GetFileName();
	bool matches=(actual && sampleName==actual);
	Trace::Log("RGNANO_SIM","expect_instrument_sample inst=%d actual=%s expected=%s => %s",instrument,actual?actual:"(null)",sampleName.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimInstrumentRoot(int instrument, int note)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM expect_instrument_root invalid args instrument=%d note=%d",instrument,note);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	Variable *root=sampleInstrument->FindVariable(SIP_ROOTNOTE);
	int actual=root?root->GetInt():-1;
	bool matches=(actual==note);
	Trace::Log("RGNANO_SIM","expect_instrument_root inst=%d actual=%d expected=%d => %s",instrument,actual,note,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimInstrumentRootSuggestion(int instrument, int note)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM expect_instrument_root_suggestion invalid args instrument=%d note=%d",instrument,note);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	int actual=sampleInstrument->GetSuggestedRootNote();
	bool matches=(actual==note);
	Trace::Log("RGNANO_SIM","expect_instrument_root_suggestion inst=%d actual=%d expected=%d => %s",instrument,actual,note,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimSampleTrim(int instrument, int start, int loopStart, int end)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM expect_sample_trim invalid instrument=%d",instrument);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	Variable *startVar=sampleInstrument->FindVariable(SIP_START);
	Variable *loopVar=sampleInstrument->FindVariable(SIP_LOOPSTART);
	Variable *endVar=sampleInstrument->FindVariable(SIP_END);
	int actualStart=startVar?startVar->GetInt():-1;
	int actualLoop=loopVar?loopVar->GetInt():-1;
	int actualEnd=endVar?endVar->GetInt():-1;
	bool matches=(actualStart==start && actualLoop==loopStart && actualEnd==end);
	Trace::Log("RGNANO_SIM","expect_sample_trim inst=%d actual=%d/%d/%d expected=%d/%d/%d => %s",
	           instrument,actualStart,actualLoop,actualEnd,start,loopStart,end,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimSampleTrimOrder(int instrument)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM expect_sample_trim_order invalid instrument=%d",instrument);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	Variable *startVar=sampleInstrument->FindVariable(SIP_START);
	Variable *loopVar=sampleInstrument->FindVariable(SIP_LOOPSTART);
	Variable *endVar=sampleInstrument->FindVariable(SIP_END);
	int start=startVar?startVar->GetInt():-1;
	int loopStart=loopVar?loopVar->GetInt():-1;
	int end=endVar?endVar->GetInt():-1;
	bool matches=(start>=0 && start<=loopStart && loopStart<=end);
	Trace::Log("RGNANO_SIM","expect_sample_trim_order inst=%d values=%d/%d/%d => %s",
	           instrument,start,loopStart,end,matches?"valid":"invalid");
	return matches;
}

bool SDLEventManager::ExpectSimRenderMode(int mode)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || mode<0 || mode>2) {
		Trace::Error("RGNANO_SIM expect_render_mode invalid mode=%d",mode);
		return false;
	}
	int actual=viewData->project_->GetRenderMode();
	bool matches=actual==mode;
	Trace::Log("RGNANO_SIM","expect_render_mode actual=%d expected=%d => %s",actual,mode,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::SimSetTempo(int bpm)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_) {
		Trace::Error("RGNANO_SIM sim_set_tempo has no project");
		return false;
	}
	Variable *tempo=viewData->project_->FindVariable(VAR_TEMPO);
	if (!tempo) {
		Trace::Error("RGNANO_SIM sim_set_tempo missing tempo variable");
		return false;
	}
	tempo->SetInt(bpm);
	Trace::Log("RGNANO_SIM","sim_set_tempo %d",bpm);
	return true;
}

bool SDLEventManager::SimSetRenderMode(int mode)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || mode<0 || mode>2) {
		Trace::Error("RGNANO_SIM sim_set_render_mode invalid mode=%d",mode);
		return false;
	}
	Variable *renderMode=viewData->project_->FindVariable(VAR_RENDER);
	if (!renderMode) {
		Trace::Error("RGNANO_SIM sim_set_render_mode missing project variable");
		return false;
	}
	renderMode->SetInt(mode);
	viewData->renderMode_=mode;
	MixerService::GetInstance()->SetRenderMode(mode);
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_set_render_mode %d",mode);
	return true;
}

bool SDLEventManager::SimSetNoteNames(const std::string &mode)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_) {
		Trace::Error("RGNANO_SIM sim_set_note_names has no project");
		return false;
	}
	int value=-1;
	if (mode=="sharps" || mode=="Sharps" || mode=="sharp") {
		value=0;
	} else if (mode=="flats" || mode=="Flats" || mode=="flat") {
		value=1;
	}
	if (value<0) {
		Trace::Error("RGNANO_SIM sim_set_note_names invalid mode %s",mode.c_str());
		return false;
	}
	Variable *noteNames=viewData->project_->FindVariable(VAR_NOTE_NAMES);
	if (!noteNames) {
		Trace::Error("RGNANO_SIM sim_set_note_names missing project variable");
		return false;
	}
	noteNames->SetInt(value);
	viewData->project_->GetNoteNameMode();
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_set_note_names %s",mode.c_str());
	return true;
}

bool SDLEventManager::SimSetScale(int scale)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || scale<0 || scale>=scaleCount) {
		Trace::Error("RGNANO_SIM sim_set_scale invalid scale %d",scale);
		return false;
	}
	Variable *scaleVar=viewData->project_->FindVariable(VAR_SCALE);
	if (!scaleVar) {
		Trace::Error("RGNANO_SIM sim_set_scale missing project variable");
		return false;
	}
	scaleVar->SetInt(scale);
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_set_scale %d",scale);
	return true;
}

bool SDLEventManager::SimSetScaleKey(int key)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || key<-1 || key>11) {
		Trace::Error("RGNANO_SIM sim_set_key invalid key %d",key);
		return false;
	}
	Variable *keyVar=viewData->project_->FindVariable(VAR_SCALE_KEY);
	if (!keyVar) {
		Trace::Error("RGNANO_SIM sim_set_key missing project variable");
		return false;
	}
	keyVar->SetInt(key);
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_set_key %d",key);
	return true;
}

bool SDLEventManager::SimImportSampleToInstrument(int instrument, const std::string &sampleName)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT || sampleName.empty()) {
		Trace::Error("RGNANO_SIM sim_import_sample_to_instrument invalid args instrument=%d sample=%s",instrument,sampleName.c_str());
		return false;
	}
	std::string aliasPath="./rgnano-sim-data/samples/";
	aliasPath+=sampleName;
	Path samplePath(aliasPath.c_str());
	Path resolvedPath(samplePath.GetPath());
	if (!resolvedPath.Exists()) {
		Trace::Error("RGNANO_SIM sim_import_sample_to_instrument missing %s (%s)",sampleName.c_str(),resolvedPath.GetPath().c_str());
		return false;
	}
	int sampleIndex=SamplePool::GetInstance()->ImportSample(samplePath);
	if (sampleIndex<0) {
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	// Importing a sample makes the slot a sample instrument (new projects start with synths)
	bank->SetInstrumentType(instrument,IT_SAMPLE);
	SampleInstrument *sampleInstrument=(SampleInstrument *)bank->GetInstrument(instrument);
	sampleInstrument->AssignSample(sampleIndex);
	int rootNote=sampleInstrument->DetectRootNoteSuggestion();
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_import_sample_to_instrument inst=%d sample=%s index=%d root=%d",instrument,sampleName.c_str(),sampleIndex,rootNote);
	return true;
}

bool SDLEventManager::SimSetSampleTrim(int instrument, int start, int end)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT || start<0 || end<=start) {
		Trace::Error("RGNANO_SIM sim_set_sample_trim invalid args instrument=%d start=%d end=%d",instrument,start,end);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	if (!sampleInstrument) return false;
	int sampleIndex=sampleInstrument->GetSampleIndex();
	SoundSource *source=SamplePool::GetInstance()->GetSource(sampleIndex);
	int sampleSize=source?source->GetSize(-1):0;
	if (sampleSize<=0 || start>=sampleSize) {
		Trace::Error("RGNANO_SIM sim_set_sample_trim sample bounds instrument=%d size=%d start=%d end=%d",instrument,sampleSize,start,end);
		return false;
	}
	if (end>sampleSize) end=sampleSize;
	Variable *startVar=sampleInstrument->FindVariable(SIP_START);
	Variable *loopStartVar=sampleInstrument->FindVariable(SIP_LOOPSTART);
	Variable *endVar=sampleInstrument->FindVariable(SIP_END);
	if (!startVar || !loopStartVar || !endVar) return false;
	startVar->SetInt(start);
	loopStartVar->SetInt(start);
	endVar->SetInt(end);
	sampleInstrument->ClearRootNoteSuggestion();
	Trace::Log("RGNANO_SIM","sim_set_sample_trim inst=%d start=%d end=%d",instrument,start,end);
	return true;
}

bool SDLEventManager::SimDetectTrimRoot(int instrument, int expected)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM sim_detect_trim_root invalid instrument=%d",instrument);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	SampleInstrument *sampleInstrument=GetSimSampleInstrument(bank,instrument);
	if (!sampleInstrument) return false;
	if (!sampleInstrument) return false;
	int actual=sampleInstrument->DetectRootNoteSuggestionFromTrim();
	bool matches=(actual==expected);
	Trace::Log("RGNANO_SIM","sim_detect_trim_root inst=%d actual=%d expected=%d => %s",instrument,actual,expected,matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::SimSetSongChain(int row, int channel, int chain)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->song_ || row<0 || row>=SONG_ROW_COUNT || channel<0 || channel>=SONG_CHANNEL_COUNT || chain<0 || chain>=CHAIN_COUNT) {
		Trace::Error("RGNANO_SIM sim_set_song_chain invalid args row=%d channel=%d chain=%d",row,channel,chain);
		return false;
	}
	*(viewData->song_->data_ + channel + SONG_CHANNEL_COUNT * row)=(unsigned char)chain;
	viewData->song_->chain_->SetUsed((unsigned char)chain);
	Trace::Log("RGNANO_SIM","sim_set_song_chain row=%d channel=%d chain=%02X",row,channel,chain);
	return true;
}

bool SDLEventManager::SimSetChainPhrase(int chain, int row, int phrase, int transpose)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->song_ || chain<0 || chain>=CHAIN_COUNT || row<0 || row>=16 || phrase<0 || phrase>=PHRASE_COUNT || transpose<0 || transpose>255) {
		Trace::Error("RGNANO_SIM sim_set_chain_phrase invalid args chain=%d row=%d phrase=%d transpose=%d",chain,row,phrase,transpose);
		return false;
	}
	*(viewData->song_->chain_->data_ + 16 * chain + row)=(unsigned char)phrase;
	*(viewData->song_->chain_->transpose_ + 16 * chain + row)=(unsigned char)transpose;
	viewData->song_->chain_->SetUsed((unsigned char)chain);
	viewData->song_->phrase_->SetUsed((unsigned char)phrase);
	Trace::Log("RGNANO_SIM","sim_set_chain_phrase chain=%02X row=%d phrase=%02X transpose=%02X",chain,row,phrase,transpose);
	return true;
}

bool SDLEventManager::SimSetPhraseNote(int phrase, int row, int note, int instrument)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->song_ || phrase<0 || phrase>=PHRASE_COUNT || row<0 || row>=16 || note<0 || note>119 || instrument<0 || instrument>=MAX_INSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM sim_set_phrase_note invalid args phrase=%d row=%d note=%d instrument=%d",phrase,row,note,instrument);
		return false;
	}
	*(viewData->song_->phrase_->note_ + 16 * phrase + row)=(unsigned char)note;
	*(viewData->song_->phrase_->instr_ + 16 * phrase + row)=(unsigned char)instrument;
	viewData->song_->phrase_->SetUsed((unsigned char)phrase);
	Trace::Log("RGNANO_SIM","sim_set_phrase_note phrase=%02X row=%d note=%d instrument=%02X",phrase,row,note,instrument);
	return true;
}

// Script parameter names use '_' for spaces: "env_amount" -> "env amount"
static Variable *FindSimInstrumentVariable(I_Instrument *instr, const std::string &name)
{
	if (!instr) return 0;
	std::string wanted=name;
	for (size_t i=0;i<wanted.size();i++) {
		if (wanted[i]=='_') wanted[i]=' ';
	}
	IteratorPtr<Variable> it(instr->GetIterator());
	for (it->Begin();!it->IsDone();it->Next()) {
		Variable &v=it->CurrentItem();
		if (wanted==v.GetName()) {
			return &v;
		}
	}
	return 0;
}

static I_Instrument *GetSimInstrument(ViewData *viewData, int instrument)
{
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_INSTRUMENT_COUNT) {
		return 0;
	}
	return viewData->project_->GetInstrumentBank()->GetInstrument(instrument);
}

bool SDLEventManager::SimSetSynth(int instrument, const std::string &preset)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM sim_set_synth invalid instrument=%d",instrument);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	if (!bank->SetInstrumentType(instrument,IT_SYNTH)) {
		return false;
	}
	SynthInstrument *synth=(SynthInstrument *)bank->GetInstrument(instrument);
	synth->LoadPreset(preset.c_str());
	bool matches=(preset==SynthInstrument::GetPresetName(synth->GetPreset()));
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_set_synth inst=%02X preset=%s => %s",instrument,preset.c_str(),matches?"ok":"unknown preset");
	return matches;
}

bool SDLEventManager::SimSetMacro(int instrument, const std::string &preset)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || instrument<0 || instrument>=MAX_SAMPLEINSTRUMENT_COUNT) {
		Trace::Error("RGNANO_SIM sim_set_macro invalid instrument=%d",instrument);
		return false;
	}
	InstrumentBank *bank=viewData->project_->GetInstrumentBank();
	if (!bank->SetInstrumentType(instrument,IT_MACRO)) {
		return false;
	}
	MacroInstrument *macro=(MacroInstrument *)bank->GetInstrument(instrument);
	macro->LoadPreset(preset.c_str());
	bool matches=(preset==MacroInstrument::GetPresetName(macro->GetPreset()));
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	if (appWindow) {
		appWindow->RefreshCurrentView();
	}
	Trace::Log("RGNANO_SIM","sim_set_macro inst=%02X preset=%s => %s",instrument,preset.c_str(),matches?"ok":"unknown preset");
	return matches;
}

bool SDLEventManager::SimSetInstrumentParam(int instrument, const std::string &name, const std::string &value)
{
	I_Instrument *instr=GetSimInstrument(GetSimViewData(),instrument);
	Variable *v=FindSimInstrumentVariable(instr,name);
	if (!v) {
		Trace::Error("RGNANO_SIM sim_set_instrument_param unknown inst=%d param=%s",instrument,name.c_str());
		return false;
	}
	if (v->GetType()==Variable::CHAR_LIST || v->GetType()==Variable::BOOL) {
		v->SetString(value.c_str());
	} else {
		v->SetInt((int)strtol(value.c_str(),0,0));
	}
	Trace::Log("RGNANO_SIM","sim_set_instrument_param inst=%02X %s=%s",instrument,v->GetName(),v->GetString());
	return true;
}

bool SDLEventManager::SimLoadInstrumentParam(int instrument, const std::string &name, const std::string &value)
{
	I_Instrument *instr=GetSimInstrument(GetSimViewData(),instrument);
	Variable *v=FindSimInstrumentVariable(instr,name);
	if (!v) {
		Trace::Error("RGNANO_SIM sim_load_instrument_param unknown inst=%d param=%s",instrument,name.c_str());
		return false;
	}
	// Exactly what a song load does with a saved PARAM
	InstrumentBank::RestoreParam(instr,v->GetName(),value.c_str());
	Trace::Log("RGNANO_SIM","sim_load_instrument_param inst=%02X %s saved=\"%s\" now=%s",instrument,v->GetName(),value.c_str(),v->GetString());
	return true;
}

// A sample in the song's pool: frame count ("-" to skip) and the peak level
// of channel 0..n between minPeak and maxPeak
bool SDLEventManager::ExpectSimSampleStats(const std::string &sampleName, const std::string &framesText, int minPeak, int maxPeak)
{
	SamplePool *pool=SamplePool::GetInstance();
	int index=-1;
	for (int i=0;i<pool->GetNameListSize();i++) {
		const char *name=pool->GetName(i);
		if (name && sampleName==name) {
			index=i;
			break;
		}
	}
	if (index<0 || !pool->GetSource(index)) {
		Trace::Log("RGNANO_SIM","expect_sample_stats %s => missing",sampleName.c_str());
		return false;
	}
	SoundSource *source=pool->GetSource(index);
	int frames=source->GetSize(-1);
	int channels=source->GetChannelCount(-1);
	const short *samples=(const short *)source->GetSampleBuffer(-1);
	int peak=0;
	for (int i=0;samples && i<frames*channels;i++) {
		int v=samples[i]<0?-samples[i]:samples[i];
		if (v>peak) peak=v;
	}
	bool framesOk=(framesText=="-") || (frames==(int)strtol(framesText.c_str(),0,0));
	bool peakOk=(peak>=minPeak && peak<=maxPeak);
	Trace::Log("RGNANO_SIM","expect_sample_stats %s frames=%d (want %s) channels=%d peak=%d (want %d..%d) => %s",
	           sampleName.c_str(),frames,framesText.c_str(),channels,peak,minPeak,maxPeak,(framesOk&&peakOk)?"match":"mismatch");
	return framesOk && peakOk;
}

bool SDLEventManager::ExpectSimInstrumentType(int instrument, const std::string &type)
{
	I_Instrument *instr=GetSimInstrument(GetSimViewData(),instrument);
	if (!instr) return false;
	const char *actual="Sample";
	if (instr->GetType()==IT_MIDI) actual="Midi";
	if (instr->GetType()==IT_SYNTH) actual="Synth";
	if (instr->GetType()==IT_MACRO) actual="Macro";
	bool matches=(type==actual);
	Trace::Log("RGNANO_SIM","expect_instrument_type inst=%02X actual=%s expected=%s => %s",instrument,actual,type.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimInstrumentName(int instrument, const std::string &name)
{
	I_Instrument *instr=GetSimInstrument(GetSimViewData(),instrument);
	if (!instr) return false;
	const char *actual=instr->GetName();
	bool matches=(actual && name==actual);
	Trace::Log("RGNANO_SIM","expect_instrument_name inst=%02X actual=%s expected=%s => %s",instrument,actual?actual:"(null)",name.c_str(),matches?"match":"mismatch");
	return matches;
}

bool SDLEventManager::ExpectSimInstrumentParam(int instrument, const std::string &name, const std::string &value)
{
	I_Instrument *instr=GetSimInstrument(GetSimViewData(),instrument);
	Variable *v=FindSimInstrumentVariable(instr,name);
	if (!v) {
		Trace::Error("RGNANO_SIM expect_instrument_param unknown inst=%d param=%s",instrument,name.c_str());
		return false;
	}
	bool matches=false;
	if (v->GetType()==Variable::CHAR_LIST || v->GetType()==Variable::BOOL) {
		// List entries with spaces are written with '_' (fm_init)
		std::string wanted=value;
		if (v->GetType()==Variable::CHAR_LIST) std::replace(wanted.begin(),wanted.end(),'_',' ');
		matches=(wanted==v->GetString());
	} else {
		matches=(v->GetInt()==(int)strtol(value.c_str(),0,0));
	}
	Trace::Log("RGNANO_SIM","expect_instrument_param inst=%02X %s actual=%s expected=%s => %s",instrument,v->GetName(),v->GetString(),value.c_str(),matches?"match":"mismatch");
	return matches;
}

// Song tools and sequencer command checks (bookmarks, track reorder,
// custom scale, ROLL/NTH/SEED...): one place for the small assertions
static bool simYes(const std::string &text)
{
	return text=="yes" || text=="1" || text=="true";
}

bool SDLEventManager::RunSimSequencerCheck(const std::string &op, const std::string &arg,
                                           const std::string &arg2, int value, int value2)
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_ || !viewData->song_) {
		Trace::Error("RGNANO_SIM %s: no song loaded",op.c_str());
		return false;
	}
	Player *player=Player::GetInstance();
	if (op=="expect_player") {
		int actual=player->GetSimValue(arg,value);
		bool atLeast=arg2.compare(0,2,">=")==0;
		int wanted=(int)strtol(arg2.c_str()+(atLeast?2:0),0,0);
		bool ok=atLeast?(actual>=wanted):(actual==wanted);
		Trace::Log("RGNANO_SIM","expect_player %s ch=%d actual=%d expected=%s => %s",arg.c_str(),value,actual,arg2.c_str(),ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_notes_repeat") {
		bool repeats=player->SimNotesRepeat(value,value2);
		bool ok=(repeats==simYes(arg));
		Trace::Log("RGNANO_SIM","expect_notes_repeat ch=%d period=%d repeats=%s expected=%s => %s",value,value2,repeats?"yes":"no",arg.c_str(),ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_project_param" || op=="sim_set_project_param") {
		Variable *v=viewData->project_->FindVariable(arg.c_str());
		if (!v) {
			Trace::Error("RGNANO_SIM %s unknown project param %s",op.c_str(),arg.c_str());
			return false;
		}
		bool listLike=(v->GetType()==Variable::CHAR_LIST || v->GetType()==Variable::BOOL);
		if (op=="sim_set_project_param") {
			if (listLike) v->SetString(arg2.c_str());
			else v->SetInt((int)strtol(arg2.c_str(),0,0));
			AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
			if (appWindow) appWindow->RefreshCurrentView();
			Trace::Log("RGNANO_SIM","sim_set_project_param %s=%s",arg.c_str(),v->GetString());
			return true;
		}
		// A list value can be given by name or by number (scale 21)
		bool numeric=!arg2.empty() && (isdigit((unsigned char)arg2[0]) || arg2[0]=='-');
		bool ok=(listLike && !numeric)?(arg2==v->GetString()):(v->GetInt()==(int)strtol(arg2.c_str(),0,0));
		Trace::Log("RGNANO_SIM","expect_project_param %s actual=%s expected=%s => %s",arg.c_str(),v->GetString(),arg2.c_str(),ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_bookmark") {
		bool marked=viewData->song_->IsBookmarked(value);
		bool ok=(marked==simYes(arg));
		Trace::Log("RGNANO_SIM","expect_bookmark row=%02X marked=%s expected=%s => %s",value,marked?"yes":"no",arg.c_str(),ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_track_muted") {
		bool muted=player->IsChannelMuted(value);
		bool ok=(muted==simYes(arg));
		Trace::Log("RGNANO_SIM","expect_track_muted ch=%d muted=%s expected=%s => %s",value,muted?"yes":"no",arg.c_str(),ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_phrase_note") {
		if (value<0 || value>=PHRASE_COUNT || value2<0 || value2>=16) return false;
		int actual=viewData->song_->phrase_->note_[16*value+value2];
		int expected=(arg=="--")?0xFF:atoi(arg.c_str());
		bool ok=(actual==expected);
		Trace::Log("RGNANO_SIM","expect_phrase_note phrase=%02X row=%d actual=%d expected=%d => %s",value,value2,actual,expected,ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_phrase_in_scale") {
		if (value<0 || value>=PHRASE_COUNT) return false;
		int notes=0,outside=0;
		for (int row=0;row<16;row++) {
			unsigned char n=viewData->song_->phrase_->note_[16*value+row];
			if (n==0xFF) continue;
			notes++;
			if (!viewData->project_->IsNoteInScale(n)) {
				outside++;
				Trace::Log("RGNANO_SIM","expect_phrase_in_scale row %d note %d is outside the scale",row,n);
			}
		}
		bool ok=(outside==0 && notes>=value2);
		Trace::Log("RGNANO_SIM","expect_phrase_in_scale phrase=%02X notes=%d outside=%d min=%d => %s",value,notes,outside,value2,ok?"match":"mismatch");
		return ok;
	}
	if (op=="expect_mixer_level" || op=="sim_set_mixer_level") {
		Mixer *mixer=Mixer::GetInstance();
		int wanted=(int)strtol(arg.c_str(),0,16);
		if (op=="sim_set_mixer_level") {
			mixer->SetLevel(value,wanted);
			Trace::Log("RGNANO_SIM","sim_set_mixer_level %d=%02X",value,wanted);
			return true;
		}
		int actual=mixer->GetLevel(value);
		bool ok=(actual==wanted);
		Trace::Log("RGNANO_SIM","expect_mixer_level %d actual=%02X expected=%02X => %s",value,actual,wanted,ok?"match":"mismatch");
		return ok;
	}
	return false;
}

bool SDLEventManager::SimSetPhraseCommand(int phrase, int row, int slot, const std::string &commandName, const std::string &paramText)
{
	ViewData *viewData=GetSimViewData();
	FourCC command=I_CMD_NONE;
	ushort param=0;
	if (!viewData || !viewData->song_ || phrase<0 || phrase>=PHRASE_COUNT || row<0 || row>=16 || slot<1 || slot>2 ||
		!ParseSimCommandName(commandName,&command) || !ParseSimHexWord(paramText,&param)) {
		Trace::Error("RGNANO_SIM sim_set_phrase_command invalid args phrase=%d row=%d slot=%d command=%s param=%s",phrase,row,slot,commandName.c_str(),paramText.c_str());
		return false;
	}
	if (slot==1) {
		*(viewData->song_->phrase_->cmd1_ + 16 * phrase + row)=command;
		*(viewData->song_->phrase_->param1_ + 16 * phrase + row)=param;
	} else {
		*(viewData->song_->phrase_->cmd2_ + 16 * phrase + row)=command;
		*(viewData->song_->phrase_->param2_ + 16 * phrase + row)=param;
	}
	viewData->song_->phrase_->SetUsed((unsigned char)phrase);
	Trace::Log("RGNANO_SIM","sim_set_phrase_command phrase=%02X row=%d slot=%d command=%s param=%04X",phrase,row,slot,commandName.c_str(),param);
	return true;
}

bool SDLEventManager::SimSetTableCommand(int tableIndex, int row, int slot, const std::string &commandName, const std::string &paramText)
{
	FourCC command=I_CMD_NONE;
	ushort param=0;
	if (tableIndex<0 || tableIndex>=TABLE_COUNT || row<0 || row>=TABLE_STEPS || slot<1 || slot>3 ||
		!ParseSimCommandName(commandName,&command) || !ParseSimHexWord(paramText,&param)) {
		Trace::Error("RGNANO_SIM sim_set_table_command invalid args table=%d row=%d slot=%d command=%s param=%s",tableIndex,row,slot,commandName.c_str(),paramText.c_str());
		return false;
	}
	Table &table=TableHolder::GetInstance()->GetTable(tableIndex);
	if (slot==1) {
		table.cmd1_[row]=command;
		table.param1_[row]=param;
	} else if (slot==2) {
		table.cmd2_[row]=command;
		table.param2_[row]=param;
	} else {
		table.cmd3_[row]=command;
		table.param3_[row]=param;
	}
	TableHolder::GetInstance()->SetUsed(tableIndex);
	Trace::Log("RGNANO_SIM","sim_set_table_command table=%02X row=%d slot=%d command=%s param=%04X",tableIndex,row,slot,commandName.c_str(),param);
	return true;
}

bool SDLEventManager::SimSaveProject()
{
	ViewData *viewData=GetSimViewData();
	if (!viewData || !viewData->project_) {
		Trace::Error("RGNANO_SIM sim_save_project has no project");
		return false;
	}
	PersistencyService::GetInstance()->Save();
	Trace::Log("RGNANO_SIM","sim_save_project");
	return true;
}

static std::string simCommandName(FourCC command)
{
	char name[5];
	fourCC2char(command,name);
	name[4]=0;
	return name;
}

// Everything a song is made of, as text: project settings, the song grid,
// chains, phrases, instruments (every knob), tables, grooves and mixer
// levels. Two songs with the same dump play the same. The walkthrough test
// compares the song built by button presses with the demo it teaches.
std::string SDLEventManager::BuildSimSongDump()
{
	ViewData *viewData=GetSimViewData();
	std::ostringstream out;
	if (!viewData || !viewData->project_ || !viewData->song_) {
		return "";
	}
	static const char *typeNames[IT_LAST]={"Sample","Midi","Synth","Macro"};
	char line[256];
	Project *project=viewData->project_;
	IteratorPtr<Variable> pit(project->GetIterator());
	for (pit->Begin();!pit->IsDone();pit->Next()) {
		Variable &v=pit->CurrentItem();
		out << "project " << v.GetName() << "=" << v.GetString() << "\n";
	}
	Song *song=viewData->song_;
	for (int row=0;row<SONG_ROW_COUNT;row++) {
		unsigned char *cells=song->data_+row*SONG_CHANNEL_COUNT;
		bool used=false;
		for (int c=0;c<SONG_CHANNEL_COUNT;c++) used=used || cells[c]!=0xFF;
		if (!used) continue;
		sprintf(line,"song %02X",row);
		out << line;
		for (int c=0;c<SONG_CHANNEL_COUNT;c++) {
			if (cells[c]==0xFF) {
				out << " --";
			} else {
				sprintf(line," %02X",cells[c]);
				out << line;
			}
		}
		out << "\n";
	}
	for (int chain=0;chain<CHAIN_COUNT;chain++) {
		for (int row=0;row<16;row++) {
			unsigned char phrase=song->chain_->data_[chain*16+row];
			if (phrase==0xFF) continue;
			sprintf(line,"chain %02X %X phrase %02X transpose %02X\n",chain,row,phrase,
			        song->chain_->transpose_[chain*16+row]);
			out << line;
		}
	}
	Phrase *phrase=song->phrase_;
	for (int p=0;p<PHRASE_COUNT;p++) {
		for (int row=0;row<16;row++) {
			int k=p*16+row;
			FourCC c1=phrase->cmd1_[k];
			FourCC c2=phrase->cmd2_[k];
			if (phrase->note_[k]==0xFF && phrase->instr_[k]==0xFF && c1==I_CMD_NONE && c2==I_CMD_NONE) continue;
			// A value without its command does nothing: only a command's
			// own value counts
			sprintf(line,"phrase %02X %X note %02X instr %02X %s %04X %s %04X\n",p,row,
			        phrase->note_[k],phrase->instr_[k],
			        simCommandName(c1).c_str(),c1==I_CMD_NONE?0:phrase->param1_[k],
			        simCommandName(c2).c_str(),c2==I_CMD_NONE?0:phrase->param2_[k]);
			out << line;
		}
	}
	InstrumentBank *bank=project->GetInstrumentBank();
	for (int i=0;i<MAX_SAMPLEINSTRUMENT_COUNT;i++) {
		I_Instrument *instr=bank->GetInstrument(i);
		if (!instr || instr->IsEmpty()) continue;
		InstrumentType type=instr->GetType();
		sprintf(line,"instrument %02X type %s\n",i,(type>=0 && type<IT_LAST)?typeNames[type]:"?");
		out << line;
		IteratorPtr<Variable> it(instr->GetIterator());
		for (it->Begin();!it->IsDone();it->Next()) {
			Variable &v=it->CurrentItem();
			sprintf(line,"instrument %02X ",i);
			out << line << v.GetName() << "=" << v.GetString() << "\n";
		}
	}
	TableHolder *tables=TableHolder::GetInstance();
	for (int t=0;t<TABLE_COUNT;t++) {
		Table &table=tables->GetTable(t);
		for (int row=0;row<TABLE_STEPS;row++) {
			FourCC cmds[3]={table.cmd1_[row],table.cmd2_[row],table.cmd3_[row]};
			ushort params[3]={table.param1_[row],table.param2_[row],table.param3_[row]};
			if (cmds[0]==I_CMD_NONE && cmds[1]==I_CMD_NONE && cmds[2]==I_CMD_NONE) continue;
			sprintf(line,"table %02X %X",t,row);
			out << line;
			for (int c=0;c<3;c++) {
				sprintf(line," %s %04X",simCommandName(cmds[c]).c_str(),cmds[c]==I_CMD_NONE?0:params[c]);
				out << line;
			}
			out << "\n";
		}
	}
	Groove *groove=Groove::GetInstance();
	for (int g=0;g<MAX_GROOVES;g++) {
		unsigned char *data=groove->GetGrooveData(g);
		sprintf(line,"groove %02X",g);
		out << line;
		for (int s=0;s<16 && data[s]!=NO_GROOVE_DATA;s++) {
			sprintf(line," %02X",data[s]);
			out << line;
		}
		out << "\n";
	}
	Mixer *mixer=Mixer::GetInstance();
	for (int l=0;l<MIXER_LEVELS;l++) {
		sprintf(line,"mixer %X %02X\n",l,mixer->GetLevel(l));
		out << line;
	}
	return out.str();
}

bool SDLEventManager::SimDumpSong(const std::string &path)
{
	std::string dump=BuildSimSongDump();
	if (dump.empty()) {
		Trace::Error("RGNANO_SIM sim_dump_song has no song");
		return false;
	}
	std::ofstream file(path.c_str(),std::ios::binary);
	file << dump;
	file.close();
	bool ok=!file.fail();
	Trace::Log("RGNANO_SIM","sim_dump_song %s (%d bytes) => %s",path.c_str(),(int)dump.size(),ok?"ok":"write failed");
	return ok;
}

bool SDLEventManager::ExpectSimSongDump(const std::string &path)
{
	std::ifstream file(path.c_str(),std::ios::binary);
	if (!file.good()) {
		Trace::Error("RGNANO_SIM expect_song_dump %s => missing (sim_dump_song makes it)",path.c_str());
		return false;
	}
	std::vector<std::string> expected;
	std::string line;
	while (std::getline(file,line)) expected.push_back(line);
	std::vector<std::string> actual;
	std::istringstream dump(BuildSimSongDump());
	while (std::getline(dump,line)) actual.push_back(line);

	// Both dumps list things in the same order: walk them together and
	// report the lines only one side has
	int differences=0;
	size_t a=0,e=0;
	while (a<actual.size() || e<expected.size()) {
		if (a<actual.size() && e<expected.size() && actual[a]==expected[e]) {
			a++;
			e++;
			continue;
		}
		size_t later=expected.size();
		for (size_t k=e;a<actual.size() && k<expected.size();k++) {
			if (expected[k]==actual[a]) {
				later=k;
				break;
			}
		}
		if (later<expected.size()) {
			for (;e<later;e++) {
				if (++differences<=40) Trace::Log("RGNANO_SIM","expect_song_dump missing: %s",expected[e].c_str());
			}
			continue;
		}
		later=actual.size();
		for (size_t k=a;e<expected.size() && k<actual.size();k++) {
			if (actual[k]==expected[e]) {
				later=k;
				break;
			}
		}
		if (later<actual.size()) {
			for (;a<later;a++) {
				if (++differences<=40) Trace::Log("RGNANO_SIM","expect_song_dump extra: %s",actual[a].c_str());
			}
			continue;
		}
		if (e<expected.size()) {
			if (++differences<=40) Trace::Log("RGNANO_SIM","expect_song_dump wanted: %s",expected[e].c_str());
			e++;
		}
		if (a<actual.size()) {
			if (++differences<=40) Trace::Log("RGNANO_SIM","expect_song_dump got: %s",actual[a].c_str());
			a++;
		}
	}
	bool ok=(differences==0);
	Trace::Log("RGNANO_SIM","expect_song_dump %s lines=%d/%d differences=%d => %s",path.c_str(),
	           (int)actual.size(),(int)expected.size(),differences,ok?"match":"mismatch");
	return ok;
}

bool SDLEventManager::ExpectSimScreenSize(SDLGUIWindowImp *window, int width, int height)
{
	if (!window) {
		Trace::Error("RGNANO_SIM expect_size has no window");
		return false;
	}
	SDL_Surface *surface=window->GetSurface();
	if (!surface) {
		Trace::Error("RGNANO_SIM expect_size has no screen surface");
		return false;
	}
	int appSurfaceWidth=surface->w;
	int appSurfaceHeight=surface->h;
	if (window->IsRGNanoSkinEnabled()) {
		appSurfaceWidth=240*window->GetScale();
		appSurfaceHeight=240*window->GetScale();
	}
	Trace::Log("RGNANO_SIM","expect_size found %dx%d app surface in %dx%d window, expected %dx%d",appSurfaceWidth,appSurfaceHeight,surface->w,surface->h,width,height);
	return appSurfaceWidth==width && appSurfaceHeight==height;
}

bool SDLEventManager::ExpectSimScreenColors(SDLGUIWindowImp *window, int minColors)
{
	if (!window) {
		Trace::Error("RGNANO_SIM expect_colors has no window");
		return false;
	}
	if (minColors<=0) {
		minColors=2;
	}
	window->Flush();
	int colorCount=CountSurfaceColors(window->GetSurface(),minColors);
	Trace::Log("RGNANO_SIM","expect_colors found %d colors, expected at least %d",colorCount,minColors);
	if (colorCount<minColors) {
		Trace::Error("RGNANO_SIM screen color assertion failed");
		return false;
	}
	return true;
}

bool SDLEventManager::ExpectSimSkinFrameClean(SDLGUIWindowImp *window)
{
	if (!window) {
		Trace::Error("RGNANO_SIM expect_skin_frame_clean has no window");
		return false;
	}
	window->Flush();
	bool clean=window->IsRGNanoSkinFrameClean();
	Trace::Log("RGNANO_SIM","expect_skin_frame_clean => %s",clean?"clean":"contaminated");
	if (!clean) {
		Trace::Error("RGNANO_SIM skin frame was contaminated by app rendering");
	}
	return clean;
}

int SDLEventManager::CountSurfaceColors(SDL_Surface *surface, int maxColors)
{
	if (!surface || maxColors<=0) {
		return 0;
	}
	std::vector<Uint32> colors;
	if (SDL_MUSTLOCK(surface)) {
		if (SDL_LockSurface(surface)<0) {
			Trace::Error("RGNANO_SIM failed to lock screen surface");
			return 0;
		}
	}
	for (int y=0;y<surface->h;y++) {
		for (int x=0;x<surface->w;x++) {
			Uint32 pixel=ReadSurfacePixel(surface,x,y);
			bool seen=false;
			for (size_t i=0;i<colors.size();i++) {
				if (colors[i]==pixel) {
					seen=true;
					break;
				}
			}
			if (!seen) {
				colors.push_back(pixel);
				if ((int)colors.size()>=maxColors) {
					if (SDL_MUSTLOCK(surface)) {
						SDL_UnlockSurface(surface);
					}
					return (int)colors.size();
				}
			}
		}
	}
	if (SDL_MUSTLOCK(surface)) {
		SDL_UnlockSurface(surface);
	}
	return (int)colors.size();
}

Uint32 SDLEventManager::ReadSurfacePixel(SDL_Surface *surface, int x, int y)
{
	int bpp=surface->format->BytesPerPixel;
	Uint8 *p=(Uint8 *)surface->pixels + y*surface->pitch + x*bpp;
	switch (bpp) {
		case 1: return *p;
		case 2: return *(Uint16 *)p;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				return p[0]<<16 | p[1]<<8 | p[2];
			}
			return p[0] | p[1]<<8 | p[2]<<16;
		default: return *(Uint32 *)p;
	}
}

void SDLEventManager::LogSimState(const char *label, bool includeScreen)
{
	GUIWindow *guiWindow=Application::GetInstance()->GetWindow();
	AppWindow *appWindow=(AppWindow *)guiWindow;
	if (!appWindow) {
		Trace::Log("RGNANO_SIM_STATE","%s appWindow=(null)",label?label:"state");
		return;
	}
	std::string summary=appWindow->GetSimDebugSummary();
	Player *player=Player::GetInstance();
	if (player) {
		summary += " ";
		summary += player->GetSimDebugSummary();
	}
	if (showPowerMenu_) {
		summary += showExitConfirm_ ?
			(exitConfirmSelection_==0 ? " overlay=power-confirm selected=Yes" : " overlay=power-confirm selected=No") :
			(std::string(" overlay=power-menu selected=")+powerMenuSelectedLabel(powerMenuSelection_));
	}
	if (showDebugScreen_) {
		const char *debugItems[]={"Audio Input Monitor","System Info","Exit Debug"};
		summary += " overlay=debug selected=";
		summary += debugItems[debugScreenSelection_];
	}
	Trace::Log("RGNANO_SIM_STATE","%s %s",label?label:"state",summary.c_str());
	if (includeScreen) {
		std::string dump=appWindow->GetSimScreenDump();
		std::istringstream lines(dump);
		std::string line;
		while (std::getline(lines,line)) {
			Trace::Log("RGNANO_SIM_SCREEN","%s",line.c_str());
		}
	}
}
#endif



void SDLEventManager::PostQuitMessage()
{
  Trace::Log("EVENT","SDEM:PostQuitMessage()") ;
	finished_=true  ;
#ifdef PLATFORM_RGNANO
	system("pid erase >/dev/null 2>&1");
#endif
} ; 


int SDLEventManager::GetKeyCode(const char *key)
{
	if (!key) {
		return -1;
	}
	if (!strcmp(key,"power") || !strcmp(key,"POWER")) {
		return SDLK_POWER;
	}
	for (int i=0;i<SDLK_LAST;i++)
  {
		if (!strcmp(key,keyname_[i]))
    {
			return i ;
		}
	}
	return -1 ;
}

// Menu/Power overlay entries. Saving only makes sense with a song open.
enum PowerItem { PI_VOLUME = 0, PI_BRIGHTNESS, PI_THEME, PI_SONG_LIST, PI_QUIT, PI_DEBUG };
// What "Save your work?" leads to once answered
static int powerConfirmAction_=PI_QUIT;
#define POWER_MENU_MAX_ITEMS 7

// System volume / brightness, same as the RG Nano's own menu: FunKey OS
// "volume" and "brightness" commands, 0-100 in steps of 10.
static int systemVolume_=50;
static int systemBrightness_=70;

static int readSystemLevel(const char *tool,int fallback) {
#ifdef PLATFORM_RGNANO
	// "<tool> get" prints 0-100; read it back through the app's file layer
	char cmd[96];
	snprintf(cmd,sizeof(cmd),"%s get > /tmp/lgpt-level 2>/dev/null",tool);
	if (system(cmd)!=0) return fallback;
	I_File *f=FileSystem::GetInstance()->Open("/tmp/lgpt-level",(char *)"r");
	if (!f) return fallback;
	char text[16];
	int n=f->Read(text,1,sizeof(text)-1);
	f->Close();
	delete f;
	if (n<=0) return fallback;
	text[n]=0;
	int value=atoi(text);
	return (value>=0 && value<=100) ? value : fallback;
#else
	return fallback;
#endif
}

static void readSystemLevels() {
	systemVolume_=readSystemLevel("volume",systemVolume_);
	systemBrightness_=readSystemLevel("brightness",systemBrightness_);
}

static void setSystemLevel(const char *tool,int value) {
#ifdef PLATFORM_RGNANO
	// Backgrounded: "set" also stores the value in the bootloader env
	char cmd[64];
	snprintf(cmd,sizeof(cmd),"%s set %d >/dev/null 2>&1 &",tool,value);
	system(cmd);
#endif
	Trace::Log("EVENT","Power menu: %s %d",tool,value);
}

static void adjustSystemLevel(int item,int delta) {
	if (item==PI_THEME) {
		AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
		int count=AppWindow::ThemeCount();
		int theme=(AppWindow::CurrentTheme()+(delta>0?1:count-1))%count;
		if (appWindow) appWindow->SelectTheme(theme);
		Trace::Log("EVENT","Power menu: colors %s",AppWindow::ThemeName(theme));
		return;
	}
	if (item==PI_VOLUME) {
		systemVolume_+=delta;
		if (systemVolume_<0) systemVolume_=0;
		if (systemVolume_>100) systemVolume_=100;
		setSystemLevel("volume",systemVolume_);
	} else if (item==PI_BRIGHTNESS) {
		systemBrightness_+=delta;
		// Never all the way dark: the menu must stay readable
		if (systemBrightness_<10) systemBrightness_=10;
		if (systemBrightness_>100) systemBrightness_=100;
		setSystemLevel("brightness",systemBrightness_);
	}
}

static bool powerMenuHasSong() {
	AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
	ViewData *viewData=appWindow ? appWindow->GetViewData() : 0;
	return viewData && viewData->project_;
}

static int powerMenuItems(int *items) {
	int count=0;
	items[count++]=PI_VOLUME;
	items[count++]=PI_BRIGHTNESS;
	items[count++]=PI_THEME;
	if (powerMenuHasSong()) {
		items[count++]=PI_SONG_LIST;
		items[count++]=PI_QUIT;
	} else {
		items[count++]=PI_QUIT;
	}
	items[count++]=PI_DEBUG;
	return count;
}

static const char *powerItemLabel(int item) {
	static char volume[32];
	static char brightness[32];
	switch (item) {
		case PI_VOLUME:
			snprintf(volume,sizeof(volume),"Volume     < %3d%% >",systemVolume_);
			return volume;
		case PI_BRIGHTNESS:
			snprintf(brightness,sizeof(brightness),"Brightness < %3d%% >",systemBrightness_);
			return brightness;
		case PI_THEME: {
			static char theme[32];
			snprintf(theme,sizeof(theme),"Colors     < %-5s>",AppWindow::ThemeName(AppWindow::CurrentTheme()));
			return theme;
		}
		case PI_SONG_LIST: return "Song List";
		case PI_QUIT: return "Quit";
		default: return "Debug tools";
	}
}

static const char *powerMenuSelectedLabel(int selection) {
	int items[POWER_MENU_MAX_ITEMS];
	int count=powerMenuItems(items);
	if (selection<0 || selection>=count) selection=0;
	return powerItemLabel(items[selection]);
}

static Uint32 themeRGB(SDL_Surface *screen, const GUIColor &c) {
	return SDL_MapRGB(screen->format, c._r & 0xFF, c._g & 0xFF, c._b & 0xFF);
}

void SDLEventManager::RenderPowerMenu(SDL_Surface *screen, SDLGUIWindowImp *window)
{
	if (!showPowerMenu_ || !screen) return;
	int scale = window ? window->GetScale() : 1;

	Uint32 bg = themeRGB(screen, AppWindow::ThemeColor(CD_BACKGROUND));
	Uint32 border = themeRGB(screen, AppWindow::ThemeColor(CD_BORDER));
	Uint32 idle = themeRGB(screen, AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 30));
	Uint32 hot = themeRGB(screen, AppWindow::ThemeColor(CD_CURSOR));
	Uint32 text = themeRGB(screen, AppWindow::ThemeColor(CD_NORMAL));
	Uint32 title = themeRGB(screen, AppWindow::ThemeColor(CD_HILITE1));

	SDL_Rect fullScreen = {0, 0, (Uint16)screen->w, (Uint16)screen->h};
	SDL_FillRect(screen, &fullScreen, bg);

	const char *heading;
	const char *labels[POWER_MENU_MAX_ITEMS];
	int count;
	int selected;
	if (showExitConfirm_) {
		heading = "Save your work?";
		labels[0] = "Yes";
		labels[1] = "No";
		count = 2;
		selected = exitConfirmSelection_;
	} else {
		heading = "MENU";
		int items[POWER_MENU_MAX_ITEMS];
		count = powerMenuItems(items);
		for (int i=0;i<count;i++) labels[i] = powerItemLabel(items[i]);
		selected = powerMenuSelection_;
	}

	const int itemHeight = 22;
	const int menuWidth = 200;
	const int menuHeight = 34 + count * (itemHeight + 4);
	SDL_Rect frame = {(Sint16)((screen->w - menuWidth) / 2 - 2), (Sint16)((screen->h - menuHeight) / 2 - 2),
		(Uint16)(menuWidth + 4), (Uint16)(menuHeight + 4)};
	SDL_FillRect(screen, &frame, border);
	SDL_Rect menuBox = {(Sint16)(frame.x + 2), (Sint16)(frame.y + 2), (Uint16)menuWidth, (Uint16)menuHeight};
	SDL_FillRect(screen, &menuBox, bg);
	DrawSimOverlayText(screen, heading, menuBox.x + 10, menuBox.y + 10, title, scale);

	for (int i=0;i<count;i++) {
		SDL_Rect item = {(Sint16)(menuBox.x + 8), (Sint16)(menuBox.y + 28 + i * (itemHeight + 4)),
			(Uint16)(menuWidth - 16), (Uint16)itemHeight};
		bool on = (i == selected);
		SDL_FillRect(screen, &item, on ? hot : idle);
		DrawSimOverlayText(screen, labels[i], item.x + 8, item.y + 7, on ? bg : text, scale);
	}
	RenderMenuHelp(screen,window,showExitConfirm_ ? "EXIT?" : "POWER",
		showExitConfirm_ ? "Save before leaving" : "Sound, light, quit",
		showExitConfirm_ ? "Up/Down choose" : "Up/Down  L/R adjust",
		showExitConfirm_ ? "A answer" : "A open choice",
		showExitConfirm_ ? "B = No, leave" : "B or Power close",
		"R+Select helper");
}

bool SDLEventManager::HandleMenuHelpInput(SDLKey key)
{
	bool combo = (key == selectKey_ && rShoulderKey_ >= 0 &&
				 rShoulderKey_ < SDLK_LAST && menuInputHeld_[rShoulderKey_]) ||
				(key == rShoulderKey_ && selectKey_ >= 0 &&
				 selectKey_ < SDLK_LAST && menuInputHeld_[selectKey_]);
	if (combo) {
		menuHelpOverlay_ = !menuHelpOverlay_;
		if (menuHelpOverlay_) menuHelpPage_ = 0;
		return true;
	}
	if (menuHelpOverlay_) {
		if (key == SDLK_u || key == SDLK_d) {
			menuHelpPage_ = menuHelpPage_ == 0 ? 1 : 0;
		}
		return true;
	}
	return false;
}

void SDLEventManager::RenderMenuHelp(SDL_Surface *screen, SDLGUIWindowImp *window,
								 const char *name, const char *field,
								 const char *cmd1, const char *cmd2,
								 const char *cmd3, const char *cmd4)
{
	if (!menuHelpOverlay_ || !screen) return;
	int scale = window ? window->GetScale() : 1;
	int boxW = 220;
	int boxH = 150;
	SDL_Rect box = { Sint16((screen->w - boxW) / 2), Sint16((screen->h - boxH) / 2),
					 Uint16(boxW), Uint16(boxH) };
	SDL_FillRect(screen,&box,SDL_MapRGB(screen->format,18,0,24));
	SDL_Rect borderTop = { box.x, box.y, box.w, 2 };
	SDL_Rect borderBottom = { box.x, Sint16(box.y + box.h - 2), box.w, 2 };
	SDL_Rect borderLeft = { box.x, box.y, 2, box.h };
	SDL_Rect borderRight = { Sint16(box.x + box.w - 2), box.y, 2, box.h };
	Uint32 border=SDL_MapRGB(screen->format,240,0,150);
	SDL_FillRect(screen,&borderTop,border);
	SDL_FillRect(screen,&borderBottom,border);
	SDL_FillRect(screen,&borderLeft,border);
	SDL_FillRect(screen,&borderRight,border);
	Uint32 pink=SDL_MapRGB(screen->format,255,80,220);
	Uint32 white=SDL_MapRGB(screen->format,245,235,255);
	DrawSimOverlayText(screen,name,box.x+12,box.y+12,pink,scale);
	if (menuHelpPage_ == 0) {
		DrawSimOverlayText(screen,"MAP",box.x+12,box.y+36,white,scale);
		DrawSimOverlayText(screen,field,box.x+12,box.y+58,pink,scale);
		DrawSimOverlayText(screen,"Global overlay",box.x+12,box.y+82,white,scale);
		DrawSimOverlayText(screen,"Tracker below paused",box.x+12,box.y+102,white,scale);
	} else {
		DrawSimOverlayText(screen,"COMMANDS",box.x+12,box.y+36,white,scale);
		DrawSimOverlayText(screen,cmd1,box.x+12,box.y+58,white,scale);
		DrawSimOverlayText(screen,cmd2,box.x+12,box.y+78,white,scale);
		DrawSimOverlayText(screen,cmd3,box.x+12,box.y+98,white,scale);
		DrawSimOverlayText(screen,cmd4,box.x+12,box.y+118,white,scale);
	}
	DrawSimOverlayText(screen,"Up/Dn page R+Sel close",box.x+12,box.y+134,pink,scale);
}

const char *SDLEventManager::GetReplayKeyName(SDLKey key)
{
	if (key == powerMenuKey_) return "p";
	if (key == selectKey_) return "k";
	if (key == rShoulderKey_) return "n";
	switch (key) {
		case SDLK_a: return "a";
		case SDLK_b: return "b";
		case SDLK_l: return "l";
		case SDLK_r: return "r";
		case SDLK_u: return "u";
		case SDLK_d: return "d";
		case SDLK_m: return "m";
		case SDLK_n: return "n";
		case SDLK_s: return "s";
		case SDLK_k: return "k";
		case SDLK_p: return "p";
		case SDLK_x: return "x";
		case SDLK_y: return "y";
		default:
			if (key >= 0 && key < SDLK_LAST && keyname_[key]) {
				return keyname_[key];
			}
			return "unknown";
	}
}

void SDLEventManager::LogReplayKey(SDLKey key, bool pressed)
{
	unsigned long now = SDL_GetTicks();
	if (replayLastEventMs_ != 0) {
		unsigned long delta = now - replayLastEventMs_;
		if (delta > 0) {
			Trace::Log("RGNANO_REPLAY", "wait %lu", delta);
		}
	}
	replayLastEventMs_ = now;
	Trace::Log("RGNANO_REPLAY", "%s %s", pressed ? "down" : "up", GetReplayKeyName(key));
}

void SDLEventManager::HandlePowerMenuInput(SDLKey key)
{
	if (showExitConfirm_) {
		// Handle exit confirmation dialog
		switch (key) {
			case SDLK_u:  // UP
				exitConfirmSelection_--;
				if (exitConfirmSelection_ < 0) exitConfirmSelection_ = 1;
				break;

			case SDLK_d:  // DOWN
				exitConfirmSelection_++;
				if (exitConfirmSelection_ > 1) exitConfirmSelection_ = 0;
				break;

			case SDLK_a:  // A button - confirm
			case SDLK_RETURN:
			case SDLK_b:  // B answers "No": one press to leave without saving
			{
				if (key == SDLK_b) exitConfirmSelection_ = 1;
				if (exitConfirmSelection_ == 0) {
					PersistencyService::GetInstance()->Save();
					Trace::Log("EVENT","Power menu: saved song");
				}
				showPowerMenu_ = false;
				showExitConfirm_ = false;
				menuHelpOverlay_ = false;
				if (powerConfirmAction_ == PI_SONG_LIST) {
					AppWindow *appWindow=(AppWindow *)Application::GetInstance()->GetWindow();
					if (appWindow) appWindow->ReturnToSongList();
				} else {
					PostQuitMessage();
				}
				break;
			}

			case SDLK_ESCAPE:
				// Back to main menu
				showExitConfirm_ = false;
				exitConfirmSelection_ = 0;
				break;

			default:
				break;
		}
	} else {
		if (key == powerMenuKey_) {
			showPowerMenu_ = false;
			showExitConfirm_ = false;
			menuHelpOverlay_ = false;
			exitConfirmSelection_ = 0;
			return;
		}

		// Handle main power menu
		switch (key) {
			case SDLK_u:  // UP
			case SDLK_d: { // DOWN
				int items[POWER_MENU_MAX_ITEMS];
				int count = powerMenuItems(items);
				powerMenuSelection_ = (powerMenuSelection_ + (key == SDLK_u ? count - 1 : 1)) % count;
				break;
			}

			case SDLK_l:  // LEFT / RIGHT change volume and brightness
			case SDLK_r: {
				int items[POWER_MENU_MAX_ITEMS];
				int count = powerMenuItems(items);
				int item = items[powerMenuSelection_ < count ? powerMenuSelection_ : 0];
				adjustSystemLevel(item, key == SDLK_l ? -10 : 10);
				break;
			}

			case SDLK_a:  // A button - confirm
			case SDLK_RETURN: {
				int items[POWER_MENU_MAX_ITEMS];
				int count = powerMenuItems(items);
				int item = items[powerMenuSelection_ < count ? powerMenuSelection_ : 0];
				if (item == PI_VOLUME || item == PI_BRIGHTNESS || item == PI_THEME) {
					// A steps up, Left/Right go either way
					adjustSystemLevel(item, 10);
				} else if ((item == PI_QUIT || item == PI_SONG_LIST) && powerMenuHasSong()) {
					// Ask first; Yes (save) is the default answer
					powerConfirmAction_ = item;
					showExitConfirm_ = true;
					exitConfirmSelection_ = 0;
				} else if (item == PI_QUIT) {
					showPowerMenu_ = false;
					menuHelpOverlay_ = false;
					PostQuitMessage();
				} else {
					// Debug screen - open it
					showPowerMenu_ = false;
					showDebugScreen_ = true;
					menuHelpOverlay_ = false;
					debugScreenSelection_ = 0;
				}
				break;
			}

			case SDLK_b:  // B button - cancel
			case SDLK_ESCAPE:
				showPowerMenu_ = false;
				menuHelpOverlay_ = false;
				break;

			default:
				break;
		}
	}
}

void SDLEventManager::HandleDebugScreenInput(SDLKey key)
{
	switch (key) {
		case SDLK_u:  // UP
			debugScreenSelection_--;
			if (debugScreenSelection_ < 0) debugScreenSelection_ = 2;
			break;

		case SDLK_d:  // DOWN
			debugScreenSelection_++;
			if (debugScreenSelection_ > 2) debugScreenSelection_ = 0;
			break;

		case SDLK_a:  // A button - select
		case SDLK_RETURN:
			if (debugScreenSelection_ == 0) {
				// Audio Input Monitor - TODO: implement
				Trace::Log("DEBUG", "Audio Input Monitor selected");
			} else if (debugScreenSelection_ == 1) {
				// System Info - TODO: implement
				Trace::Log("DEBUG", "System Info selected");
			} else if (debugScreenSelection_ == 2) {
				// Exit Debug
				showDebugScreen_ = false;
				menuHelpOverlay_ = false;
				debugScreenSelection_ = 0;
			}
			break;

			case SDLK_b:  // B button - back
		case SDLK_ESCAPE:
			showDebugScreen_ = false;
			menuHelpOverlay_ = false;
			debugScreenSelection_ = 0;
			break;

		default:
			break;
	}
}

void SDLEventManager::RenderDebugScreen(SDL_Surface *screen, SDLGUIWindowImp *window)
{
	if (!showDebugScreen_ || !screen) return;
	int scale = window ? window->GetScale() : 1;

	// Fill entire screen with dark background
	SDL_Rect fullScreen = {0, 0, screen->w, screen->h};
	SDL_FillRect(screen, &fullScreen, SDL_MapRGB(screen->format, 20, 20, 20));

	// Menu box
	int menuWidth = 200;
	int menuHeight = 140;
	SDL_Rect menuBox = {
		(screen->w - menuWidth) / 2,
		(screen->h - menuHeight) / 2,
		menuWidth,
		menuHeight
	};
	SDL_FillRect(screen, &menuBox, SDL_MapRGB(screen->format, 240, 240, 240));

	// Title bar
	SDL_Rect titleBar = {menuBox.x, menuBox.y, menuWidth, 30};
	SDL_FillRect(screen, &titleBar, SDL_MapRGB(screen->format, 60, 60, 60));
	DrawSimOverlayText(screen,"Debug",titleBar.x + 10,titleBar.y + 11,SDL_MapRGB(screen->format, 255, 255, 255),scale);

	// Menu items
	int itemHeight = 25;
	int itemY = menuBox.y + 40;

	const char* items[3] = {
		"Audio Input Monitor",
		"System Info",
		"Exit Debug"
	};

	for (int i = 0; i < 3; i++) {
		SDL_Rect itemRect = {menuBox.x + 10, itemY + i * (itemHeight + 5), menuWidth - 20, itemHeight};
		Uint32 itemColor = (debugScreenSelection_ == i) ?
			SDL_MapRGB(screen->format, 80, 120, 200) :
			SDL_MapRGB(screen->format, 200, 200, 200);
		SDL_FillRect(screen, &itemRect, itemColor);
		DrawSimOverlayText(screen,items[i],itemRect.x + 6,itemRect.y + 8,
			SDL_MapRGB(screen->format, debugScreenSelection_ == i ? 255 : 20, debugScreenSelection_ == i ? 255 : 20, debugScreenSelection_ == i ? 255 : 20),scale);
	}
	RenderMenuHelp(screen,window,"DEBUG","Diagnostics",
		"Up/Down choose",
		"A select",
		"B back",
		"R+Select helper");
}
