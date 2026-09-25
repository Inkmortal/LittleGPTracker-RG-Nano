#ifndef _SDL_EVENT_MANAGER_
#define _SDL_EVENT_MANAGER_

#include "Foundation/T_Singleton.h"
#include "UIFramework/SimpleBaseClasses/EventManager.h"
#include "Services/Controllers/ButtonControllerSource.h"
#include "Services/Controllers/JoystickControllerSource.h"
#include "Services/Controllers/HatControllerSource.h"
#include "Services/Controllers/KeyboardControllerSource.h"
#include <SDL/SDL.h>

#include <string>
#include <vector>

#define MAX_JOY_COUNT 4

class SDLGUIWindowImp;


class SDLEventManager: public T_Singleton<SDLEventManager>,public EventManager {
public:
	SDLEventManager() ;
	~SDLEventManager() ;
	virtual bool Init() ;
	virtual int MainLoop() ;
	virtual void PostQuitMessage() ;
	virtual int GetKeyCode(const char *name) ;
	void RenderPowerMenu(SDL_Surface *screen, SDLGUIWindowImp *window=0) ;
	void RenderDebugScreen(SDL_Surface *screen, SDLGUIWindowImp *window=0) ;

private:
	static bool finished_ ;
	static bool dumpEvent_ ;
	static bool showPowerMenu_ ;
	static int powerMenuSelection_ ;
	static bool showExitConfirm_ ;
	static int exitConfirmSelection_ ;
	static bool showDebugScreen_ ;
	static int debugScreenSelection_ ;
	static bool menuHelpOverlay_ ;
	static int menuHelpPage_ ;
	static bool menuInputHeld_[SDLK_LAST] ;
	static int powerMenuKey_ ;
	static int rShoulderKey_ ;
	static int selectKey_ ;
	static unsigned long replayLastEventMs_ ;
	const char *keyname_[SDLK_LAST] ;
	SDL_Joystick *joystick_[MAX_JOY_COUNT];
	ButtonControllerSource *buttonCS_[MAX_JOY_COUNT] ;
	JoystickControllerSource *joystickCS_[MAX_JOY_COUNT] ;
	HatControllerSource *hatCS_[MAX_JOY_COUNT] ;
	KeyboardControllerSource *keyboardCS_ ;

	void HandlePowerMenuInput(SDLKey key) ;
	// Redraw after a Menu/Power or debug overlay input. The device waits for
	// events instead of polling, so nothing else would repaint the screen.
	void RefreshOverlays(bool wasOpen) ;
#ifdef PLATFORM_RGNANO
	// Firmware shutdown (long power press): save the song, then power off
	void HandleShutdownRequest() ;
#endif
	void HandleDebugScreenInput(SDLKey key) ;
	bool HandleMenuHelpInput(SDLKey key) ;
	void LogReplayKey(SDLKey key, bool pressed) ;
	const char *GetReplayKeyName(SDLKey key) ;
	void RenderMenuHelp(SDL_Surface *screen, SDLGUIWindowImp *window,
						const char *name, const char *field,
						const char *cmd1, const char *cmd2,
						const char *cmd3, const char *cmd4) ;

#ifdef PLATFORM_RGNANO_SIM
	struct SimCommand {
		std::string op;
		std::string arg;
		std::string arg2;
		std::string arg3;
		int value;
		int value2;
	};
	std::vector<SimCommand> simCommands_;
	size_t simCommandIndex_;
	unsigned long simNextCommandTime_;
	int simPendingReleaseKey_;
	// Goal commands (goto/focus/set/page/instrument) press one combo per
	// tick until the live app state matches, instead of scripted key lists.
	std::vector<int> simGoalRelease_;
	int simGoalSteps_;
	std::string simGoalLastState_;
	int StepSimGoal(SDLGUIWindowImp *window, SimCommand &command);
	void PressSimCombo(SDLGUIWindowImp *window, int modifier, int key);
	int simMouseKey_;
	bool simScriptActive_;
	bool simScriptFailed_;
	void LoadSimScript();
	bool AddSimScriptLine(const std::string &line, const char *scriptPath, int lineNumber);
	bool AppendSimRoute(const std::string &routeName, const char *scriptPath, int lineNumber);
	void ProcessSimScript(SDLGUIWindowImp *window);
	bool HandleSimMouse(SDLGUIWindowImp *window, SDL_Event &event);
	void SetSimKey(SDLGUIWindowImp *window, int key, bool pressed);
	int GetSimButtonAt(int x, int y);
	void SaveSimScreenshot(SDLGUIWindowImp *window, const std::string &path);
	void SaveSimAppScreenshot(SDLGUIWindowImp *window, const std::string &path);
	void FailSimScript(const char *message);
	bool ExpectSimFile(const std::string &path);
	bool ExpectSimProjectFileBytes(const std::string &path, int minBytes);
	bool ExpectSimProjectSample(const std::string &sampleName);
	bool ExpectSimLog(const std::string &needle);
	bool ExpectSimNoError();
	bool ExpectSimScreensDiffer(const std::string &firstPath, const std::string &secondPath);
	bool ExpectSimView(const std::string &viewName);
	bool ExpectSimSelectedText(const std::string &needle);
	bool ExpectSimPlayerRunning(const std::string &expected);
	bool ExpectSimPlayMode(const std::string &modeName);
	bool ExpectSimStreamingSample(const std::string &needle);
	bool ExpectSimPlayingChannel(int channel, const std::string &instrument);
	bool ExpectSimChannelRow(int channel, const std::string &hexRow);
	bool ExpectSimAudioActivity(int minPeak);
	bool ExpectSimAudioSilence(int maxPeak);
	bool ExpectSimAudioCaptureBytes(int minBytes);
	bool ExpectSimScreenText(const std::string &needle);
	bool ExpectSimSongChain(int row, int channel, const std::string &expected);
	bool ExpectSimChainPhrase(int chain, int row, const std::string &expected);
	bool ExpectSimPhraseRowCount(int phrase, int minRows);
	bool ExpectSimPhraseCommand(int phrase, int row, int slot, const std::string &expected);
	bool ExpectSimPhraseParam(int phrase, int row, int slot, const std::string &expected);
	bool ExpectSimGroove(int channel, int groove);
	bool ExpectSimTableActive(int channel, int table);
	bool ExpectSimTempo(int bpm);
	bool ExpectSimLimiterGr(int minTenths,int maxTenths);
	bool ExpectSimAudioPeakMax(int maxPeak);
	bool ExpectSimInstrumentSample(int instrument, const std::string &sampleName);
	bool ExpectSimInstrumentRoot(int instrument, int note);
	bool ExpectSimInstrumentRootSuggestion(int instrument, int note);
	bool ExpectSimSampleTrim(int instrument, int start, int loopStart, int end);
	bool ExpectSimSampleTrimOrder(int instrument);
	bool ExpectSimRenderMode(int mode);
	bool SimSetTempo(int bpm);
	bool SimSetRenderMode(int mode);
	bool SimSetNoteNames(const std::string &mode);
	bool SimSetScale(int scale);
	bool SimSetScaleKey(int key);
	bool SimImportSampleToInstrument(int instrument, const std::string &sampleName);
	bool SimSetSampleTrim(int instrument, int start, int end);
	bool SimDetectTrimRoot(int instrument, int expected);
	bool SimSetSongChain(int row, int channel, int chain);
	bool SimSetChainPhrase(int chain, int row, int phrase, int transpose);
	bool SimSetPhraseNote(int phrase, int row, int note, int instrument);
	bool SimSetSynth(int instrument, const std::string &preset);
	bool SimSetInstrumentParam(int instrument, const std::string &name, const std::string &value);
	bool SimLoadInstrumentParam(int instrument, const std::string &name, const std::string &value);
	bool ExpectSimSampleStats(const std::string &sampleName, const std::string &framesText, int minPeak, int maxPeak);
	bool ExpectSimInstrumentType(int instrument, const std::string &type);
	bool ExpectSimInstrumentName(int instrument, const std::string &name);
	bool ExpectSimInstrumentParam(int instrument, const std::string &name, const std::string &value);
	bool SimSetPhraseCommand(int phrase, int row, int slot, const std::string &command, const std::string &param);
	bool SimSetTableCommand(int table, int row, int slot, const std::string &command, const std::string &param);
	bool SimSaveProject();
	bool ExpectSimScreenSize(SDLGUIWindowImp *window, int width, int height);
	bool ExpectSimScreenColors(SDLGUIWindowImp *window, int minColors);
	bool ExpectSimSkinFrameClean(SDLGUIWindowImp *window);
	int CountSurfaceColors(SDL_Surface *surface, int maxColors);
	Uint32 ReadSurfacePixel(SDL_Surface *surface, int x, int y);
	void LogSimState(const char *label, bool includeScreen);
#endif
} ;
#endif
