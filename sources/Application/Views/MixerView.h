#ifndef _MIXER_VIEW_H_
#define _MIXER_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"

class MixerView: public View {
public:
	MixerView(GUIWindow &w,ViewData *viewData) ;
	~MixerView() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int tick=0) ;
	virtual void OnFocus() ;
protected:
	void processNormalButtonMask(unsigned int mask) ;
    void processSelectionButtonMask(unsigned int mask) ;
	void onStart() ;
	void onStop() ;
	void updateCursor(int dx,int dy)  ;
    void drawChannelMeters(bool force=false) ;
    void drawChannelWaveform(int bus, int x, int y, int width, int height, bool selected) ;
    void drawWaveform(bool force=false) ;
    // Mixer strips: the 8 tracks, reverb and delay returns, master
    int stripLevel(int strip) ;
    void setStripLevel(int strip,int value) ;
    void drawStripInfo() ;
private:
    static const int WAVEFORM_DRAW_COLUMNS = 192 ;
	const char *song_ ;
    unsigned int lastWaveformDrawMs_ ;
    char lastChannelNote_[SONG_CHANNEL_COUNT][3] ;
    bool waveformPrimed_ ;
    int waveformLineTop_[WAVEFORM_DRAW_COLUMNS] ;
    int waveformLineBottom_[WAVEFORM_DRAW_COLUMNS] ;
    int waveformUpper_[WAVEFORM_DRAW_COLUMNS] ;
    int waveformLower_[WAVEFORM_DRAW_COLUMNS] ;

	struct {                      // .Clipboard structure
        bool active_ ;            // .If currently making a selection
        unsigned char *data_ ;    // .Null if clipboard empty
        int x_ ;                  // .Current selection positions
        int y_ ;                  // .
        int offset_ ;             // .
        int width_ ;              // .Size of selection
        int height_ ;             // .
    } clipboard_ ;

	int saveX_ ;
	int saveY_ ;
	int saveOffset_ ;
	bool invertBatt_ ;
	bool soloOn_ ;
} ;
#endif
