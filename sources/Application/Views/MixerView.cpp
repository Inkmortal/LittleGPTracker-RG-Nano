#include "Application/AppWindow.h"
#include "MixerView.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "UIController.h"
#include "Application/Utils/char.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <string>
#include <iostream>
#include <sstream>

MixerView::MixerView(GUIWindow &w,ViewData *viewData):View(w,viewData) {
	clipboard_.active_=false ;
	clipboard_.data_=0 ;
	invertBatt_=false;
	soloOn_=false;
    lastWaveformDrawMs_=0;
    waveformPrimed_=false;
    for (int i=0; i<SONG_CHANNEL_COUNT; i++) {
        lastChannelNote_[i][0]='-';
        lastChannelNote_[i][1]='-';
        lastChannelNote_[i][2]=0;
    }
    for (int i=0; i<WAVEFORM_DRAW_COLUMNS; i++) {
        waveformLineTop_[i]=-1;
        waveformLineBottom_[i]=-1;
        waveformUpper_[i]=-1;
        waveformLower_[i]=-1;
    }
}

MixerView::~MixerView() {
} 


void MixerView::onStart() {
	Player *player=Player::GetInstance() ;
	unsigned char from=viewData_->songX_ ;
	unsigned char to=from ;
	//if (clipboard_.active_) {
	//	GUIRect r=getSelectionRect();
	//	from=r.Left() ;
	//	to=r.Right() ;
	//}
	player->OnStartButton(PM_SONG,from,false,to) ;
} ;

void MixerView::onStop() {
	Player *player=Player::GetInstance() ;
	unsigned char from=viewData_->songX_ ;
	unsigned char to=from ;
	player->OnStartButton(PM_SONG,from,true,to) ;
} ;

void MixerView::OnFocus() {
} ;

// 8 tracks, reverb return, delay return, master
#define MIXER_STRIPS 12
#define STRIP_MASTER 11
static const int stripCol[MIXER_STRIPS]={3,5,7,9,11,13,15,17,20,22,24,27};
static const char stripName[MIXER_STRIPS]={'1','2','3','4','5','6','7','8','C','D','R','M'};
// The send-effect returns, in strip order after the tracks
static const int returnLevel[3]={MIXER_LEVEL_CHORUS,MIXER_LEVEL_DELAY,MIXER_LEVEL_REVERB};
static const char *returnName[3]={"CHORUS","ECHO","REVERB"};

void MixerView::updateCursor(int dx,int dy) {
	int x=viewData_->mixerCol_ ;
	x+=dx ;
	if (x<0) x=0 ;
	if (x>=MIXER_STRIPS) x=MIXER_STRIPS-1 ;
	viewData_->mixerCol_=x ;
	isDirty_=true;
}

void MixerView::ProcessButtonMask(unsigned short mask,bool pressed) {
	//if (!pressed) {
	//	if (viewMode_==VM_MUTEON) {
	//		if (mask&EPBM_R) {
	//			toggleMute() ;
	//		}
	//	} ;
	//	if (viewMode_==VM_SOLOON) {
	//		if (mask&EPBM_R) {
	//			switchSoloMode() ;
	//		}
	//	} ;
	//	return ;
	//} ;
	//
	// Like every other screen, act on presses only
	if (!pressed) return ;
	
	if (clipboard_.active_) {
		viewMode_=VM_SELECTION ;
	} ;
	// Process selection related keys
	
	if (viewMode_==VM_SELECTION) {
        if (clipboard_.active_==false) {
            clipboard_.active_=true ;
            clipboard_.x_=viewData_->songX_ ;
            clipboard_.y_=viewData_->songY_ ;
            clipboard_.offset_=viewData_->songOffset_ ;
			saveX_=clipboard_.x_ ;
			saveY_=clipboard_.y_ ;
			saveOffset_=clipboard_.offset_ ;
        }
        processSelectionButtonMask(mask) ;
    } else {
	   
       // Switch back to normal mode

        viewMode_=VM_NORMAL ;
        processNormalButtonMask(mask) ;
    }
} ;


/******************************************************
 processNormalButtonMask:
        process button mask in the case there is no
        selection active
 ******************************************************/
 
void MixerView::processNormalButtonMask(unsigned int mask) {

	// B Modifier

	// Same as Song/Chain/Phrase: B+RB mutes, A+RB solos
	int strip=viewData_->mixerCol_ ;
	UIController *controller=UIController::GetInstance() ;
	if (mask&EPBM_B) {
		if ((mask&EPBM_R) && strip<SONG_CHANNEL_COUNT) {
			controller->ToggleMute(strip,strip) ;
			isDirty_=true ;
		}
		if (mask==(EPBM_B|EPBM_A)) {
			// Back to unity, like B+A resets a knob anywhere
			setStripLevel(strip,strip==STRIP_MASTER?100:MIXER_UNITY) ;
			isDirty_=true ;
		}
	} else {

	  // A modifier: faders, and solo with RB

	  if (mask&EPBM_A) {
		if (mask&EPBM_R) {
			if (strip<SONG_CHANNEL_COUNT) {
				controller->SwitchSoloMode(strip,strip,!soloOn_) ;
				soloOn_=!soloOn_ ;
			}
		} else {
			int step=(strip==STRIP_MASTER)?10:0x10 ;
			if (mask&EPBM_UP) setStripLevel(strip,stripLevel(strip)+step) ;
			if (mask&EPBM_DOWN) setStripLevel(strip,stripLevel(strip)-step) ;
			if (mask&EPBM_RIGHT) setStripLevel(strip,stripLevel(strip)+1) ;
			if (mask&EPBM_LEFT) setStripLevel(strip,stripLevel(strip)-1) ;
		}
		isDirty_=true ;
	  } else {

		  // R Modifier

          	if (mask&EPBM_R) {
				if (mask&EPBM_L) {
					controller->UnMuteAll() ;  // RB+LB, as on the other screens
					isDirty_=true ;
				}
				if (mask&EPBM_UP) {
					ViewType vt=VT_SONG;
					ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
					SetChanged();
					NotifyObservers(&ve) ;
				}
				if (mask&EPBM_DOWN) {
					ViewType vt=VT_FX;
					ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
					SetChanged();
					NotifyObservers(&ve) ;
				}
	    	} else {

			// L Modifier
			
				if (mask&EPBM_L) {

				} else {
					// No modif
					if (mask==EPBM_START) {
						onStart() ;
					}
				   if (mask&EPBM_LEFT) updateCursor(-1,0)  ;
				   if (mask&EPBM_RIGHT) updateCursor(1,0) ;
				}
		    }
	  } 
	}
} ;

/******************************************************
 processSelectionButtonMask:
        process button mask in the case there is a
        selection active
 ******************************************************/
 
void MixerView::processSelectionButtonMask(unsigned int mask) {

	// B Modifier

	if (mask&EPBM_B) {

    } else {

	  // A modifier

	  if (mask&EPBM_A) {

	  } else {

		  // R Modifier

          	if (mask&EPBM_R) {
 				if (mask&EPBM_START) {
				    onStop() ;
                }
	    	} else {

    			// No modifier
	          		if (mask&EPBM_START) {
					   onStart() ;
	    			}
		    }
	  } 
	}
}

void MixerView::DrawView() {

	Clear() ;

	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;

// Draw title

	SetColor(CD_NORMAL) ;

	Player *player=Player::GetInstance() ;
	
	std::ostringstream os;

	os << "Mixer " << ((player->GetSequencerMode()==SM_SONG)?"Song":"Live") ;

    std::string buffer(os.str());

	DrawString(pos._x,pos._y,buffer.c_str(),props) ;

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    drawChannelMeters(true);
    drawStripInfo();
    drawWaveform(true);
#else
	// Now draw busses

	GUIPoint anchor=GetAnchor() ;
	char hex[3] ;
	pos=anchor ;
	short dx=3;

	for (int i=0;i<8;i++) {
		if (i==viewData_->mixerCol_) {
			 props.invert_=true;
			 SetColor(CD_HILITE2) ;
		}
		int bus=Mixer::GetInstance()->GetBus(i) ;
		hex2char(bus,hex) ;
		DrawString(pos._x,pos._y,hex,props) ;
		pos._x+=dx ;	
		if (i==viewData_->mixerCol_) {
			 props.invert_=false;
			 SetColor(CD_NORMAL) ;
		}
	}; 
	
    drawWaveform(true) ;
    drawMap() ;
	drawNotes() ;
#endif
    
	if (player->IsRunning()) {
		OnPlayerUpdate(PET_UPDATE) ;
	} ;
} ;

void MixerView::OnPlayerUpdate(PlayerEventType ,unsigned int tick) {

	Player *player=Player::GetInstance() ;

	// Draw clipping indicator & CPU usage

	GUIPoint anchor=GetAnchor() ;
	GUIPoint pos=anchor ;

	GUITextProperties props ;
	SetColor(CD_NORMAL) ;

    if (View::miniLayout_) {
      pos._y=0 ;
      pos._x=25 ;
    } else {
      pos=anchor ;
      pos._x+=25 ;
    }
    
	if (player->Clipped()) {
           DrawString(pos._x,pos._y,"clip",props); 
    } else {
           DrawString(pos._x,pos._y,"----",props); 
    }
	char strbuffer[10] ;

	pos._y+=1 ;
	// Audio engine load; amber when the device is close to its limit
	int load=player->GetPlayedBufferPercentage() ;
	sprintf(strbuffer,"%3.3d%%",load>999?999:load) ;
	SetColor(load>=80?CD_CURSOR:CD_NORMAL) ;
	DrawString(pos._x,pos._y,strbuffer,props) ;
	SetColor(CD_NORMAL) ;

    System *sys=System::GetInstance() ;
    int batt=sys->GetBatteryLevel() ;
    if (batt>=0) {
		if (batt<90) {
			SetColor(CD_HILITE2) ;
			invertBatt_=!invertBatt_ ;
		} else {
			invertBatt_=false ;
		} ;
		props.invert_=invertBatt_ ;

	    pos._y+=1 ;
    	sprintf(strbuffer,"%3.3d",batt) ; 
	    DrawString(pos._x,pos._y,strbuffer,props) ;
    }
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
    int time=int(player->GetPlayTime()) ;
    int mi=time/60 ;
    int se=time-mi*60 ;
	sprintf(strbuffer,"%2.2d:%2.2d",mi,se) ; 
	pos._y+=1 ;	
	DrawString(pos._x,pos._y,strbuffer,props) ;

    drawChannelMeters() ;
    drawWaveform() ;

} ;

int MixerView::stripLevel(int strip) {
	if (strip==STRIP_MASTER) {
		return viewData_->project_->GetMasterVolume() ;
	}
	return Mixer::GetInstance()->GetLevel(strip<SONG_CHANNEL_COUNT?strip:
	       returnLevel[strip-SONG_CHANNEL_COUNT]) ;
}

void MixerView::setStripLevel(int strip,int value) {
	if (strip==STRIP_MASTER) {
		Variable *v=viewData_->project_->FindVariable(VAR_MASTERVOL) ;
		if (v) {
			if (value<0) value=0 ;
			if (value>100) value=100 ;
			v->SetInt(value) ;
		}
		return ;
	}
	Mixer::GetInstance()->SetLevel(strip<SONG_CHANNEL_COUNT?strip:
	      returnLevel[strip-SONG_CHANNEL_COUNT],value) ;
}

// The selected strip in words, and how to use the screen
void MixerView::drawStripInfo() {
	GUITextProperties props ;
	char line[40] ;
	int strip=viewData_->mixerCol_ ;
	int level=stripLevel(strip) ;
	if (strip<SONG_CHANNEL_COUNT) {
		sprintf(line,"TRACK %d  %02X  %3d%%%s",strip+1,level,(level*100)/MIXER_UNITY,
		        Player::GetInstance()->IsChannelMuted(strip)?"  muted":"") ;
	} else if (strip==STRIP_MASTER) {
		sprintf(line,"MASTER  %3d%%",level) ;
	} else {
		sprintf(line,"%s RETURN  %02X  %3d%%",returnName[strip-SONG_CHANNEL_COUNT],level,(level*100)/MIXER_UNITY) ;
	}
	SetColor(CD_HILITE1) ;
	DrawString(1,21,"                            ",props) ;
	DrawString(1,21,line,props) ;
	SetColor(CD_MUTE) ;
	DrawString(1,22,"A+Up/Dn level B+RB mute",props) ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawChannelMeters(bool force) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    MixerService *mixer = MixerService::GetInstance();
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    Player *player = Player::GetInstance();
    const int top = 40;
    const int height = 96;
    GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor slot=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,35);
    GUIColor fill=AppWindow::ThemeBlend(CD_BACKGROUND,CD_HILITE2,55);
    GUIColor fillOn=AppWindow::ThemeColor(CD_HILITE2);
    GUIColor knob=AppWindow::ThemeColor(CD_NORMAL);
    GUIColor knobOn=AppWindow::ThemeColor(CD_CURSOR);
    GUIColor meter=AppWindow::ThemeColor(CD_PLAY);
    GUIColor hot=AppWindow::ThemeColor(CD_CURSOR);
    GUIColor muted=AppWindow::ThemeColor(CD_MUTE);

    if (force) {
        imp->SetColor(panel);
        GUIRect area(0, top - 4, 240, top + height + 4);
        imp->DrawRect(area);
    }

    GUITextProperties props;
    props.invert_ = false;
    char label[2] = {0, 0};
    for (int s=0; s<MIXER_STRIPS; s++) {
        bool selected = (s == viewData_->mixerCol_);
        bool isMuted = s < SONG_CHANNEL_COUNT && player->IsChannelMuted(s);
        int x = stripCol[s] * 8 + 1;
        // Fader: slot, filled up to the level, a knob at the level
        int value = stripLevel(s);
        int full = (s == STRIP_MASTER) ? 100 : 0xFF;
        int h = (value * (height - 4)) / full;
        imp->SetColor(slot);
        GUIRect track(x, top, x + 6, top + height);
        imp->DrawRect(track);
        imp->SetColor(isMuted ? muted : (selected ? fillOn : fill));
        GUIRect body(x + 1, top + height - h, x + 5, top + height);
        imp->DrawRect(body);
        imp->SetColor(selected ? knobOn : knob);
        GUIRect cap(x - 1, top + height - h - 2, x + 7, top + height - h + 1);
        imp->DrawRect(cap);
        // A tick at unity (C0) on the track and return faders
        if (s != STRIP_MASTER) {
            int uy = top + height - (MIXER_UNITY * (height - 4)) / 0xFF;
            imp->SetColor(slot);
            GUIRect unity(x + 7, uy, x + 9, uy + 1);
            imp->DrawRect(unity);
        }
        // Live level beside the fader (tracks and master)
        int level = 0;
        if (s < SONG_CHANNEL_COUNT) {
            level = mixer->GetBusPeakPercent(Mixer::GetInstance()->GetBus(s));
        } else if (s == STRIP_MASTER) {
            level = mixer->GetMasterPeakPercent();
        }
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        int m = (level * height) / 100;
        imp->SetColor(panel);
        GUIRect clearMeter(x + 9, top, x + 12, top + height);
        imp->DrawRect(clearMeter);
        if (m > 0) {
            imp->SetColor(level > 92 ? hot : meter);
            GUIRect bar(x + 9, top + height - m, x + 12, top + height);
            imp->DrawRect(bar);
        }

        label[0] = stripName[s];
        SetColor(selected ? CD_CURSOR : (s >= SONG_CHANNEL_COUNT ? CD_HILITE1 : CD_NORMAL));
        props.invert_ = selected;
        DrawString(stripCol[s], 18, label, props);
        props.invert_ = false;
        SetColor(CD_MUTE);
        DrawString(stripCol[s], 19, isMuted ? "M" : " ", props);
    }
    SetColor(CD_NORMAL);
#endif
}

void MixerView::drawChannelWaveform(int bus, int x, int y, int width, int height, bool selected) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    MixerService *mixer = MixerService::GetInstance();
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    const int columns = AudioMixer::WAVEFORM_SIZE;
    const int mid = y + (height / 2);
    GUIColor background=AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor trace = selected ? AppWindow::ThemeColor(CD_NORMAL) : AppWindow::ThemeColor(CD_HILITE2);

    imp->SetColor(background);
    GUIRect clear(x, y, x + width, y + height);
    imp->DrawRect(clear);

    int peakAbs = 1;
    for (int i=0; i<columns; i++) {
        int sample = mixer->GetBusWaveformSample(bus, i);
        int absSample = sample < 0 ? -sample : sample;
        if (absSample > peakAbs) peakAbs = absSample;
    }
    int gain = (peakAbs < 100) ? ((100 * 100) / peakAbs) : 100;
    if (gain > 500) gain = 500;

    int previousY = mid;
    imp->SetColor(trace);
    for (int col=0; col<width; col++) {
        int startIndex = (col * columns) / width;
        int endIndex = ((col + 1) * columns) / width;
        if (endIndex <= startIndex) endIndex = startIndex + 1;
        if (endIndex > columns) endIndex = columns;

        int total = 0;
        int count = 0;
        for (int index=startIndex; index<endIndex; index++) {
            total += mixer->GetBusWaveformSample(bus, index);
            count++;
        }
        int sample = count > 0 ? total / count : 0;
        int waveY = mid - ((sample * gain * (height / 2 - 2)) / 10000);
        if (waveY < y + 1) waveY = y + 1;
        if (waveY >= y + height - 1) waveY = y + height - 2;

        int top = previousY < waveY ? previousY : waveY;
        int bottom = previousY > waveY ? previousY : waveY;
        if (bottom - top < 1) {
            bottom = top + 1;
        }
        if (bottom >= y + height - 1) bottom = y + height - 2;
        GUIRect wave(x + col, top, x + col + 1, bottom + 1);
        imp->DrawRect(wave);
        previousY = waveY;
    }
#endif
}

void MixerView::drawWaveform(bool force) {
    MixerService *mixer = MixerService::GetInstance();

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    unsigned int now=SDL_GetTicks();
    if (!force && lastWaveformDrawMs_!=0 && now-lastWaveformDrawMs_<33) {
        return;
    }
    lastWaveformDrawMs_=now;

    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    const int x = 24;
    const int y = 188;
    const int width = 192;
    const int height = 36;
    const int mid = y + (height / 2);
    const int columns = AudioMixer::WAVEFORM_SIZE;
    const int stepPx = 1;
    GUIColor scopeBackground=AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor scopeTrace=AppWindow::ThemeColor(CD_HILITE2);
    GUIColor scopeCenter=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);
    int peakAbs = 1;
    for (int i=0; i<columns; i++) {
        int sample = mixer->GetMasterWaveformSample(i);
        int absSample = sample < 0 ? -sample : sample;
        if (absSample > peakAbs) peakAbs = absSample;
    }
    int gain = (peakAbs < 85) ? ((85 * 100) / peakAbs) : 100;
    if (gain > 520) gain = 520;

    imp->SetColor(scopeBackground);
    GUIRect clear(x, y, x + width, y + height);
    imp->DrawRect(clear);

    if (force || !waveformPrimed_) {
        for (int i=0; i<WAVEFORM_DRAW_COLUMNS; i++) {
            waveformLineTop_[i]=-1;
            waveformLineBottom_[i]=-1;
            waveformUpper_[i]=-1;
            waveformLower_[i]=-1;
        }
        waveformPrimed_=true;
    }

    imp->SetColor(scopeCenter);
    for (int col = 0; col < width; col += 12) {
        GUIRect center(x + col, mid, x + col + 2, mid + 1);
        imp->DrawRect(center);
    }

    for (int col = 0; col < width; col += stepPx) {
        int drawIndex = col / stepPx;
        int scaled = (col * (columns - 1) * 256) / (width - 1);
        int sampleIndex = scaled / 256;
        int frac = scaled - (sampleIndex * 256);
        int nextIndex = sampleIndex + 1;
        if (nextIndex >= columns) nextIndex = columns - 1;
        int sample = mixer->GetMasterWaveformSample(sampleIndex);
        int nextSample = mixer->GetMasterWaveformSample(nextIndex);
        int smoothed = sample + (((nextSample - sample) * frac) / 256);
        int waveY = mid - ((smoothed * gain * (height / 2 - 2)) / 10000);
        if (waveY < y + 2) waveY = y + 2;
        if (waveY >= y + height - 2) waveY = y + height - 3;

        imp->SetColor(scopeTrace);
        int lineTop = waveY;
        int lineBottom = waveY;
        GUIRect wave(x + col, waveY, x + col + stepPx, waveY + 1);
        imp->DrawRect(wave);

        if (sampleIndex>0) {
            int prevScaled = ((col - stepPx) * (columns - 1) * 256) / (width - 1);
            if (prevScaled < 0) prevScaled = 0;
            int prevIndex = prevScaled / 256;
            int prevFrac = prevScaled - (prevIndex * 256);
            int prevNextIndex = prevIndex + 1;
            if (prevNextIndex >= columns) prevNextIndex = columns - 1;
            int prevSample = mixer->GetMasterWaveformSample(prevIndex);
            int prevNextSample = mixer->GetMasterWaveformSample(prevNextIndex);
            int prevSmoothed = prevSample + (((prevNextSample - prevSample) * prevFrac) / 256);
            int prevY = mid - ((prevSmoothed * gain * (height / 2 - 2)) / 10000);
            if (prevY < y + 2) prevY = y + 2;
            if (prevY >= y + height - 2) prevY = y + height - 3;
            if (prevY != waveY) {
                int delta = prevY > waveY ? prevY - waveY : waveY - prevY;
                int bridgeTop = waveY;
                int bridgeBottom = waveY;
                if (delta <= 4) {
                    bridgeTop = prevY < waveY ? prevY : waveY;
                    bridgeBottom = prevY > waveY ? prevY : waveY;
                } else if (prevY < waveY) {
                    bridgeTop = waveY - 2;
                } else {
                    bridgeBottom = waveY + 2;
                }
                if (bridgeTop < y + 2) bridgeTop = y + 2;
                if (bridgeBottom >= y + height - 2) bridgeBottom = y + height - 3;
                GUIRect bridge(x + col, bridgeTop, x + col + 1, bridgeBottom + 1);
                imp->DrawRect(bridge);
                lineTop = bridgeTop;
                lineBottom = bridgeBottom;
            }
        }

        waveformLineTop_[drawIndex]=lineTop;
        waveformLineBottom_[drawIndex]=lineBottom;
    }
#else
    GUITextProperties props;
    GUIPoint pos;
    pos._x = 3;
    pos._y = 72;
    DrawString(pos._x, pos._y, "scope unavailable", props);
#endif
}
