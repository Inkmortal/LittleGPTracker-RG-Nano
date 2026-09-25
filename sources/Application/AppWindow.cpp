#include "Application/Utils/UndoHistory.h"
#include "System/Console/CrashLog.h"
#include "AppWindow.h"
#include "Application/Utils/RecentSongs.h"
#include "Views/BaseClasses/FieldView.h"
#include "Views/BaseClasses/UIIntVarField.h"
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Commands/EventDispatcher.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Config.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Player/TablePlayback.h"
#include "Application/Utils/char.h"
#include "Application/Views/ModalDialogs/MessageBox.h"
#include "Application/Views/ModalDialogs/SelectProjectDialog.h"
#include "Foundation/Variables/WatchedVariable.h"
#include "Player/Player.h"
#include "Services/Midi/MidiService.h"
#include "System/Console/Trace.h"
#include "UIFramework/Interfaces/I_GUIWindowFactory.h"
#include "Views/UIController.h"
#include <sstream>
#include <string.h>

AppWindow *instance = 0;

// Theme colours, in ColorDefinition-like order
struct AppTheme {
    const char *name;
    unsigned char background[3], normal[3], border[3], hilite1[3],
        hilite2[3], cursor[3], play[3], mute[3], row[3], row2[3],
        majorbeat[3], songFE[3], song00[3];
};

static const AppTheme appThemes[] = {
    // Ink: warm off-white on near black, orange labels, yellow cursor
    {"Ink", {0x12, 0x12, 0x14}, {0xE6, 0xE4, 0xDF}, {0x5E, 0x5A, 0x56},
     {0xF0, 0xA0, 0x60}, {0xE8, 0xD8, 0xB8}, {0xFF, 0xD0, 0x40},
     {0x8C, 0xD8, 0x7C}, {0x80, 0x7C, 0x78}, {0x72, 0x6E, 0x6A},
     {0xA8, 0xA2, 0x9C}, {0xFF, 0xF4, 0xE0}, {0x72, 0x6E, 0x6A},
     {0x50, 0x4E, 0x4C}},
    // Slate: dark blue with teal accents
    {"Slate", {0x0E, 0x12, 0x18}, {0xD8, 0xDE, 0xE6}, {0x2F, 0x6F, 0x8F},
     {0x4F, 0xB8, 0xC8}, {0x5C, 0xC8, 0xA8}, {0xFF, 0xC1, 0x4D},
     {0x7C, 0xE0, 0x7C}, {0x7A, 0x84, 0x94}, {0x5A, 0x7A, 0x9A},
     {0x9A, 0xB8, 0xD8}, {0xF2, 0xE4, 0xC0}, {0x5C, 0x6A, 0x7A},
     {0x44, 0x50, 0x5E}},
    // Mono: greys only, white cursor
    {"Mono", {0x10, 0x10, 0x10}, {0xD8, 0xD8, 0xD8}, {0x60, 0x60, 0x60},
     {0xB0, 0xB0, 0xB0}, {0xC8, 0xC8, 0xC8}, {0xFF, 0xFF, 0xFF},
     {0xE0, 0xE0, 0xE0}, {0x78, 0x78, 0x78}, {0x68, 0x68, 0x68},
     {0x98, 0x98, 0x98}, {0xFF, 0xFF, 0xFF}, {0x68, 0x68, 0x68},
     {0x48, 0x48, 0x48}},
    // Amber: retro monochrome terminal
    {"Amber", {0x14, 0x0C, 0x04}, {0xFF, 0xB0, 0x40}, {0x80, 0x50, 0x18},
     {0xFF, 0xC8, 0x70}, {0xE0, 0x98, 0x38}, {0xFF, 0xE0, 0xA0},
     {0xFF, 0xD8, 0x80}, {0x90, 0x60, 0x28}, {0x90, 0x60, 0x28},
     {0xC0, 0x84, 0x38}, {0xFF, 0xD8, 0x90}, {0x90, 0x60, 0x28},
     {0x60, 0x40, 0x18}},
    // Pink: the original magenta look
    {"Pink", {0x1D, 0x0A, 0x1F}, {0xF5, 0xEB, 0xFF}, {0xFF, 0x00, 0x8C},
     {0xB7, 0x50, 0xD1}, {0xDB, 0x33, 0xDB}, {0xFF, 0x00, 0x8C},
     {0xFF, 0x00, 0x8C}, {0xF5, 0xEB, 0xFF}, {0xBA, 0x28, 0xF9},
     {0xFF, 0x00, 0xFF}, {0xBA, 0x28, 0xF9}, {0xA5, 0x5B, 0x8F},
     {0x85, 0x3B, 0x6F}},
};
static const int appThemeCount = sizeof(appThemes) / sizeof(appThemes[0]);
static int currentTheme = 0;
#define THEME_FILE "root:.lgpt-theme"

static GUIColor themeRGB(const unsigned char *c) { return GUIColor(c[0], c[1], c[2]); }

GUIColor AppWindow::backgroundColor_(0x12, 0x12, 0x14);
GUIColor AppWindow::normalColor_(0xE6, 0xE4, 0xDF);
GUIColor AppWindow::borderColor_(0x5E, 0x5A, 0x56);
GUIColor AppWindow::songviewfeColor_(0x72, 0x6E, 0x6A);
GUIColor AppWindow::songview00Color_(0x50, 0x4E, 0x4C);
GUIColor AppWindow::highlightColor_(0xF0, 0xA0, 0x60);
GUIColor AppWindow::highlight2Color_(0xE8, 0xD8, 0xB8);
GUIColor AppWindow::consoleColor_(0x00, 0xFF, 0x00);
GUIColor AppWindow::cursorColor_(0xFF, 0xD0, 0x40);
GUIColor AppWindow::playColor_(0x8C, 0xD8, 0x7C);
GUIColor AppWindow::muteColor_(0x80, 0x7C, 0x78);
GUIColor AppWindow::rownumberColor_(0x72, 0x6E, 0x6A);
GUIColor AppWindow::rownumber2Color_(0xA8, 0xA2, 0x9C);
GUIColor AppWindow::majorbeatColor_(0xFF, 0xF4, 0xE0);

int AppWindow::charWidth_ = 8;
int AppWindow::charHeight_ = 8;

// #define _FORCE_SDL_EVENT_

static void ProjectSelectCallback(View &v, ModalView &dialog) {

    SelectProjectDialog &spd = (SelectProjectDialog &)dialog;
    if (dialog.GetReturnCode() > 0) {
        Path selected = spd.GetSelection();
        instance->SaveLastProject(selected);
        instance->LoadProject(selected.GetPath().c_str());
    } else {
        System::GetInstance()->PostQuitMessage();
    }
};

int AppWindow::ThemeCount() { return appThemeCount; }

const char *AppWindow::ThemeName(int theme) {
    if (theme < 0 || theme >= appThemeCount)
        theme = 0;
    return appThemes[theme].name;
}

int AppWindow::CurrentTheme() { return currentTheme; }

static void applyTheme(int theme) {
    if (theme < 0 || theme >= appThemeCount)
        theme = 0;
    currentTheme = theme;
    const AppTheme &t = appThemes[theme];
    AppWindow::SetThemeColors(themeRGB(t.background), themeRGB(t.normal),
                              themeRGB(t.border), themeRGB(t.hilite1),
                              themeRGB(t.hilite2), themeRGB(t.cursor),
                              themeRGB(t.play), themeRGB(t.mute),
                              themeRGB(t.row), themeRGB(t.row2),
                              themeRGB(t.majorbeat), themeRGB(t.songFE),
                              themeRGB(t.song00));
}

void AppWindow::SetThemeColors(GUIColor background, GUIColor normal,
                               GUIColor border, GUIColor hilite1,
                               GUIColor hilite2, GUIColor cursor, GUIColor play,
                               GUIColor mute, GUIColor row, GUIColor row2,
                               GUIColor majorbeat, GUIColor songFE,
                               GUIColor song00) {
    backgroundColor_ = background;
    normalColor_ = normal;
    borderColor_ = border;
    highlightColor_ = hilite1;
    highlight2Color_ = hilite2;
    cursorColor_ = cursor;
    playColor_ = play;
    muteColor_ = mute;
    rownumberColor_ = row;
    rownumber2Color_ = row2;
    majorbeatColor_ = majorbeat;
    songviewfeColor_ = songFE;
    songview00Color_ = song00;
}

// Saved theme name, or -1
static int loadSavedTheme() {
    Path path(THEME_FILE);
    I_File *file = FileSystem::GetInstance()->Open(path.GetPath().c_str(), (char *)"r");
    if (!file)
        return -1;
    char name[32];
    int n = file->Read(name, 1, sizeof(name) - 1);
    file->Close();
    delete file;
    if (n <= 0)
        return -1;
    name[n] = 0;
    for (int i = 0; i < n; i++) {
        if (name[i] == '\n' || name[i] == '\r')
            name[i] = 0;
    }
    for (int i = 0; i < appThemeCount; i++) {
        if (!strcmp(name, appThemes[i].name))
            return i;
    }
    return -1;
}

void AppWindow::SelectTheme(int theme) {
    applyTheme(theme);
    defineAllColors();
    Path path(THEME_FILE);
    I_File *file = FileSystem::GetInstance()->Open(path.GetPath().c_str(), (char *)"w");
    if (file) {
        file->Printf("%s\n", ThemeName(currentTheme));
        file->Close();
        delete file;
    }
    GUIWindow::Clear(backgroundColor_);
    InvalidateScreenCache();
    SetDirty();
}

GUIColor AppWindow::ThemeColor(ColorDefinition cd) {
    switch (cd) {
    case CD_BACKGROUND:
        return backgroundColor_;
    case CD_BORDER:
        return borderColor_;
    case CD_HILITE1:
        return highlightColor_;
    case CD_HILITE2:
        return highlight2Color_;
    case CD_CONSOLE:
        return consoleColor_;
    case CD_CURSOR:
        return cursorColor_;
    case CD_PLAY:
        return playColor_;
    case CD_MUTE:
        return muteColor_;
    case CD_SONGVIEWFE:
        return songviewfeColor_;
    case CD_SONGVIEW00:
        return songview00Color_;
    case CD_ROW:
        return rownumberColor_;
    case CD_ROW2:
        return rownumber2Color_;
    case CD_MAJORBEAT:
        return majorbeatColor_;
    case CD_NORMAL:
    default:
        return normalColor_;
    }
}

GUIColor AppWindow::ThemeBlend(ColorDefinition from, ColorDefinition to,
                               int percent) {
    GUIColor a = ThemeColor(from);
    GUIColor b = ThemeColor(to);
    return GUIColor(a._r + (b._r - a._r) * percent / 100,
                    a._g + (b._g - a._g) * percent / 100,
                    a._b + (b._b - a._b) * percent / 100);
}

void AppWindow::defineColor(const char *colorName, GUIColor &color) {

    Config *config = Config::GetInstance();
    const char *value = config->GetValue(colorName);
    if (value) {
        unsigned char r;
        char2hex(value, &r);
        unsigned char g;
        char2hex(value + 2, &g);
        unsigned char b;
        char2hex(value + 4, &b);
        color = GUIColor(r, g, b);
    }
}

// config.xml colours override the theme
void AppWindow::defineAllColors() {
    defineColor("BACKGROUND", backgroundColor_);
    defineColor("FOREGROUND", normalColor_);
    defineColor("BORDER", borderColor_);
    defineColor("SONGVIEW_FE", songviewfeColor_);
    defineColor("SONGVIEW_00", songview00Color_);
    defineColor("HICOLOR1", highlightColor_);
    defineColor("HICOLOR2", highlight2Color_);
    defineColor("CURSORCOLOR", cursorColor_);
    defineColor("PLAYCOLOR", playColor_);
    defineColor("MUTECOLOR", muteColor_);
    defineColor("ROWCOLOR1", rownumberColor_);
    defineColor("ROWCOLOR2", rownumber2Color_);
    defineColor("MAJORBEAT", majorbeatColor_);
}

AppWindow::AppWindow(I_GUIWindowImp &imp) : GUIWindow(imp) {

    instance = this;

    // Init all members

    _statusLine[0] = 0;

    _currentView = 0;
    _viewData = 0;
    _songView = 0;
    _chainView = 0;
    _phraseView = 0;
    _projectView = 0;
    _instrumentView = 0;
    _tableView = 0;
    _nullView = 0;
    _mixerView = 0;
    _fxView = 0;
    _eqView = 0;
    _limiterView = 0;
    _scaleView = 0;
    _grooveView = 0;
    _closeProject = 0;
    _loadAfterSaveAsProject = 0;
    _loadAfterResume = 0;
    _lastA = 0;
    _lastB = 0;
    _mask = 0;
    colorIndex_ = CD_NORMAL;

    EventDispatcher *ed = EventDispatcher::GetInstance();
    ed->SetWindow(this);

    Status::Install(this);

    // Init midi services
    MidiService::GetInstance()->Init();

    int savedTheme = loadSavedTheme();
    applyTheme(savedTheme >= 0 ? savedTheme : 0);
    defineAllColors();

    GUIWindow::Clear(backgroundColor_);

    _nullView = new NullView((*this), 0);
    _currentView = _nullView;
    _nullView->SetDirty(true);

    Config *config = Config::GetInstance(); // Possible to disable autoloading
    const char *autoLoadEnabled = config->GetValue("AUTO_LOAD_LAST");
    bool shouldAutoLoad =
        (!autoLoadEnabled || // Default to yes if not in config
         strcmp(autoLoadEnabled, "YES") == 0);

    SelectProjectDialog *spd = new SelectProjectDialog(*_currentView);
    Path lastProjectPath = GetLastProjectPath();
    if (shouldAutoLoad && lastProjectPath.Exists()) {
        Trace::Log("AppWindow", "Auto-loading last project: %s",
                   lastProjectPath.GetPath().c_str());
        _newProjectToLoad = lastProjectPath.GetPath().c_str();
        _loadAfterResume = true;
        delete spd;
    } else { // Show project selection dialog
        _currentView->DoModal(spd, ProjectSelectCallback);
    }

    memset(_charScreen, ' ', 1200);
    memset(_preScreen, ' ', 1200);
    memset(_charScreenProp, 0, 1200);
    memset(_preScreenProp, 0, 1200);

    Redraw();
};

AppWindow::~AppWindow() { MidiService::GetInstance()->Close(); }

int AppWindow::GetVisibleColumns() const {
    GUIRect rect = const_cast<AppWindow *>(this)->GetRect();
    int columns = rect.Width() / charWidth_;
    if (columns < 0) {
        columns = 0;
    }
    if (columns > 40) {
        columns = 40;
    }
    return columns;
}

int AppWindow::GetVisibleRows() const {
    GUIRect rect = const_cast<AppWindow *>(this)->GetRect();
    int rows = rect.Height() / charHeight_;
    if (rows < 0) {
        rows = 0;
    }
    if (rows > 30) {
        rows = 30;
    }
    return rows;
}

void AppWindow::DrawString(const char *string, GUIPoint &pos,
                           GUITextProperties &props, bool force) {

    int visibleColumns = GetVisibleColumns();
    int visibleRows = GetVisibleRows();

    char buffer[41];
    if ((pos._y < 0) || (pos._y >= visibleRows) || (pos._x >= visibleColumns)) {
        return;
    }
    int len = strlen(string);
    int offset = (pos._x < 0) ? -pos._x : 0;
    len -= offset;
    int available = visibleColumns - ((pos._x < 0) ? 0 : pos._x);
    len = MIN(len, available);
    if (len <= 0) {
        return;
    }
    memcpy(buffer, string + offset, len);
    buffer[len] = 0;

    int index = ((pos._x < 0) ? 0 : pos._x) + 40 * pos._y;
    memcpy(_charScreen + index, buffer, len);
    unsigned char prop = colorIndex_ + (props.invert_ ? PROP_INVERT : 0);
    memset(_charScreenProp + index, prop, len);
    if (force) {
        memset(_preScreen + index, 0, len);
        memset(_preScreenProp + index, 0xFF, len);
    }
};

void AppWindow::RepaintNow(bool full) {
    if (full) {
        InvalidateScreenCache();
        Redraw();
    }
    Flush();
}

void AppWindow::InvalidateScreenCache() {
    memset(_preScreen, 0, 1200);
    memset(_preScreenProp, 0xFF, 1200);
}

void AppWindow::Clear(bool all) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    GUIWindow::Clear(backgroundColor_);
    all = true;
#endif
    memset(_charScreen, ' ', 1200);
    memset(_charScreenProp, 0, 1200);
    if (all) {
        memset(_preScreen, ' ', 1200);
        memset(_preScreenProp, 0, 1200);
    };
};

void AppWindow::InvalidateCharCache() {
    memset(_preScreen, 0, 1200);
    memset(_preScreenProp, 0xFF, 1200);
}

void AppWindow::ClearRect(GUIRect &r) {

    int x = r.Left();
    int y = r.Top();
    int w = r.Width();
    int h = r.Height();
    int visibleColumns = GetVisibleColumns();
    int visibleRows = GetVisibleRows();

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= visibleColumns || y >= visibleRows || w <= 0 || h <= 0) {
        return;
    }
    if (x + w > visibleColumns) {
        w = visibleColumns - x;
    }
    if (y + h > visibleRows) {
        h = visibleRows - y;
    }

    unsigned char *st = _charScreen + x + (40 * y);
    unsigned char *pr = _charScreenProp + x + (40 * y);
    unsigned char *pst = _preScreen + x + (40 * y);
    unsigned char *ppr = _preScreenProp + x + (40 * y);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            *st++ = ' ';
            *pr++ = 0;
            *pst++ = 0;
            *ppr++ = 0xFF;
        }
        st += (40 - w);
        pr += (40 - w);
        pst += (40 - w);
        ppr += (40 - w);
    }
};

//
// Redraws the screen and flush it.
//

void AppWindow::Redraw() {

    SysMutexLocker locker(drawMutex_);

    if (_currentView) {
        _currentView->Redraw();
        Invalidate();
    }
};

//
// Flush current screen to display
//

void AppWindow::Flush() {

    SysMutexLocker locker(drawMutex_);

    Lock();
    long flushStart = System::GetInstance()->GetClock();

    GUITextProperties props;
    GUIPoint pos;

    ColorDefinition color = (ColorDefinition)-1;
    pos._x = 0;
    pos._y = 0;

    int count = 0;

    unsigned char *current = _charScreen;
    unsigned char *previous = _preScreen;
    unsigned char *currentProp = _charScreenProp;
    unsigned char *previousProp = _preScreenProp;
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 40; x++) {
#ifndef _LGPT_NO_SCREEN_CACHE_
            if ((*current != *previous) || (*currentProp != *previousProp)) {
#endif
                props.invert_ = (*currentProp & PROP_INVERT) != 0;
                if (((*currentProp) & 0x7F) != color) {
                    color = (ColorDefinition)((*currentProp) & 0x7F);
                    GUIColor gcolor = ThemeColor(color);
                    GUIWindow::SetColor(gcolor);
                }
                GUIWindow::DrawChar(*current, pos, props);
                count++;
#ifndef _LGPT_NO_SCREEN_CACHE_
            }
#endif
            current++;
            previous++;
            currentProp++;
            previousProp++;
            pos._x += AppWindow::charWidth_;
        }
        pos._y += AppWindow::charHeight_;
        pos._x = 0;
    }
    long flushEnd = System::GetInstance()->GetClock();
    if (_currentView) {
        _currentView->DrawGraphics();
    }
    GUIWindow::Flush();
    Unlock();
    {
        Player *player = Player::GetInstance();
        char state[48];
        snprintf(state, sizeof(state), "view %s player %s", GetCurrentViewName(),
                 player && player->IsRunning() ? "on" : "off");
        CrashLog::Heartbeat(state);
    }
    memcpy(_preScreen, _charScreen, 1200);
    memcpy(_preScreenProp, _charScreenProp, 1200);
};

void AppWindow::LoadProject(const Path &p) {
    Trace::Log("LoadProject", "%s\n", p.GetPath().c_str());
    // The song list can show recently opened songs first
    RecentSongs::Touch(Path(p).GetName());
    UndoHistory::Clear(); // another song: the old steps don't apply
    CrashLog::Note("load song %s", Path(p).GetName().c_str());
    _root = p;

    _closeProject = false;
    _loadAfterSaveAsProject = false;

    PersistencyService *persist = PersistencyService::GetInstance();

    TablePlayback::Reset();

    Path::SetAlias("project", _root.GetPath().c_str());
    Path::SetAlias("samples", "project:samples");

    // Load the sample pool

    SamplePool *pool = SamplePool::GetInstance();

    pool->Load();

    Project *project = new Project();

    bool succeeded = persist->Load();
    if (!succeeded) {
        project->GetInstrumentBank()->AssignDefaults();
    };

    // Project

    WatchedVariable::Disable();

    project->GetInstrumentBank()->Init();

    WatchedVariable::Enable();

    ApplicationCommandDispatcher::GetInstance()->Init(project);

    // Create view data

    _viewData = new ViewData(project);

    // Create & observe the player
    Player *player = Player::GetInstance();
    bool playerOK = player->Init(project, _viewData);
    player->AddObserver(*this);

    // Create the controller
    UIController *controller = UIController::GetInstance();
    controller->Init(project, _viewData);

    // Create & observe all views
    _songView = new SongView((*this), _viewData, _root.GetName().c_str());
    _songView->AddObserver((*this));

    _chainView = new ChainView((*this), _viewData);
    _chainView->AddObserver((*this));

    _phraseView = new PhraseView((*this), _viewData);
    _phraseView->AddObserver((*this));

    _projectView = new ProjectView((*this), _viewData);
    _projectView->AddObserver((*this));

    _instrumentView = new InstrumentView((*this), _viewData);
    _instrumentView->AddObserver((*this));

    _tableView = new TableView((*this), _viewData);
    _tableView->AddObserver((*this));

    _grooveView = new GrooveView((*this), _viewData);
    _grooveView->AddObserver(*this);

    _mixerView = new MixerView((*this), _viewData);
    _mixerView->AddObserver(*this);

    _fxView = new FXView((*this), _viewData);
    _fxView->AddObserver(*this);

    _eqView = new EQView((*this), _viewData);
    _eqView->AddObserver(*this);

    _limiterView = new LimiterView((*this), _viewData);
    _limiterView->AddObserver(*this);
    _scaleView = new ScaleView((*this), _viewData);
    _scaleView->AddObserver(*this);

    _currentView = _songView;
    _currentView->OnFocus();

    if (!playerOK) {
        MessageBox *mb =
            new MessageBox(*_songView, "Failed to initialize audio", MBBF_OK);
        _songView->DoModal(mb);
    }

    Redraw();
}

void AppWindow::ReturnToSongList() {
    CloseProject();
    SetDirty();
    RepaintNow(true);
}

void AppWindow::CloseProject() {

    _closeProject = false;
    Player *player = Player::GetInstance();
    player->Stop();
    player->RemoveObserver(*this);

    player->Reset();

    SamplePool *pool = SamplePool::GetInstance();
    pool->Reset();

    TableHolder::GetInstance()->Reset();
    TablePlayback::Reset();

    ApplicationCommandDispatcher::GetInstance()->Close();

    SAFE_DELETE(_songView);
    SAFE_DELETE(_chainView);
    SAFE_DELETE(_phraseView);
    SAFE_DELETE(_projectView);
    SAFE_DELETE(_instrumentView);
    SAFE_DELETE(_tableView);

    UIController *controller = UIController::GetInstance();
    controller->Reset();

    SAFE_DELETE(_viewData);

    _currentView = _nullView;
    _nullView->SetDirty(true);

    SelectProjectDialog *spd = new SelectProjectDialog(*_currentView);
    _currentView->DoModal(spd, ProjectSelectCallback);
};

AppWindow *AppWindow::Create(GUICreateWindowParams &params) {
    I_GUIWindowImp &imp =
        I_GUIWindowFactory::GetInstance()->CreateWindowImp(params);
    AppWindow *w = new AppWindow(imp);
    return w;
};

void AppWindow::SetDirty() { _isDirty = true; };

void AppWindow::SetCurrentViewDirty() {
    if (_currentView) {
        _currentView->SetDirty(true);
    }
    SetDirty();
};

void AppWindow::RefreshCurrentView() {
    SetCurrentViewDirty();
    Redraw();
};

bool AppWindow::onEvent(GUIEvent &event) {

    // We need to tell the app to quit once we're out of the
    // mixer lock, otherwise the windows driver will never return

    _shouldQuit = false;

    _isDirty = false;

    unsigned short v = 1 << event.GetValue();

    MixerService *sm = MixerService::GetInstance();
    sm->Lock();

    switch (event.GetType()) {

    case ET_PADBUTTONDOWN:

        _mask |= v;
        if (_currentView)
            _currentView->ProcessButton(_mask, true);
        break;

    case ET_PADBUTTONUP:

        _mask &= (0xFFFF - v);
        if (_currentView)
            _currentView->ProcessButton(_mask, false);
        break;

    case ET_SYSQUIT:
        _shouldQuit = true;
        break;

        /*		case ET_KEYDOWN:
            if
           (event.GetValue()==EKT_ESCAPE&&!Player::GetInstance()->IsRunning()) {
                if (_currentView!=_listView) {
                    CloseProject() ;
                    _isDirty=true ;
                } else {
                    System::GetInstance()->PostQuitMessage() ;
                };
            } ;*/

    default:
        break;
    }
    sm->Unlock();

    if (_shouldQuit) {
        onQuitApp();
    }
    if (_closeProject) {
        CloseProject();
        _isDirty = true;
    }
    if (_loadAfterSaveAsProject) {
        CloseProject();
        _isDirty = true;
        LoadProject(_newProjectToLoad.c_str());
    }
#ifdef _SHOW_GP2X_
    Redraw();
#else
    if (_isDirty)
        Redraw();
#endif
    return false;
};

void AppWindow::onUpdate() {
    if (_loadAfterResume) {
        _loadAfterResume = false;
        _isDirty = true;
        LoadProject(_newProjectToLoad.c_str());
        return;
    }
    Flush();
};

void AppWindow::LayoutChildren() {};

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
const char *AppWindow::GetCurrentViewName() const {
    if (_currentView == _songView)
        return "song";
    if (_currentView == _chainView)
        return "chain";
    if (_currentView == _phraseView)
        return "phrase";
    if (_currentView == _projectView)
        return "project";
    if (_currentView == _instrumentView)
        return "instrument";
    if (_currentView == _tableView)
        return "table";
    if (_currentView == _grooveView)
        return "groove";
    if (_currentView == _mixerView)
        return "mixer";
    if (_currentView == _fxView)
        return "fx";
    if (_currentView == _eqView)
        return "eq";
    if (_currentView == _limiterView)
        return "limit";
    if (_currentView == _scaleView)
        return "scale";
    if (_currentView == _nullView)
        return "null";
    return "unknown";
}

ViewData *AppWindow::GetViewData() const { return _viewData; }

Variable *AppWindow::GetSimFocusedVariable() const {
    if (_currentView != _instrumentView && _currentView != _projectView &&
        _currentView != _fxView && _currentView != _eqView &&
        _currentView != _limiterView &&
        _currentView != _scaleView) {
        return 0;
    }
    UIField *focus = ((FieldView *)_currentView)->GetFocus();
    UIIntVarField *field = dynamic_cast<UIIntVarField *>(focus);
    return field ? &field->GetVariable() : 0;
}

// Text of the first highlighted (cursor) item on screen
std::string AppWindow::GetSimFocusedText() const {
    std::string summary = GetSimSelectionSummary();
    size_t a = summary.find('"');
    if (a == std::string::npos) {
        return "";
    }
    size_t b = summary.find('"', a + 1);
    return summary.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
}

bool AppWindow::ScreenContains(const char *needle) const {
    if (!needle || !needle[0]) {
        return false;
    }
    char row[41];
    for (int y = 0; y < 30; y++) {
        memcpy(row, _charScreen + y * 40, 40);
        row[40] = 0;
        if (strstr(row, needle)) {
            return true;
        }
    }
    return false;
}

std::string AppWindow::GetSimSelectionSummary() const {
    std::ostringstream out;
    bool any = false;
    for (int y = 0; y < 30; y++) {
        int x = 0;
        while (x < 40) {
            int index = x + 40 * y;
            if ((_charScreenProp[index] & PROP_INVERT) == 0) {
                x++;
                continue;
            }
            int start = x;
            std::string text;
            while (x < 40 && (_charScreenProp[x + 40 * y] & PROP_INVERT) != 0) {
                text += (char)_charScreen[x + 40 * y];
                x++;
            }
            while (!text.empty() && text[text.size() - 1] == ' ') {
                text.erase(text.size() - 1);
            }
            while (!text.empty() && text[0] == ' ') {
                text.erase(0, 1);
            }
            if (!text.empty()) {
                if (any) {
                    out << "; ";
                }
                out << "r" << y << "c" << start << "=\"" << text << "\"";
                any = true;
            }
        }
    }
    if (!any) {
        out << "(none)";
    }
    return out.str();
}

std::string AppWindow::GetSimScreenDump() const {
    std::ostringstream out;
    for (int y = 0; y < 30; y++) {
        char row[41];
        memcpy(row, _charScreen + y * 40, 40);
        row[40] = 0;
        int end = 39;
        while (end >= 0 && row[end] == ' ') {
            row[end] = 0;
            end--;
        }
        out << y;
        if (y < 10) {
            out << " ";
        }
        out << "| " << row << "\n";
    }
    out << "selected: " << GetSimSelectionSummary();
    return out.str();
}

std::string AppWindow::GetSimDebugSummary() const {
    std::ostringstream out;
    out << "view=" << GetCurrentViewName();
    if (_viewData) {
        int songAbsRow = _viewData->songOffset_ + _viewData->songY_;
        out << " song(row=" << songAbsRow
            << " screenY=" << _viewData->songY_
            << " ch=" << _viewData->songX_;
        if (_viewData->song_) {
            unsigned char songValue = *(_viewData->GetCurrentSongPointer());
            out << " cell=";
            if (songValue == 0xFF) {
                out << "--";
            } else {
                char buffer[8];
                sprintf(buffer, "%02X", songValue);
                out << buffer;
            }
        }
        out << ")";
        out << " chain(current=" << _viewData->currentChain_
            << " row=" << _viewData->chainRow_
            << " col=" << _viewData->chainCol_;
        if (_viewData->song_) {
            unsigned char chainValue = *(_viewData->GetCurrentChainPointer());
            out << " cell=";
            if (chainValue == 0xFF) {
                out << "--";
            } else {
                char buffer[8];
                sprintf(buffer, "%02X", chainValue);
                out << buffer;
            }
        }
        out << ")";
        out << " phrase(current=" << _viewData->currentPhrase_
            << " row=" << _viewData->phraseCurPos_ << ")";
        out << " instrument=" << _viewData->currentInstrument_;
        out << " table=" << _viewData->currentTable_;
        out << " groove=" << _viewData->currentGroove_;
        out << " mixer(row=" << _viewData->mixerRow_
            << " col=" << _viewData->mixerCol_ << ")";
        out << " playMode=" << _viewData->playMode_;
    } else {
        out << " project=(none)";
    }
    out << " selected=" << GetSimSelectionSummary();
    return out.str();
}

void AppWindow::LogDebugState(const char *label, bool includeScreen) const {
    std::string summary = GetSimDebugSummary();
    Player *player = Player::GetInstance();
    if (player) {
        summary += " ";
        summary += player->GetSimDebugSummary();
    }
    Trace::Log("RGNANO_STATE", "%s %s", label ? label : "state", summary.c_str());
    if (includeScreen) {
        std::string dump = GetSimScreenDump();
        std::istringstream lines(dump);
        std::string line;
        while (std::getline(lines, line)) {
            Trace::Log("RGNANO_SCREEN", "%s", line.c_str());
        }
    }
}
#endif

void AppWindow::Update(Observable &o, I_ObservableData *d) {

    ViewEvent *ve = (ViewEvent *)d;

    switch (ve->GetType()) {

    case VET_SWITCH_VIEW: {
        ViewType *vt = (ViewType *)ve->GetData();
        CrashLog::Note("screen %d", (int)*vt);
        if (_currentView) {
            _currentView->LooseFocus();
        }
        switch (*vt) {
        case VT_SONG:
            _currentView = _songView;
            break;
        case VT_CHAIN:
            _currentView = _chainView;
            break;
        case VT_PHRASE:
            _currentView = _phraseView;
            break;
        case VT_PROJECT:
            _currentView = _projectView;
            break;
        case VT_INSTRUMENT:
            _currentView = _instrumentView;
            break;
        case VT_TABLE:
            _currentView = _tableView;
            break;
        case VT_TABLE2:
            _currentView = _tableView;
            break;
        case VT_GROOVE:
            _currentView = _grooveView;
            break;
        case VT_MIXER:
            _currentView = _mixerView;
            break;
        case VT_FX:
            _currentView = _fxView;
            break;
        case VT_EQ:
            _currentView = _eqView;
            break;
        case VT_LIMIT:
            _currentView = _limiterView;
            break;
        case VT_SCALE:
            _currentView = _scaleView;
            break;
        }
        _currentView->SetFocus(*vt);
        _isDirty = true;
        GUIWindow::Clear(backgroundColor_, true);
        Clear(true);
        Redraw();
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
        {
            const char *dumpInput = Config::GetInstance()->GetValue("DUMPEVENT");
            if (dumpInput && !strcmp(dumpInput, "YES")) {
                LogDebugState("after-view-switch", true);
            }
        }
#endif
        break;
    }

    case VET_PLAYER_POSITION_UPDATE: {
        PlayerEvent *pt = (PlayerEvent *)ve;

        if (_currentView) {
            SysMutexLocker locker(drawMutex_);
            _currentView->OnPlayerUpdate(pt->GetType(), pt->GetTickCount());
            Invalidate();
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
            const char *dumpInput = Config::GetInstance()->GetValue("DUMPEVENT");
            if (dumpInput && !strcmp(dumpInput, "YES")) {
                Trace::Log("RGNANO_PLAYER", "event=%d tick=%u %s",
                           pt->GetType(), pt->GetTickCount(),
                           Player::GetInstance()->GetSimDebugSummary().c_str());
            }
#endif
        }

        break;
    }

        /*	  case VET_LIST_SELECT:
              {
                char *name=(char*)ve->GetData() ;
                LoadProject(name) ;
                break ;
              } */

    case VET_SAVEAS_PROJECT: {
        char *name = (char *)ve->GetData();
        _loadAfterSaveAsProject = true;
        _newProjectToLoad = name;
        break;
    }

    case VET_QUIT_PROJECT: {
        // defer event to after we got out of the view
        _closeProject = true;
        break;
    }
    case VET_QUIT_APP:
        _shouldQuit = true;
        break;
    }
}

void AppWindow::onQuitApp() {
    Player *player = Player::GetInstance();
    player->Stop();
    player->RemoveObserver(*this);

    player->Reset();
    System::GetInstance()->PostQuitMessage();
}
void AppWindow::Print(char *line) {

    //	GUIWindow::Clear(View::backgroundColor_,true) ;
    Clear();
    strcpy(_statusLine, line);

    // Calculate screen dimensions dynamically (30x30 chars for RG Nano, 40x30 for standard)
    GUIRect rect = GetRect();
    int screenWidth = rect.Width() / 8;
    int screenHeight = rect.Height() / 8;

    int position = screenWidth;
    position -= strlen(_statusLine);
    position /= 2;
    GUIPoint pos(position, 12);
    //
    GUITextProperties props;
    SetColor(CD_NORMAL);
    DrawString(_statusLine, pos, props);
    char buildString[80];
    sprintf(buildString, "Piggy build %s.%s.%s", PROJECT_NUMBER,
            PROJECT_RELEASE, BUILD_COUNT);
    pos._y = screenHeight - 2;  // Position 2 rows from bottom
    pos._x = (screenWidth - strlen(buildString)) / 2;
    DrawString(buildString, pos, props);
    Flush();
};

void AppWindow::SetColor(ColorDefinition cd) { colorIndex_ = cd; };

Path AppWindow::GetLastProjectPath() {
    Path lastProjectFile(LAST_PROJECT_NAME);
    FileSystem *fs = FileSystem::GetInstance();
    I_File *file = fs->Open(lastProjectFile.GetPath().c_str(), "r");

    if (!file) {
        return Path();
    }

    // Get file size
    file->Seek(0, SEEK_END);
    int length = file->Tell();
    
    if (length <= 0) {
        file->Close();
        delete file;
        return Path();
    }

    // Allocate buffer and seek back to start
    char *buffer = (char *)SYS_MALLOC(length + 1);
    memset(buffer, 0, length + 1);

    file->Seek(0, SEEK_SET); // Seek back to start
    int bytes = file->Read(buffer, 1, length); // Read full length
    file->Close();
    delete file;

    if (bytes <= 0) {
        Trace::Error("GetLastProject: Failed to read last project file");
        SYS_FREE(buffer);
        return Path();
    }

    buffer[bytes] = 0; // Null terminate

    // Remove newline if present
    int len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = 0;
    }

    Path result;
    if (strlen(buffer) > 0) {
        if (strstr(buffer, "lgpt_") != NULL) { // Ensure it's an lgpt project
            result = Path(buffer);
        } else {
            Trace::Error("GetLastProject: Invalid project path format: %s",
                         buffer);
        }
    }
    if (!result.IsDirectory()) {
        Trace::Error("GetLastProject: path does not exist: %s", result.GetPath().c_str());
    }

    SYS_FREE(buffer);
    return result;
}

void AppWindow::SaveLastProject(const Path &p) {
    Path lastProjectFile(LAST_PROJECT_NAME);
    FileSystem *fs = FileSystem::GetInstance();
    I_File *file = fs->Open(lastProjectFile.GetPath().c_str(), "w");

    if (!file) {
        Trace::Error("SaveLastProject: Failed to open %s for writing",
                     LAST_PROJECT_NAME);
        return;
    }

    std::string pathStr = p.GetPath();
    file->Write(pathStr.c_str(), 1, pathStr.length());
    file->Write("\n", 1, 1);
    file->Close();
    delete file;

    Trace::Log("SaveLastProject", "Saved last project: %s",
               p.GetPath().c_str());
}
