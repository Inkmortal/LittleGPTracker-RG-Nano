
#ifndef _APP_WINDOW_H_
#define _APP_WINDOW_H_

#include "Application/Views/ChainView.h"
#include "Application/Views/ConsoleView.h"
#include "Application/Views/GrooveView.h"
#include "Application/Views/FXView.h"
#include "Application/Views/InstrumentView.h"
#include "Application/Views/MixerView.h"
#include "Application/Views/NullView.h"
#include "Application/Views/PhraseView.h"
#include "Application/Views/ProjectView.h"
#include "Application/Views/SongView.h"
#include "Application/Views/TableView.h"
#include "Application/Views/ViewData.h"
#include "Foundation/Observable.h"
#include "System/Process/SysMutex.h"
#include "System/io/Status.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include <string>

#define PROP_INVERT 0x80

class AppWindow : public GUIWindow, I_Observer, Status {
  protected:
    AppWindow(I_GUIWindowImp &imp);
    virtual ~AppWindow();

  public:
    // Theme colors for pixel graphics, so scopes and panels follow config.xml
    static GUIColor ThemeColor(ColorDefinition cd);
    // Colour themes, chosen in the Menu/Power menu and kept in
    // root:.lgpt-theme. config.xml colours still override a theme.
    static int ThemeCount();
    static const char *ThemeName(int theme);
    static int CurrentTheme();
    // Switch theme now: recolour, repaint and remember it
    void SelectTheme(int theme);
    static void SetThemeColors(GUIColor background, GUIColor normal,
                               GUIColor border, GUIColor hilite1,
                               GUIColor hilite2, GUIColor cursor,
                               GUIColor play, GUIColor mute, GUIColor row,
                               GUIColor row2, GUIColor majorbeat,
                               GUIColor songFE, GUIColor song00);
    // percent 0 = from, 100 = to
    static GUIColor ThemeBlend(ColorDefinition from, ColorDefinition to,
                               int percent);

    static AppWindow *Create(GUICreateWindowParams &);
    // Repaint every character cell on the next flush, erasing pixel graphics
    // (for example a closed dialog's key caps)
    void InvalidateScreenCache();
    // Paint now, outside the normal event path (overlays drawn by the event
    // manager). full = redraw the view and repaint every cell first.
    void RepaintNow(bool full);
    void LoadProject(const Path &path);
    void SaveLastProject(const Path &p);
    void CloseProject();
    // Close the song and show the song list (Menu/Power > Song List)
    void ReturnToSongList();

    virtual void Clear(bool all = false);
    void InvalidateCharCache();
    virtual void ClearRect(GUIRect &rect);
    virtual void SetColor(ColorDefinition cd);
    int GetVisibleColumns() const;
    int GetVisibleRows() const;
    void SetDirty();
    void SetCurrentViewDirty();
    void RefreshCurrentView();
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    const char *GetCurrentViewName() const;
    // Variable behind the focused field on Instrument/Project screens, or 0
    class Variable *GetSimFocusedVariable() const;
    std::string GetSimFocusedText() const;
    ViewData *GetViewData() const;
    bool ScreenContains(const char *needle) const;
    std::string GetSimDebugSummary() const;
    std::string GetSimScreenDump() const;
    std::string GetSimSelectionSummary() const;
    void LogDebugState(const char *label, bool includeScreen) const;
#endif

  protected: // GUIWindow implementation
    virtual bool onEvent(GUIEvent &event);
    virtual void onUpdate();
    virtual void LayoutChildren();
    virtual void Flush();
    virtual void Redraw();

    // override draw string to avoid going too far off
    // the screen.
    virtual void DrawString(const char *string, GUIPoint &pos,
                            GUITextProperties &props, bool overlay = false);

    // I_Observer implementation

    virtual void Update(Observable &o, I_ObservableData *d);

    // Status implementation

    virtual void Print(char *);

    void defineColor(const char *colorName, GUIColor &color);
    void defineAllColors();


    void onQuitApp();

  private:
    View *_currentView;
    ViewData *_viewData;
    SongView *_songView;
    ChainView *_chainView;
    PhraseView *_phraseView;
    ProjectView *_projectView;
    InstrumentView *_instrumentView;
    TableView *_tableView;
    GrooveView *_grooveView;
    FXView *_fxView;
    NullView *_nullView;
    MixerView *_mixerView;

    Path _root;

    bool _isDirty;
    bool _closeProject;
    bool _loadAfterSaveAsProject;
    bool _loadAfterResume;
    bool _shouldQuit;
    unsigned short _mask;
    unsigned long _lastA;
    unsigned long _lastB;
    char _statusLine[80];
    std::string _newProjectToLoad;
    unsigned char _charScreen[1200];
    unsigned char _charScreenProp[1200];
    unsigned char _preScreen[1200];
    unsigned char _preScreenProp[1200];

    static GUIColor backgroundColor_;
    static GUIColor normalColor_;
    static GUIColor borderColor_;
    static GUIColor songviewfeColor_;
    static GUIColor songview00Color_;
    static GUIColor highlight2Color_;
    static GUIColor highlightColor_;
    static GUIColor consoleColor_;
    static GUIColor cursorColor_;
    static GUIColor playColor_;
    static GUIColor muteColor_;
    static GUIColor rownumberColor_;
    static GUIColor rownumber2Color_;
    static GUIColor majorbeatColor_;
#ifdef PLATFORM_RGNANO
// bin: is the read-only OPK on the device; keep it next to the songs
#define LAST_PROJECT_NAME "root:.lgpt-last"
#else
#define LAST_PROJECT_NAME "bin:last_project"
#endif

    ColorDefinition colorIndex_;

    static int charWidth_;
    static int charHeight_;

    SysMutex drawMutex_;

    Path GetLastProjectPath();
};

#endif
