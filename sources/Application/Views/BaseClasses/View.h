
#ifndef _VIEW_H_
#define _VIEW_H_

#include "Application/Model/Config.h"
#include "Application/Model/Project.h"
#include "Application/Player/Player.h"
#include "Foundation/T_SimpleList.h"
#include "I_Action.h"
#include "UIFramework/Interfaces/I_GUIGraphics.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include "ViewEvent.h"
#ifdef SDL2
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif

enum GUIEventPadButtonMasks {
    EPBM_LEFT = 1,
    EPBM_DOWN = 2,
    EPBM_RIGHT = 4,
    EPBM_UP = 8,
    EPBM_L = 16,
    EPBM_B = 32,
    EPBM_A = 64,
    EPBM_R = 128,
    EPBM_START = 256,
    EPBM_SELECT = 512,
    EPBM_DOUBLE_A = 1024,
    EPBM_DOUBLE_B = 2048
};

enum ViewType {
    VT_SONG,
    VT_CHAIN,
    VT_PHRASE,
    VT_PROJECT,
    VT_INSTRUMENT,
    VT_TABLE,  // Table screen under phrase
    VT_TABLE2, // Table screen under instrument
    VT_GROOVE,
    VT_MIXER,
    VT_FX,     // send effects, under the mixer
    VT_EQ,     // master EQ, right of FX
    VT_LIMIT   // master limiter, right of EQ
};

enum ViewMode {
    VM_NORMAL,
    VM_NEW,
    VM_CLONE,
    VM_SELECTION,
    VM_MUTEON,
    VM_SOLOON
};

enum ColorDefinition {
    CD_BACKGROUND,
    CD_NORMAL,
    CD_BORDER,
    CD_HILITE1,
    CD_HILITE2,
    CD_CONSOLE,
    CD_CURSOR,
    CD_PLAY,
    CD_MUTE,
    CD_SONGVIEWFE,
    CD_SONGVIEW00,
    CD_ROW,
    CD_ROW2,
    CD_MAJORBEAT
};

enum ViewUpdateDirection { VUD_LEFT = 0, VUD_RIGHT, VUD_UP, VUD_DOWN };

class View;
class ModalView;

typedef void (*ModalViewCallback)(View &v, ModalView &d);

class View : public Observable {
  public:
    View(GUIWindow &w, ViewData *viewData);
    View(View &v);

    void SetFocus(ViewType vt) {
        viewType_ = vt;
        hasFocus_ = true;
        OnFocus();
    };

    void LooseFocus() { hasFocus_ = false; };

    void Clear();

    void ProcessButton(unsigned short mask, bool pressed);

    void Redraw();

    // Override in subclasses

    virtual void DrawView() = 0;
    // Pixel graphics drawn after the text layer is flushed, so they are never
    // painted over by character cells. Goes to the open modal if there is one.
    void DrawGraphics();
    // Guide page and section explaining this screen (A in the helper)
    virtual void GetGuideTopic(const char *&page, const char *&section);
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus() = 0;

    void SetDirty(bool dirty);

    // Primitive locking mechanism

    bool Lock();
    void WaitForObject();
    void Unlock();

    // Char based draw routines

    virtual void SetColor(ColorDefinition cd);
    virtual void ClearRect(int x, int y, int w, int h);
    virtual void DrawString(int x, int y, const char *txt,
                            GUITextProperties &props);

    void DoModal(ModalView *view, ModalViewCallback cb = 0);

    void EnableNotification();
    void SetNotification(const char *notification, int offset = 2);

  protected:
    // Override to draw pixel graphics on top of this view's text
    virtual void drawGraphics() {}
    virtual void ProcessButtonMask(unsigned short mask, bool pressed) = 0;

    // to remove once everything got to viewdata

    inline void updateData(unsigned char *c, int offset, unsigned char limit,
                           bool wrap) {
        int v = *c;
        if (v == 0xFF) { // Uninitiaized data
            v = 0;
        }
        v += offset;
        if (v < 0)
            v = (wrap ? (limit + 1 + v) : 0);
        if (v > limit)
            v = (wrap ? v - (limit + 1) : limit);
        *c = v;
    }

    GUIPoint GetAnchor();
    GUIPoint GetTitlePosition();

    void drawMap();
    void drawContextOverlay();
    void getHowToSteps(const char **lines);
    int getMoreKeys(const char **lines, int max);
    // Dialogs override: they are not the screen underneath
    virtual bool IsModal() { return false; }
    virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                         const char *&edit, const char *&field,
                                         const char *&cmd1, const char *&cmd2,
                                         const char *&cmd3, const char *&cmd4,
                                         const char *&cmd5, const char *&cmd6,
                                         const char *&cmd7);
    void drawPlaybackScope();
    void drawNotes();
    void drawMiniMeters();
    void drawMiniWaveform(bool force = false);
    // Pixel piano roll of one phrase (16 steps): note heads with a tail to
    // the next note or KILL, dots on the beats, optional playhead column
    // Render the current phrase (PM_PHRASE) or chain (PM_CHAIN) to a new
    // sample on a free instrument, then open it
    void renderToSample(int mode);
public:
    void ShowInstrument(int instrument);
protected:
    void drawPhraseRoll(int phrase, int x, int y, int w, int h, int playStep,
                        bool active, int cursorStep = -1);
    void drawOverlayLine(int x, int y, int width, const char *text,
                         GUITextProperties &props);
    void drawContextMap(int x, int y, int width, GUITextProperties &props);

  public: // temp hack for modl windo constructors
    GUIWindow &w_;
    ViewData *viewData_;

  protected:
    ViewMode viewMode_;
    bool isDirty_; // .Do we need to redraw screeen
    ViewType viewType_;
    bool hasFocus_;
    bool suppressPlaybackScope_;
    bool hasModal() { return modalView_ != 0; }

  private:
    unsigned short mask_;
    bool locked_;
    uint32_t notificationTime_;
    uint16_t NOTIFICATION_TIMEOUT;
    std::string displayNotification_;
    int notiDistY_;
    static bool initPrivate_;
    ModalView *modalView_;
    ModalViewCallback modalViewCallback_;

  public:
    static int margin_;
    static bool undoGesture_; // an A-hold of edits in progress (one undo step)
    static int songRowCount_;
    static bool miniLayout_;
    static bool ultraCompactLayout_;  // For RG Nano 240x240
    static int altRowNumber_;
    static int cursorAnimFrame_;  // For cursor pulse animation
    static bool contextOverlay_;
    static int contextOverlayPage_;
};

#endif
