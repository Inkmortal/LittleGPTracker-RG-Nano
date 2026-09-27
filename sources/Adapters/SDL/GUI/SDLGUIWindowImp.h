#ifndef SDL_GUI_WINDOW_H_
#define SDL_GUI_WINDOW_H_

#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <SDL/SDL.h>

bool ProcessDEBEvent(SDL_Event &event) ;
void ProcessButtonChange(unsigned short,unsigned short) ;

#define MAX_OVERLAYS 250

struct SDLCreateWindowParams: public GUICreateWindowParams {
	SDLCreateWindowParams():cacheFonts_(true),framebuffer_(false) {} ;
	bool cacheFonts_ ;
	bool framebuffer_ ;
} ;

class SDLGUIWindowImp: public I_GUIWindowImp {

public:

	SDLGUIWindowImp(GUICreateWindowParams &p) ;
	virtual ~SDLGUIWindowImp() ;

public: // I_GUIWindowImp implementation

	virtual void SetColor(GUIColor &) ;
	virtual void DrawRect(GUIRect &) ;
	virtual void DrawChar(const char c,GUIPoint &pos,GUITextProperties &);
	virtual void DrawString(const char *string,GUIPoint &pos,GUITextProperties &,bool overlay=false);
	virtual GUIRect GetRect() ;
	virtual void Invalidate() ;
	virtual void Flush();
	virtual void Lock() ;
	virtual void Unlock() ;
	virtual void Clear(GUIColor &, bool overlay=false) ;
	virtual void ClearRect(GUIRect &) ;
	virtual void PushEvent(GUIEvent &event) ;

public: // Added functionality
	void ProcessExpose() ;
	void ProcessQuit() ;
	void ProcessUserEvent(SDL_Event &event) ;
	SDL_Surface* GetSurface() { return screen_; }
	// Only the letters' own pixels, in the current color, at a pixel
	// position: text over a colored panel (the guide's example boxes)
	void DrawTransparentString(const char *string,GUIPoint &pos) ;
	bool IsRGNanoSkinEnabled() const { return rgnanoSkin_; }
	bool IsRGNanoSkinFrameClean() ;
	int GetAppAnchorX() const { return appAnchorX_; }
	int GetAppAnchorY() const { return appAnchorY_; }
	int GetScale() const { return mult_; }
	void SetRGNanoButtonPressed(int key, bool pressed);
protected:
	void prepareFonts() ;
	void prepareFullFonts() ;
	void prepareBPP1Fonts() ;
	void prepareBitmaps() ;
  void transform(const GUIRect &srcRect,SDL_Rect *dstRect);
  void transform(const GUIPoint &srcPoint, int *x, int *y);
  bool isAppPixelVisible(int x, int y, int w, int h) const;
  bool clipToAppSurface(SDL_Rect *rect) const;
#ifdef PLATFORM_RGNANO_SIM
	void DrawRGNanoSkin() ;
	void DrawRGNanoControls() ;
	void DrawRGNanoButton(int x, int y, int w, int h, Uint32 color, Uint32 pressedColor, int key, const char *label) ;
	void DrawRGNanoShoulder(int x, int y, bool rightSide, Uint32 color, Uint32 pressedColor, int key, const char *label) ;
	void DrawRGNanoLabel(int x, int y, const char *text, Uint32 color) ;
	void DrawRGNanoGlyph(int x, int y, char c, Uint32 color) ;
#endif
#ifdef _SHOW_GP2X_
	void drawGP2X() ;
	void drawGP2XOverlay() ;
	void drawSub(int which) ;
	SDL_Surface *load_image(int which,char *filename,int x,int y) ;
#endif
private:
	SDL_Surface *screen_;
//	SDL_Surface *offscreen_ ;
	GUIRect screenRect_ ;
	unsigned int currentColor_ ;
	unsigned int backgroundColor_ ;
	unsigned int foregroundColor_ ;
	int bitDepth_ ;
	bool cacheFonts_ ;
	bool framebuffer_ ;
  bool windowed_;
	SDL_Rect updateRects_[MAX_OVERLAYS] ;
	int updateCount_ ;
	int appAnchorX_ ;
	int appAnchorY_ ;
	int mult_ ;
	bool rgnanoSkin_ ;
#ifdef PLATFORM_RGNANO_SIM
	bool rgnanoButtonPressed_[SDLK_LAST] ;
#endif
} ;
#endif
