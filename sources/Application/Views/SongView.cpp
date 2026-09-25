#include "SongView.h"
#include <string.h>
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/ProjectDatas.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"
#include "UIController.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#include "Application/AppWindow.h"
#include <SDL/SDL.h>
#endif
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

/****************
 Constructor
 ****************/

SongView::SongView(GUIWindow &w, ViewData *viewData, const char *song)
    : View(w, viewData) {

    updatingChain_ = false;
    lastChain_ = 0;
    songname_ = song;

    for (int i = 0; i < 8; i++) {
        this->lastPlayedPosition_[i] = 0;
        this->lastQueuedPosition_[i] = 0;
        this->queuedCellShown_[i] = -1;
    }
    clipboard_.active_ = false;
    clipboard_.data_ = 0;
    invertBatt_ = false;
    canDeepClone_ = false;
    jumpLength_ = 0x10; // B-jump 16 rows like LSDJ
    reorder_ = false;
    pastedOnA_ = false;
}

/****************
 Destructor
 ****************/

SongView::~SongView() {
    if (clipboard_.data_ != 0)
        SYS_FREE((void *)clipboard_.data_);
};

/******************************************************
 updateChain:
        update current chain value by adding offset
        parameter
 ******************************************************/

void SongView::updateChain(int offset) {

    unsigned int chain = viewData_->UpdateSongChain(offset);
    updatingChain_ = true;
    lastChain_ = chain;
    updateX_ = viewData_->songX_;
    updateY_ = viewData_->songY_;
    isDirty_ = true;
    canDeepClone_ = false;
}

/******************************************************
 updateChain:
        set current chain value to value parameter
 ******************************************************/

void SongView::setChain(unsigned char value) {
    viewData_->SetSongChain(value);
    lastChain_ = value;
    isDirty_ = true;
}

/******************************************************
 updateSongOffset:
        Jump from the current position up or down
    by [offset] rows
 ******************************************************/

void SongView::updateSongOffset(int offset) {
    viewData_->UpdateSongOffset(offset);
    isDirty_ = true;
    canDeepClone_ = false;
}

/******************************************************
 updateCursor:
        modify location of cursor in view by
        adding dx & dy parameters
 ******************************************************/

void SongView::updateCursor(int dx, int dy) {
    viewData_->UpdateSongCursor(dx, dy);
    isDirty_ = true;
    canDeepClone_ = false;
}

/******************************************************
 cutPosition:
        copy current position content to clipboard &
        erase current position value
 ******************************************************/

void SongView::cutPosition() {

    // prepare selection data
    clipboard_.x_ = viewData_->songX_;
    clipboard_.y_ = viewData_->songY_;
    clipboard_.offset_ = viewData_->songOffset_;

    saveX_ = viewData_->songX_;
    saveY_ = viewData_->songY_;
    saveOffset_ = viewData_->songOffset_;

    // cut selection
    cutSelection();
};

/******************************************************
 pastePosition:
        set current position to last chain value if
        current step is empty
 ******************************************************/

bool SongView::pasteLast() {

    // If we're on an empty spot, we past the last chain
    // otherwise we take the current chain as last

    unsigned char *c = viewData_->GetCurrentSongPointer();
    bool wasEmpty = (*c == 0xFF);
    if (wasEmpty) {
        *c = lastChain_;
        viewData_->song_->chain_->SetUsed(*c);
        isDirty_ = true;
        // Pasting re-uses the last chain; make that visible for beginners
        static char hint[40];
        bool hasContent = viewData_->song_->chain_->data_[(*c) * 16] != 0xFF;
        sprintf(hint, hasContent ? "Reused %2.2X. A again = new" : "Chain %2.2X", *c);
        View::SetNotification(hint);
    } else {
        lastChain_ = *c;
    }
    return wasEmpty;
};

/******************************************************
 clonePosition:
        slim clone current position
 ******************************************************/

void SongView::clonePosition() {

    unsigned char *pos = viewData_->GetCurrentSongPointer();
    unsigned char current = *pos;
    if (current == 255)
        return;

    unsigned short next = viewData_->song_->chain_->GetNext();
    if (next == NO_MORE_CHAIN)
        return;

    unsigned char *src = viewData_->song_->chain_->data_ + 16 * current;
    unsigned char *dst = viewData_->song_->chain_->data_ + 16 * next;

    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };

    src = viewData_->song_->chain_->transpose_ + 16 * current;
    dst = viewData_->song_->chain_->transpose_ + 16 * next;

    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };
    setChain((unsigned char)next);
    isDirty_ = true;
};

/******************************************************
 deepClonePosition:
        deep clone chain and all phrases within
        made by koisignal (https://github.com/koi-ikeno)
 ******************************************************/

void SongView::deepClonePosition() {
    Phrase *ph = viewData_->song_->phrase_;
    Chain *ch = viewData_->song_->chain_;
    unsigned char *pos = viewData_->GetCurrentSongPointer();
    unsigned char curChainNum = *pos;

    if (curChainNum == CHAIN_COUNT) {
        View::SetNotification("no more chains!");
        return;
    }

    unsigned char *srcChain = ch->data_ + 16 * curChainNum;
    unsigned char *dstChain = ch->data_ + 16 * curChainNum;
    unsigned short srcPhrases[16];
    unsigned short dstPhrases[16];

    // Init outside valid range
    for (int i = 0; i < 16; i++) {
        srcPhrases[i] = NO_MORE_CHAIN;
        dstPhrases[i] = NO_MORE_CHAIN;
    }

    for (int i = 0; i < 16; i++) {
        unsigned short srcPhraseNum = *srcChain;

        // skip when "--"
        if (srcPhraseNum == CHAIN_COUNT) {
            srcChain++;
            dstChain++;
            continue;
        }

        unsigned short newPhraseNum = NO_MORE_CHAIN;

        for (int j = 0; j < 16; j++) {
            if (srcPhrases[j] == srcPhraseNum) {
                newPhraseNum = dstPhrases[j];
                break;
            }
        }

        if (newPhraseNum == NO_MORE_CHAIN) {
            newPhraseNum = ph->GetNext();
            if (newPhraseNum == NO_MORE_PHRASE) {
                View::SetNotification("no more phrases!");
                return;
            }
            for (int k = 0; k < 16; k++) {
                *(ph->note_ + 16 * newPhraseNum + k) =
                    *(ph->note_ + 16 * srcPhraseNum + k);
                *(ph->instr_ + 16 * newPhraseNum + k) =
                    *(ph->instr_ + 16 * srcPhraseNum + k);
                *(ph->cmd1_ + 16 * newPhraseNum + k) =
                    *(ph->cmd1_ + 16 * srcPhraseNum + k);
                *(ph->cmd2_ + 16 * newPhraseNum + k) =
                    *(ph->cmd2_ + 16 * srcPhraseNum + k);
                *(ph->param1_ + 16 * newPhraseNum + k) =
                    *(ph->param1_ + 16 * srcPhraseNum + k);
                *(ph->param2_ + 16 * newPhraseNum + k) =
                    *(ph->param2_ + 16 * srcPhraseNum + k);
            }
        }
        srcPhrases[i] = srcPhraseNum;
        dstPhrases[i] = newPhraseNum;
        *dstChain = newPhraseNum;
        srcChain++;
        dstChain++;
    }
    View::SetNotification("deep clone");

    setChain((unsigned char)curChainNum);
}

void SongView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 7) {
        if (viewData_->songX_ < clipboard_.x_) {
            viewData_->songX_ = 0;
            clipboard_.x_ = 7;
        } else {
            viewData_->songX_ = 7;
            clipboard_.x_ = 0;
        }
        isDirty_ = true;
    } else {
        if (viewData_->songY_ < clipboard_.y_) {
            viewData_->songY_ = 0;
            clipboard_.y_ = 0x17;
        } else {
            clipboard_.y_ = 0;
            viewData_->songY_ = 0x17;
        }
        isDirty_ = true;
    }
}

/******************************************************
 OnFocus:
        called when current view is becoming active
 ******************************************************/

void SongView::OnFocus() {
    clipboard_.active_ = false;
    reorder_ = false;
};

GUIRect SongView::getSelectionRect() {

    GUIRect selRect(clipboard_.x_, clipboard_.y_ + clipboard_.offset_,
                    viewData_->songX_,
                    viewData_->songY_ + viewData_->songOffset_);

    selRect.Normalize();
    return selRect;
}

/******************************************************
 fillClipboard:
        fill clipboard with current selection value
 ******************************************************/

void SongView::fillClipboardData() {

    // Clear current selection data

    if (!clipboard_.data_)
        SYS_FREE((void *)clipboard_.data_);

    // Prepare selection related information

    GUIRect selRect = getSelectionRect();

    // Set current selection  data

    clipboard_.width_ = selRect.Width() + 1;
    clipboard_.height_ = selRect.Height() + 1;

    clipboard_.data_ =
        (unsigned char *)SYS_MALLOC(clipboard_.width_ * clipboard_.height_);

    unsigned char *src = viewData_->song_->data_ + selRect.Left() +
                         SONG_CHANNEL_COUNT * selRect.Top();
    unsigned char *dst = clipboard_.data_;

    for (int j = 0; j < clipboard_.height_; j++) {
        for (int i = 0; i < clipboard_.width_; i++) {
            *dst++ = *src++;
        }
        src += (SONG_CHANNEL_COUNT - clipboard_.width_);
    }
};

/******************************************************
 copySelection:
        copy current selection to clipboard
 ******************************************************/

void SongView::copySelection() {

    fillClipboardData();
    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->songX_ = saveX_;
    viewData_->songY_ = saveY_;
    viewData_->songOffset_ = saveOffset_;
    View::SetNotification("copied selection");
}

/******************************************************
 cutSelection:
        cut current selection to clipboard
 ******************************************************/

void SongView::cutSelection() {

    // first copy the data to clipboard

    fillClipboardData();
    GUIRect selRect = getSelectionRect();

    // now move all rows up for cut

    unsigned char *dst = viewData_->song_->data_ + selRect.Left() +
                         SONG_CHANNEL_COUNT * (selRect.Top());
    unsigned char *src = dst + SONG_CHANNEL_COUNT * clipboard_.height_;

    int rowCount = SONG_ROW_COUNT - selRect.Bottom() - 1;

    for (int j = 0; j < rowCount; j++) {

        for (int i = 0; i < clipboard_.width_; i++) {
            *dst++ = *src++;
        }
        src += (SONG_CHANNEL_COUNT - clipboard_.width_);
        dst += (SONG_CHANNEL_COUNT - clipboard_.width_);
    }

    for (int j = 0; j > clipboard_.height_; j++) {
        for (int i = 0; i < clipboard_.width_; i++) {
            *dst++ = 0xFF;
        }
        dst += (SONG_CHANNEL_COUNT - clipboard_.width_);
    };

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->songX_ = saveX_;
    viewData_->songY_ = saveY_;
    viewData_->songOffset_ = saveOffset_;

    isDirty_ = true;
}

/******************************************************
 pasteSelection:
        paste clipboard content to song
 ******************************************************/

void SongView::pasteClipboard() {

    if (!clipboard_.data_)
        return;

    // Check we're not out of scope

    int width = clipboard_.width_;
    int height = clipboard_.height_;

    if (viewData_->songX_ + width > SONG_CHANNEL_COUNT) {
        width = SONG_CHANNEL_COUNT - viewData_->songX_;
    }
    if (viewData_->songY_ + viewData_->songOffset_ + height > SONG_ROW_COUNT) {
        height = SONG_ROW_COUNT - viewData_->songY_ - viewData_->songOffset_;
    } else {

        // Move down from insert point

        unsigned char *dst = viewData_->song_->data_ + viewData_->songX_ +
                             (SONG_ROW_COUNT - 1) * SONG_CHANNEL_COUNT;
        unsigned char *src = dst - height * SONG_CHANNEL_COUNT;

        int rowCount =
            SONG_ROW_COUNT - (viewData_->songY_ + viewData_->songOffset_);

        for (int j = 0; j < rowCount; j++) {
            for (int i = 0; i < width; i++) {
                *dst++ = *src++;
            }
            dst -= (SONG_CHANNEL_COUNT + width);
            src -= (SONG_CHANNEL_COUNT + width);
        }
    }

    // Prepare copy pointer

    unsigned char *dst = viewData_->GetCurrentSongPointer();
    unsigned char *src = clipboard_.data_;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            *dst++ = *src++;
        }
        dst += (SONG_CHANNEL_COUNT - width);
        src += (clipboard_.width_ - width);
    }

    updateCursor(0, height);
}

void SongView::unMuteAll() {

    UIController *controller = UIController::GetInstance();
    controller->UnMuteAll();
};

void SongView::toggleMute() {

    UIController *controller = UIController::GetInstance();

    int from = viewData_->songX_;
    int to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    };
    controller->ToggleMute(from, to);
    viewMode_ = (viewMode_ != VM_MUTEON) ? VM_MUTEON : VM_NORMAL;
};

void SongView::switchSoloMode() {

    UIController *controller = UIController::GetInstance();
    int from = viewData_->songX_;
    int to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    };
    controller->SwitchSoloMode(from, to, (viewMode_ != VM_SOLOON));
    viewMode_ = (viewMode_ != VM_SOLOON) ? VM_SOLOON : VM_NORMAL;
    isDirty_ = true;
};

void SongView::onStart() {
    // Always play with zero offset in chains when in SongView
    viewData_->chainRow_ = 0;
    Player *player = Player::GetInstance();
    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    }
    int renderMode = viewData_->renderMode_;
    if (renderMode > 0 && !player->IsRunning()) {
        viewData_->isRendering_ = true;
        View::SetNotification("Rendering started!");
    } else if (viewData_->isRendering_ && player->IsRunning()) {
        viewData_->isRendering_ = false;
        View::SetNotification("Rendering done!");
    }
    player->OnSongStartButton(from, to, false, false);
};

void SongView::startCurrentRow() {
    Player *player = Player::GetInstance();
    if (player->GetSequencerMode() != SM_LIVE) {
        toggleLiveMode();
    }
    player->OnSongStartButton(0, 7, false, false);
}

void SongView::startImmediate() {
    Player *player = Player::GetInstance();

    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    player->OnSongStartButton(from, to, false, true);
}

void SongView::onStop() {
    // Always play with zero offset in chains when in SongView
    viewData_->chainRow_ = 0;
    Player *player = Player::GetInstance();
    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    }

    player->OnSongStartButton(from, to, true, false);
};

// LB + Up/Down: the next place a part starts, going that way (wrapping
// round the song): a bookmarked row, or the first chain of a block on this
// track (a chain right under an empty cell or at row 00)
void SongView::jumpToNextSection(int direction) {

    Song *song = viewData_->song_;
    int x = viewData_->songX_;
    int from = viewData_->songY_ + viewData_->songOffset_;
    int target = -1;
    bool bookmark = false;
    for (int i = 1; i < SONG_ROW_COUNT && target < 0; i++) {
        int row = (from + direction * i + SONG_ROW_COUNT) % SONG_ROW_COUNT;
        unsigned char cell = song->data_[x + SONG_CHANNEL_COUNT * row];
        bool blockStart =
            cell != 0xFF &&
            (row == 0 || song->data_[x + SONG_CHANNEL_COUNT * (row - 1)] == 0xFF);
        if (song->IsBookmarked(row) || blockStart) {
            target = row;
            bookmark = song->IsBookmarked(row);
        }
    }
    if (target < 0) {
        SetNotification("No sections or bookmarks");
        return;
    }

    // Show the target a few rows from the top, like the B jumps
    int visible = View::songRowCount_;
    if (target < viewData_->songOffset_ ||
        target >= viewData_->songOffset_ + visible) {
        viewData_->songOffset_ = target - 4;
        if (viewData_->songOffset_ > SONG_ROW_COUNT - visible) {
            viewData_->songOffset_ = SONG_ROW_COUNT - visible;
        }
        if (viewData_->songOffset_ < 0) {
            viewData_->songOffset_ = 0;
        }
    }
    viewData_->songY_ = target - viewData_->songOffset_;
    static char hint[32];
    sprintf(hint, bookmark ? "Bookmark %2.2X" : "Section %2.2X", target);
    SetNotification(hint);
    isDirty_ = true;
}

void SongView::toggleBookmark() {
    int row = viewData_->songY_ + viewData_->songOffset_;
    viewData_->song_->ToggleBookmark(row);
    static char hint[32];
    sprintf(hint, viewData_->song_->IsBookmarked(row) ? "Bookmark %2.2X set"
                                                      : "Bookmark %2.2X removed",
            row);
    SetNotification(hint);
    isDirty_ = true;
}

// Track reorder mode (Up on row 00, as on the M8): Left/Right pick a track,
// A + Left/Right carries it over its neighbour with its chains, mute and
// mixer level; Down or B goes back to the grid
void SongView::processReorderButtonMask(unsigned int mask) {
    if (mask & EPBM_R) {
        // Screen moves, mute/solo and the song play as usual
        reorder_ = false;
        isDirty_ = true;
        processNormalButtonMask(mask);
        return;
    }
    if (mask == EPBM_DOWN || mask == EPBM_B) {
        reorder_ = false;
        SetNotification("Tracks done");
        isDirty_ = true;
        return;
    }
    if (mask & EPBM_A) {
        if (mask & EPBM_LEFT) {
            moveTrack(-1);
        } else if (mask & EPBM_RIGHT) {
            moveTrack(1);
        }
        return;
    }
    if (mask == EPBM_LEFT || mask == EPBM_RIGHT) {
        updateCursor(mask == EPBM_LEFT ? -1 : 1, 0);
        return;
    }
    if (mask == EPBM_START) {
        onStart();
    }
}

void SongView::moveTrack(int direction) {
    int a = viewData_->songX_;
    int b = a + direction;
    if (b < 0 || b >= SONG_CHANNEL_COUNT) {
        SetNotification(b < 0 ? "Already the first track" : "Already the last track");
        return;
    }
    if (Player::GetInstance()->IsRunning()) {
        SetNotification("Stop playing to move tracks");
        return;
    }
    unsigned char *data = viewData_->song_->data_;
    for (int row = 0; row < SONG_ROW_COUNT; row++) {
        unsigned char t = data[row * SONG_CHANNEL_COUNT + a];
        data[row * SONG_CHANNEL_COUNT + a] = data[row * SONG_CHANNEL_COUNT + b];
        data[row * SONG_CHANNEL_COUNT + b] = t;
    }
    Mixer *mixer = Mixer::GetInstance();
    int level = mixer->GetLevel(a);
    mixer->SetLevel(a, mixer->GetLevel(b));
    mixer->SetLevel(b, level);
    UIController::GetInstance()->SwapTracks(a, b);
    viewData_->songX_ = b;
    static char hint[32];
    sprintf(hint, "Track %d -> %d", a + 1, b + 1);
    SetNotification(hint);
    isDirty_ = true;
}

/******************************************************
 ProcessButtonMask:
        process button mask even coming from the main
        application window
 ******************************************************/

void SongView::ProcessButtonMask(unsigned short mask, bool pressed) {

    if (!pressed) {
        if (viewMode_ == VM_MUTEON) {
            if (mask & EPBM_R) {
                toggleMute();
            }
        };
        if (viewMode_ == VM_SOLOON) {
            if (mask & EPBM_R) {
                switchSoloMode();
            }
        };
        return;
    };

    // Was the previous press an A that pasted into an empty cell? (only
    // A + Select looks at it, on this press)
    bool pastedOnA = pastedOnA_;
    pastedOnA_ = pastedOnA && mask == (EPBM_A | EPBM_SELECT);

    if (reorder_) {
        processReorderButtonMask(mask);
        return;
    }

    if (viewMode_ == VM_NEW && mask == (EPBM_A | EPBM_SELECT)) {
        viewMode_ = VM_NORMAL;
    }

    if (viewMode_ == VM_NEW) {
        if (mask == EPBM_A) {
            unsigned short next = viewData_->song_->chain_->GetNext();
            if (next != NO_MORE_CHAIN) {
                setChain((unsigned char)next);
                isDirty_ = true;
                static char hint[40];
                sprintf(hint, "New chain %2.2X", (unsigned char)next);
                View::SetNotification(hint);
            }
            mask &= (0xFFFF - EPBM_A);
        }
    }

    if (viewMode_ == VM_CLONE) {
        if ((mask & EPBM_A) && (mask & EPBM_L)) {
            clonePosition();
            mask &= (0xFFFF - (EPBM_A | EPBM_L));
            canDeepClone_ = true;
        } else {
            viewMode_ = VM_SELECTION;
        }
    };

    if (canDeepClone_ && (mask & EPBM_A) && (mask & EPBM_L)) {
        deepClonePosition();
        mask &= (0xFFFF - (EPBM_A | EPBM_L));
        canDeepClone_ = false;
    }
    if (clipboard_.active_) {
        viewMode_ = VM_SELECTION;
    };
    // Process selection related keys

    if (viewMode_ == VM_SELECTION) {
        if (clipboard_.active_ == false) {
            clipboard_.active_ = true;
            clipboard_.x_ = viewData_->songX_;
            clipboard_.y_ = viewData_->songY_;
            clipboard_.offset_ = viewData_->songOffset_;
            saveX_ = clipboard_.x_;
            saveY_ = clipboard_.y_;
            saveOffset_ = clipboard_.offset_;
        }
        processSelectionButtonMask(mask);
    } else {

        // Switch back to normal mode

        viewMode_ = VM_NORMAL;
        processNormalButtonMask(mask);
    }
}

/******************************************************
 processNormalButtonMask:
        process button mask in the case there is no
        selection active
 ******************************************************/

void SongView::toggleLiveMode() {
    Player *player = Player::GetInstance();
    bool live = player->GetSequencerMode() != SM_LIVE;
    player->SetSequencerMode(live ? SM_LIVE : SM_SONG);
    SetNotification(live ? "LIVE: Start cues a cell" : "SONG mode", 0);
    isDirty_ = true;
}

void SongView::processNormalButtonMask(unsigned int mask) {

    if (mask == EPBM_SELECT) {
        toggleLiveMode();
        return;
    }

    if (mask == (EPBM_A | EPBM_SELECT)) {
        // A + Select bookmarks the row. Holding A on an empty cell has just
        // pasted a chain there: that press was the start of this combo
        if (pastedOnA_) {
            *viewData_->GetCurrentSongPointer() = 0xFF;
        }
        toggleBookmark();
        return;
    }

    // B Modifier

    if (mask & EPBM_B) {

        // B+A cuts; B+Up/Down jump. Never both from one press.
        if (mask & EPBM_A)
            mask &= ~(EPBM_UP | EPBM_DOWN);
        if (mask & EPBM_DOWN)
            updateSongOffset(SongView::jumpLength_);
        if (mask & EPBM_UP)
            updateSongOffset(-SongView::jumpLength_);
        if ((mask & EPBM_A) && (!(mask & EPBM_R)))
            cutPosition();
        if (mask & EPBM_L) {

            viewMode_ = VM_CLONE;
        };
        if (mask & EPBM_R) {
            toggleMute();
        };
        if (mask & EPBM_START) {
            if (Player::GetInstance()->GetSequencerMode() == SM_LIVE) {
                Player::GetInstance()->Stop();
            } else {
                startImmediate();
            }
        }
    } else {

        // A modifier

        if (mask & EPBM_A) {

            if (mask & EPBM_DOWN)
                updateChain(-0x10);
            if (mask & EPBM_UP)
                updateChain(0x10);
            if (mask & EPBM_LEFT)
                updateChain(-0x01);
            if (mask & EPBM_RIGHT)
                updateChain(0x01);
            if (mask & EPBM_L && !canDeepClone_) {
                pasteClipboard();
            }
            if (mask == EPBM_A) {
                // Only an empty cell offers "A again = new chain"; on a
                // filled one A would keep replacing it with a new number
                if (pasteLast()) {
                    viewMode_ = VM_NEW;
                    pastedOnA_ = true;
                }
            }
            if (mask & EPBM_R) {
                switchSoloMode();
            };
        } else {

            // R Modifier

            if (mask & EPBM_R) {

                if (mask & EPBM_L) {
                    unMuteAll();
                }

                if (mask & EPBM_RIGHT) {
                    unsigned char *data = viewData_->GetCurrentSongPointer();
                    if (*data != 0xFF) {
                        ViewType vt = VT_CHAIN;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        viewData_->currentChain_ = *data;
                        SetChanged();
                        NotifyObservers(&ve);
                    } else {
                        // Explain why nothing opened instead of doing nothing
                        View::SetNotification("Empty: press A for a chain");
                    }
                }

                if (mask & EPBM_UP) {
                    ViewType vt = VT_PROJECT;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_DOWN) {
                    ViewType vt = VT_MIXER;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if ((mask & EPBM_START) &&
                    Player::GetInstance()->GetSequencerMode() == SM_LIVE) {
                    onStop();
                }

            } else {

                // L Modifier

                if (mask & EPBM_L) {
                    if (mask & EPBM_DOWN)
                        jumpToNextSection(1);
                    if (mask & EPBM_UP)
                        jumpToNextSection(-1);
                    if (mask & EPBM_START)
                        startCurrentRow();
                    if (mask & EPBM_LEFT)
                        nudgeTempo(-1);
                    if (mask & EPBM_RIGHT)
                        nudgeTempo(1);
                } else {

                    // No modifier

                    if (mask == EPBM_UP && viewData_->songY_ == 0 &&
                        viewData_->songOffset_ == 0) {
                        // Up past row 00: move whole tracks (M8's Up Up)
                        reorder_ = true;
                        SetNotification("Move track: A+Left/Right");
                        isDirty_ = true;
                        return;
                    }
                    if (mask & EPBM_DOWN)
                        updateCursor(0, 1);
                    if (mask & EPBM_UP)
                        updateCursor(0, -1);
                    if (mask & EPBM_LEFT)
                        updateCursor(-1, 0);
                    if (mask & EPBM_RIGHT)
                        updateCursor(1, 0);

                    if (mask & EPBM_START) {
                        onStart();
                    }
                }
            }
        }
    }

    if ((!(mask & EPBM_A)) && updatingChain_) {
        unsigned char *c = viewData_->song_->data_ + updateX_ +
                           8 * (viewData_->songOffset_ + updateY_);
        viewData_->song_->chain_->SetUsed(*c);
        updatingChain_ = false;
    }
};

/******************************************************
 processSelectionButtonMask:
        process button mask in the case there is a
        selection active
 ******************************************************/

void SongView::processSelectionButtonMask(unsigned int mask) {

    // B Modifier

    if (mask & EPBM_B) {
        if (mask & EPBM_R) {
            toggleMute();
        };
        if (mask & EPBM_L) {
            extendSelection();
        };
        if (mask == EPBM_B) {
            copySelection();
        }

    } else {

        // A modifier

        if (mask & EPBM_A) {
            if (mask & EPBM_L) {
                cutSelection();
            }
            if (mask & EPBM_R) {
                switchSoloMode();
            };
        } else {

            // R Modifier

            if (mask & EPBM_R) {

                if (mask & EPBM_L) {
                    unMuteAll();
                }

                if (mask & EPBM_RIGHT) {
                    unsigned char *data = viewData_->GetCurrentSongPointer();
                    if (*data != 0xFF) {
                        ViewType vt = VT_CHAIN;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        viewData_->currentChain_ = *data;
                        SetChanged();
                        NotifyObservers(&ve);
                    } else {
                        // Explain why nothing opened instead of doing nothing
                        View::SetNotification("Empty: press A for a chain");
                    }
                }

                if (mask & EPBM_UP) {
                    ViewType vt = VT_PROJECT;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_DOWN) {
                    ViewType vt = VT_MIXER;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if ((mask & EPBM_START) &&
                    Player::GetInstance()->GetSequencerMode() == SM_LIVE) {
                    onStop();
                }

            } else if (!(mask & EPBM_L)) {

                // No modifier (LB combos only work without a selection)

                if (mask & EPBM_DOWN)
                    updateCursor(0, 1);
                if (mask & EPBM_UP)
                    updateCursor(0, -1);
                if (mask & EPBM_LEFT)
                    updateCursor(-1, 0);
                if (mask & EPBM_RIGHT)
                    updateCursor(1, 0);
                if (mask & EPBM_START) {
                    onStart();
                }
            }
        }
    }
}

/******************************************************
 Redraw:
        redraw completely the song view
 ******************************************************/

void SongView::drawSongCell(int track, int songRow, bool queued) {
    int y = songRow - viewData_->songOffset_;
    if (y < 0 || y >= View::songRowCount_ || clipboard_.active_) {
        return;
    }
    GUIPoint anchor = GetAnchor();
    GUITextProperties props;
    unsigned char d =
        viewData_->song_->data_[SONG_CHANNEL_COUNT * songRow + track];
    bool cursor = (track == viewData_->songX_ && y == viewData_->songY_);
    if (queued) {
        SetColor(CD_PLAY);
        props.invert_ = true;
    } else if (cursor) {
        SetColor(((cursorAnimFrame_ / 30) % 2 == 0) ? CD_HILITE2 : CD_HILITE1);
        props.invert_ = true;
    } else {
        SetColor(d == 0xFE ? CD_SONGVIEWFE : (d == 0x00 ? CD_SONGVIEW00 : CD_NORMAL));
    }
    char text[3];
    if (d == 0xFF) {
        strcpy(text, "--");
    } else {
        hex2char(d, text);
    }
    DrawString(anchor._x + track * 3, anchor._y + y, text, props);
    SetColor(CD_NORMAL);
}

void SongView::DrawView() {
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        queuedCellShown_[i] = -1;
    }

    Clear();
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Prepare selection related information

    GUIRect selRect;
    if (clipboard_.active_) {
        selRect = GUIRect(clipboard_.x_, clipboard_.y_ + clipboard_.offset_,
                          viewData_->songX_,
                          viewData_->songY_ + viewData_->songOffset_);

        selRect.Normalize();
    }

    // Draw title

    SetColor(CD_NORMAL);

    Player *player = Player::GetInstance();

    std::ostringstream os;

    if (reorder_) {
        os << "Song - MOVE TRACKS";
    } else {
        os << ((player->GetSequencerMode() == SM_SONG) ? "Song" : "Live");

        os << " - ";
        if (songname_.substr(0, 5) == "lgpt_") {
            os << songname_.substr(5);
        } else {
            os << songname_;
        }
    }
    std::string buffer(os.str());

    DrawString(pos._x, pos._y, buffer.c_str(), props);

    // Compute song grid location

    GUIPoint anchor = GetAnchor();

    // Display row numbers

    char row[3];
    pos = anchor;
    pos._x -= 3;
    for (int j = 0; j < View::songRowCount_; j++) {
        int p = j + viewData_->songOffset_;
        ((p / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        // Bookmarked rows: the number drawn as an amber tag
        if (viewData_->song_->IsBookmarked(p)) {
            SetColor(CD_CURSOR);
            props.invert_ = true;
        }
        hex2char((unsigned char)p, row);
        DrawString(pos._x, pos._y, row, props);
        props.invert_ = false;
        pos._y += 1;
    }

    if (reorder_) {
        // Track numbers above the grid, the one being moved lit
        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            char label[3] = {' ', (char)('1' + i), 0};
            bool current = (i == viewData_->songX_);
            SetColor(current ? CD_HILITE2 : CD_MUTE);
            props.invert_ = current;
            DrawString(anchor._x + i * 3, anchor._y - 1, label, props);
        }
        props.invert_ = false;
    }

    SetColor(CD_NORMAL);

    pos = anchor;
    unsigned char *data =
        viewData_->song_->data_ + (SONG_CHANNEL_COUNT * viewData_->songOffset_);
    short dx = 3;
    short dy = 1;

    for (int j = 0; j < View::songRowCount_; j++) {

        pos._x = anchor._x;
        unsigned char *rowData = viewData_->song_->data_ +
                                  (SONG_CHANNEL_COUNT * (viewData_->songOffset_ + j));

        for (int i = 0; i < 8; i++) {

            bool invert = false;

            // see if we need to invert current step
            if (clipboard_.active_) {
                if ((i >= selRect.Left()) && (i <= selRect.Right()) &&
                    (j + viewData_->songOffset_ >= selRect.Top()) &&
                    (j + viewData_->songOffset_ <= selRect.Bottom())) {
                    invert = true;
                }
            } else {
                if (i == viewData_->songX_ && j == viewData_->songY_) {
                    invert = true;
                }
            }

            // draw current step
            unsigned char d = rowData[i];

            if (d == 0xFE) {
                SetColor(CD_SONGVIEWFE);
            } else if (d == 0x00) {
                SetColor(CD_SONGVIEW00);
            } else {
                SetColor(CD_NORMAL);
            }

            if (invert) {
                // Cursor pulse animation: alternate between HILITE2 and HILITE1
                if ((cursorAnimFrame_ / 30) % 2 == 0) {
                    SetColor(CD_HILITE2);
                } else {
                    SetColor(CD_HILITE1);
                }
                props.invert_ = true;
            }

            if (d == 0xFF) {
                DrawString(pos._x, pos._y, "--", props);
            } else {
                hex2char(d, row);
                DrawString(pos._x, pos._y, row, props);
            }

            if (invert) {
                SetColor(CD_NORMAL);
                props.invert_ = false;
            }

            pos._x += dx;
        }

        pos._y += dy;
        data += SONG_CHANNEL_COUNT;
    }
    SetColor(CD_NORMAL);

    drawMap();
    drawNotes();
    if (reorder_) {
        int y = anchor._y + View::songRowCount_ + 4;
        SetColor(CD_HILITE2);
        DrawString(0, y, "A+Left/Right move  Down done", props);
        SetColor(CD_NORMAL);
    } else if (player->GetSequencerMode() == SM_LIVE) {
        // Live mode's buttons, under the track strip (the title says Live;
        // the helper lists the rest)
        int y = anchor._y + View::songRowCount_ + 4;
        SetColor(CD_PLAY);
        DrawString(0, y, "St cue  LB+St row  RB+St stop", props);
        SetColor(CD_NORMAL);
    }
    drawSideMeters(true);
    drawMiniWaveform(true);

    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };
};

/******************************************************
 OnPlayerUpdate:
        Called when positions in player change. Should
        provide visual feedback of currently played
        position
 ******************************************************/

void SongView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    Player *player = Player::GetInstance();
    drawMiniWaveform(eventType == PET_STOP);

    GUIPoint anchor = GetAnchor();
    GUIPoint pos = anchor;
    pos._x -= 1;

    GUITextProperties props;
    SetColor(CD_CURSOR);

    // Loop on all channels

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {

        // Clear all current positions

        int y = lastPlayedPosition_[i] - viewData_->songOffset_;
        if (y >= 0 && y < View::songRowCount_ &&
            viewData_->playMode_ != PM_AUDITION) {
            pos._y = anchor._y + y;
            DrawString(pos._x, pos._y, " ", props);
        }

        // Clear all last queued positions

        y = lastQueuedPosition_[i] - viewData_->songOffset_;
        if (y >= 0 && y < View::songRowCount_) {
            pos._y = anchor._y + y;
            DrawString(pos._x, pos._y, " ", props);
        }

        // For each playing position, draw current location

        if (player->IsChannelPlaying(i)) {
            if (eventType != PET_STOP) {
                if (viewData_->currentPlayChain_[i] != 0xFF) {
                    int y = viewData_->songPlayPos_[i] - viewData_->songOffset_;
                    if (y >= 0 && y < View::songRowCount_) {
                        pos._y = anchor._y + y;
                        if (!player->IsChannelMuted(i)) {
                            SetColor(CD_PLAY);
                            DrawString(pos._x, pos._y, ">", props);
                        } else {
                            SetColor(CD_MUTE);
                            DrawString(pos._x, pos._y, "-", props);
                        }
                        SetColor(CD_CURSOR);
                        lastPlayedPosition_[i] = viewData_->songPlayPos_[i];
                    }
                }
            }
        }

        // Live mode: the cued cell blinks in the play colour until it starts

        int cue = -1;
        if (player->GetSequencerMode() == SM_LIVE && eventType != PET_STOP &&
            player->GetQueueingMode(i) != QM_NONE) {
            QueueingMode q = player->GetQueueingMode(i);
            if (q == QM_CHAINSTART || q == QM_PHRASESTART || q == QM_TICKSTART) {
                cue = player->GetQueuePosition(i);
            }
        }
        if (queuedCellShown_[i] >= 0 && queuedCellShown_[i] != cue) {
            drawSongCell(i, queuedCellShown_[i], false);
            queuedCellShown_[i] = -1;
        }
        if (cue >= 0) {
            bool on = player->GetLiveIndicator(i)[0] != ' ';
            drawSongCell(i, cue, on);
            queuedCellShown_[i] = cue;
        }
        SetColor(CD_CURSOR);

        // If in live mode, update queued position

        if (player->GetSequencerMode() == SM_LIVE) {
            if (player->GetQueueingMode(i) != QM_NONE) {

                if (eventType != PET_STOP) {
                    int y =
                        player->GetQueuePosition(i) - viewData_->songOffset_;
                    if (y >= 0 && y < View::songRowCount_) {
                        pos._y = anchor._y + y;
                        char *indicator = player->GetLiveIndicator(i);
                        DrawString(pos._x, pos._y, indicator, props);
                        lastQueuedPosition_[i] = player->GetQueuePosition(i);
                    }
                }
            };
        }
        pos._x += 3;
    }

    SetColor(CD_NORMAL);

    // Draw clipping indicator & CPU usage

    if (View::miniLayout_) {
        pos._y = 0;
        pos._x = 25;
    } else {
        pos = anchor;
        pos._x += 25;
    }

    if (player->Clipped()) {
        DrawString(pos._x, pos._y, "clip", props);
    } else {
        DrawString(pos._x, pos._y, "----", props);
    }

    char strbuffer[10];
    pos._y += 1;
    // Audio engine load; amber when the device is close to its limit
    int load = player->GetPlayedBufferPercentage();
    sprintf(strbuffer, "%3.3d%%", load > 999 ? 999 : load);
    SetColor(load >= 80 ? CD_CURSOR : CD_NORMAL);
    DrawString(pos._x, pos._y, strbuffer, props);
    SetColor(CD_NORMAL);

    System *sys = System::GetInstance();
    int batt = sys->GetBatteryLevel();
    if (batt >= 0) {
        if (batt < 90) {
            SetColor(CD_HILITE2);
            invertBatt_ = !invertBatt_;
        } else {
            invertBatt_ = false;
        };
        props.invert_ = invertBatt_;

        pos._y += 1;
        sprintf(strbuffer, "%3.3d", batt);
        DrawString(pos._x, pos._y, strbuffer, props);
    }

    if (eventType != PET_STOP) {
        SetColor(CD_NORMAL);
        props.invert_ = false;
        int time = int(player->GetPlayTime());
        int mi = time / 60;
        int se = time - mi * 60;
        sprintf(strbuffer, "%2.2d:%2.2d", mi, se);
        pos._y += 1;
        DrawString(pos._x, pos._y, strbuffer, props);
    }
    drawNotes();
    drawSideMeters(eventType == PET_STOP);
};

// Level meters in the strip right of the grid: one thin bar per track,
// like a mixer's meter bridge. Empty slots stay visible when stopped.
void SongView::drawSideMeters(bool force) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    if (!ultraCompactLayout_) {
        return;
    }
    static int shown[SONG_CHANNEL_COUNT];
    static unsigned int lastDrawMs = 0;
    unsigned int now = SDL_GetTicks();
    if (!force && lastDrawMs != 0 && now - lastDrawMs < 33) {
        return;
    }
    lastDrawMs = now;

    GUIPoint anchor = GetAnchor();
    const int barWidth = 3;
    const int step = 4;
    // Right after the last track's two-digit column
    const int x0 = (anchor._x + SONG_CHANNEL_COUNT * 3 - 1) * 8;
    const int top = anchor._y * 8 + 1;
    const int bottom = (anchor._y + View::songRowCount_) * 8 - 2;
    if (x0 + SONG_CHANNEL_COUNT * step > w_.GetRect().Width()) {
        return;
    }

    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    Player *player = Player::GetInstance();
    MixerService *mixer = MixerService::GetInstance();
    bool live = player->IsRunning() && viewData_->playMode_ != PM_AUDITION;
    GUIColor slot = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 30);
    GUIColor level = AppWindow::ThemeColor(CD_PLAY);
    GUIColor current = AppWindow::ThemeColor(CD_HILITE2);
    GUIColor hot = AppWindow::ThemeColor(CD_CURSOR);
    GUIColor muted = AppWindow::ThemeColor(CD_MUTE);

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        int target = 0;
        if (live && player->IsChannelPlaying(i)) {
            target = mixer->GetBusPeakPercent(i);
        }
        // Fast attack, slow fall so the bars read smoothly
        shown[i] = (target > shown[i]) ? target : shown[i] - 4;
        if (shown[i] < 0) {
            shown[i] = 0;
        }
        int x = x0 + i * step;
        imp->SetColor(slot);
        GUIRect slotRect(x, top, x + barWidth, bottom);
        imp->DrawRect(slotRect);
        int h = ((bottom - top) * shown[i]) / 100;
        if (h <= 0) {
            continue;
        }
        GUIColor fill = level;
        if (player->IsChannelMuted(i)) {
            fill = muted;
        } else if (i == viewData_->songX_) {
            fill = current;
        }
        imp->SetColor(fill);
        GUIRect fillRect(x, bottom - h, x + barWidth, bottom);
        imp->DrawRect(fillRect);
        // Amber cap near full scale
        if (shown[i] >= 90) {
            imp->SetColor(hot);
            GUIRect capRect(x, bottom - h, x + barWidth, bottom - h + 2);
            imp->DrawRect(capRect);
        }
    }
#endif
}

void SongView::nudgeTempo(int direction) {
    ApplicationCommandDispatcher *dispatcher =
        ApplicationCommandDispatcher::GetInstance();
    switch (direction) {
    case -1:
        dispatcher->OnNudgeDown();
        break;
    case 1:
        dispatcher->OnNudgeUp();
        break;
    }
}
