#include "Application/AppWindow.h"
#include "MixerView.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
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

void MixerView::updateCursor(int dx,int dy) {
	int x=viewData_->mixerCol_ ;
	x+=dx ;
	if (x<0) x=0 ;
	if (x>7) x=7 ;
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

	if (mask&EPBM_B) {
	} else {

	  // A modifier

	  if (mask&EPBM_A) {

	  } else {

		  // R Modifier

          	if (mask&EPBM_R) {
				if (mask&EPBM_UP) {
					ViewType vt=VT_SONG;
					ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
					SetChanged();
					NotifyObservers(&ve) ;
				}	
				if (mask&EPBM_START) {
				    onStop() ;
                }
	    	} else {

			// L Modifier
			
				if (mask&EPBM_L) {

				} else {
					// No modif
					if (mask&EPBM_START) {
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
	sprintf(strbuffer,"%3.3d%%",player->GetPlayedBufferPercentage()) ; 
	DrawString(pos._x,pos._y,strbuffer,props) ;

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

void MixerView::drawChannelMeters(bool force) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    MixerService *mixer = MixerService::GetInstance();
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    const int startX = 24;
    const int labelRow = 4;
    const int meterTop = 52;
    const int meterHeight = 38;
    const int stripWidth = 24;
    const int meterWidth = 12;
    const int scopeY = 96;
    const int scopeHeight = 14;
    const int noteRow = 14;
    GUIColor panel=AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor meterLow=AppWindow::ThemeColor(CD_HILITE1);
    GUIColor meterHot=AppWindow::ThemeColor(CD_HILITE2);
    GUIColor meterClip=AppWindow::ThemeColor(CD_NORMAL);
    GUIColor selected=AppWindow::ThemeColor(CD_NORMAL);
    GUIColor frame=AppWindow::ThemeBlend(CD_BACKGROUND,CD_BORDER,45);

    if (force) {
        imp->SetColor(panel);
        GUIRect area(0, 28, 240, 126);
        imp->DrawRect(area);
    }

    GUITextProperties props;
    props.invert_ = false;
    char label[3];
    for (int i=0; i<SONG_CHANNEL_COUNT; i++) {
        int stripX = startX + i * stripWidth;
        int x = stripX + 6;
        int bus = Mixer::GetInstance()->GetBus(i);
        int level = mixer->GetBusPeakPercent(bus);
        if (level<0) level=0;
        if (level>100) level=100;
        bool isSelected = (i == viewData_->mixerCol_);
        bool isActive = Player::GetInstance()->IsChannelPlaying(i) || level > 0;

        imp->SetColor(isSelected ? selected : frame);
        GUIRect outline(x, meterTop - 1, x + meterWidth + 1, meterTop + meterHeight + 1);
        imp->DrawRect(outline);
        imp->SetColor(panel);
        GUIRect inner(x + 1, meterTop, x + meterWidth, meterTop + meterHeight);
        imp->DrawRect(inner);

        int fill = (level * (meterHeight - 2)) / 100;
        if (fill > 0) {
            if (level > 92) {
                imp->SetColor(meterClip);
            } else if (level > 68) {
                imp->SetColor(meterHot);
            } else {
                imp->SetColor(isActive ? meterLow : frame);
            }
            GUIRect bar(x + 2, meterTop + meterHeight - 1 - fill,
                        x + meterWidth - 1, meterTop + meterHeight - 1);
            imp->DrawRect(bar);
        }

        SetColor(isSelected ? CD_HILITE2 : CD_NORMAL);
        hex2char(bus, label);
        DrawString(stripX / 8, labelRow, label, props);
        drawChannelWaveform(bus, stripX + 4, scopeY, 16, scopeHeight, isSelected);

        SetColor(CD_NORMAL);
        DrawString(stripX / 8, noteRow, "   ", props);
        if (Player::GetInstance()->IsRunning() && viewData_->playMode_ != PM_AUDITION) {
            const char *playedNote = Player::GetInstance()->GetPlayedNote(i);
            char noteChar = playedNote && playedNote[0] ? playedNote[0] : ' ';
            const char *playedOctave = Player::GetInstance()->GetPlayedOctive(i);
            if (noteChar != ' ') {
                lastChannelNote_[i][0] = noteChar;
                if (playedOctave && playedOctave[1] && playedOctave[1] != ' ') {
                    lastChannelNote_[i][1] = playedOctave[1];
                } else if (playedOctave && playedOctave[0] &&
                           playedOctave[0] != ' ') {
                    lastChannelNote_[i][1] = playedOctave[0];
                } else {
                    lastChannelNote_[i][1] = '-';
                }
            }
            SetColor(isSelected ? CD_HILITE2 : CD_HILITE1);
            DrawString(stripX / 8, noteRow, lastChannelNote_[i], props);
        } else {
            lastChannelNote_[i][0]='-';
            lastChannelNote_[i][1]='-';
            SetColor(CD_NORMAL);
            DrawString(stripX / 8, noteRow, lastChannelNote_[i], props);
        }
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
    const int y = 132;
    const int width = 192;
    const int height = 56;
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
