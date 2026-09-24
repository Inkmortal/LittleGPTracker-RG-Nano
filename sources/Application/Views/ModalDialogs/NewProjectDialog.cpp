#include "NewProjectDialog.h"
#include "Application/AppWindow.h"
#include "Application/Utils/RandomNames.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <stdlib.h>
#include <string.h>

// Layout in characters, relative to the dialog window
#define DIALOG_WIDTH 26
#define DIALOG_HEIGHT 19
#define NAME_X 2
#define NAME_Y 2
#define STATUS_Y 4
#define GRID_ROWS 4
#define GRID_COLS 10
#define ACTION_ROW GRID_ROWS
#define GRID_X 3
#define GRID_Y 6
#define KEY_STEP 2 // one key every 2 characters (16 px) both ways
#define ACTION_Y (GRID_Y + GRID_ROWS * KEY_STEP)
#define LEGEND_Y (ACTION_Y + 2)

// Alphabetical, not QWERTY: on a D-pad the next letter is always one step
// away. ' ' marks an unused cell.
static const char *gridRows[GRID_ROWS] = {
    "ABCDEFGHIJ",
    "KLMNOPQRST",
    "UVWXYZ-_  ",
    "0123456789",
};

enum NameAction { NA_CASE = 0, NA_RANDOM, NA_CANCEL, NA_DONE, NA_COUNT };
static const int actionX[NA_COUNT] = {1, 6, 14, 22};

static const char *actionText(int action, bool lower) {
    switch (action) {
    case NA_CASE:
        return lower ? "ABC" : "abc";
    case NA_RANDOM:
        return "RANDOM";
    case NA_CANCEL:
        return "CANCEL";
    default:
        return "DONE";
    }
}

static char gridChar(int row, int col, bool lower) {
    char c = gridRows[row][col];
    return (lower && c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static int rowWidth(int row) {
    if (row == ACTION_ROW)
        return NA_COUNT;
    int w = GRID_COLS;
    while (w > 0 && gridRows[row][w - 1] == ' ')
        w--;
    return w;
}

// Character column at the middle of a key
static int keyCenter(int row, int col) {
    if (row == ACTION_ROW)
        return actionX[col] + (int)strlen(actionText(col, false)) / 2;
    return GRID_X + col * KEY_STEP;
}

NewProjectDialog::NewProjectDialog(View &view, Path currentPath)
    : ModalView(view), currentPath_(currentPath), cursor_(0), row_(0),
      col_(0), lower_(false), suggested_(false), renaming_(false),
      title_("NEW SONG") {}

void NewProjectDialog::SetRename(const char *title, const std::string &current) {
    renaming_ = true;
    title_ = title;
    name_ = current.substr(0, MAX_NAME_LENGTH);
}

NewProjectDialog::~NewProjectDialog() {}

bool NewProjectDialog::nameTaken() {
    if (renaming_)
        return false;
    return !name_.empty() && currentPath_.Descend(GetName()).Exists();
}

void NewProjectDialog::DrawView() {

    SetWindow(DIALOG_WIDTH, DIALOG_HEIGHT);

    GUITextProperties props;
    char buffer[2] = {0, 0};

    SetColor(CD_HILITE1);
    DrawString((DIALOG_WIDTH - (int)strlen(title_)) / 2, 0, title_, props);

    SetColor(CD_NORMAL);
    DrawString(0, NAME_Y, "NAME", props);

    // Name field: typed letters, cursor block, dots for free space
    for (int i = 0; i < MAX_NAME_LENGTH; i++) {
        bool atCursor = (i == cursor_);
        if (i < (int)name_.size()) {
            buffer[0] = name_[i];
            SetColor(atCursor ? CD_CURSOR : (suggested_ ? CD_HILITE1 : CD_NORMAL));
        } else {
            buffer[0] = atCursor ? ' ' : '.';
            SetColor(atCursor ? CD_CURSOR : CD_MUTE);
        }
        props.invert_ = atCursor;
        DrawString(NAME_X + 3 + i, NAME_Y, buffer, props);
    }
    props.invert_ = false;

    const char *status = "";
    bool taken = nameTaken();
    if (taken) {
        status = "name taken, change it";
    } else if (suggested_) {
        status = "type to replace";
    }
    SetColor(taken ? CD_CURSOR : CD_MUTE);
    DrawString(1, STATUS_Y, "                        ", props);
    DrawString(NAME_X + 3, STATUS_Y, status, props);

    // Keys: letters sit on a 16 px grid; drawGraphics() adds the key caps
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            buffer[0] = gridChar(row, col, lower_);
            if (buffer[0] == ' ')
                continue;
            bool on = (row == row_ && col == col_);
            SetColor(on ? CD_CURSOR : CD_NORMAL);
            props.invert_ = on;
            DrawString(GRID_X + col * KEY_STEP, GRID_Y + row * KEY_STEP, buffer, props);
        }
    }
    props.invert_ = false;

    DrawString(0, ACTION_Y, "                          ", props);
    for (int i = 0; i < NA_COUNT; i++) {
        bool on = (row_ == ACTION_ROW && col_ == i);
        SetColor(on ? CD_CURSOR : CD_HILITE1);
        props.invert_ = on;
        DrawString(actionX[i], ACTION_Y, actionText(i, lower_), props);
    }
    props.invert_ = false;

    SetColor(CD_MUTE);
    DrawString(1, LEGEND_Y, name_.empty() ? "B back     SEL abc/ABC"
                                          : "B erase    SEL abc/ABC", props);
    DrawString(1, LEGEND_Y + 1, "LB/RB cursor  START done", props);
    SetColor(CD_NORMAL);
    View::EnableNotification();
}

#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
// Outline of a w x h px box, t px thick
static void drawBox(SDLGUIWindowImp *imp, GUIColor c, int x, int y, int w,
                    int h, int t) {
    imp->SetColor(c);
    GUIRect top(x, y, x + w, y + t), bottom(x, y + h - t, x + w, y + h);
    GUIRect left(x, y, x + t, y + h), right(x + w - t, y, x + w, y + h);
    imp->DrawRect(top);
    imp->DrawRect(bottom);
    imp->DrawRect(left);
    imp->DrawRect(right);
}
#endif

void NewProjectDialog::drawGraphics() {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    const int ox = windowLeft() * 8;
    const int oy = windowTop() * 8;
    GUIColor cap = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 70);
    GUIColor hot = AppWindow::ThemeColor(CD_CURSOR);
    GUIColor field = AppWindow::ThemeBlend(CD_BACKGROUND, CD_HILITE1, 55);

    // Name field frame
    drawBox(imp, field, ox + (NAME_X + 3) * 8 - 4, oy + NAME_Y * 8 - 4,
            MAX_NAME_LENGTH * 8 + 8, 16, 1);

    // Letter keys: 14 px caps with a 2 px gap. The selected cap is solid
    // amber around its letter cell, which is already drawn inverted in amber.
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            if (gridRows[row][col] == ' ')
                continue;
            bool on = (row == row_ && col == col_);
            int x = ox + (GRID_X + col * KEY_STEP) * 8 - 3;
            int y = oy + (GRID_Y + row * KEY_STEP) * 8 - 3;
            drawBox(imp, on ? hot : cap, x, y, 14, 14, on ? 3 : 1);
        }
    }

    for (int i = 0; i < NA_COUNT; i++) {
        bool on = (row_ == ACTION_ROW && col_ == i);
        int len = strlen(actionText(i, lower_));
        drawBox(imp, on ? hot : cap, ox + actionX[i] * 8 - 3,
                oy + ACTION_Y * 8 - 3, len * 8 + 6, 14, on ? 3 : 1);
    }
#endif
}

void NewProjectDialog::OnPlayerUpdate(PlayerEventType,
                                      unsigned int currentTick) {};

void NewProjectDialog::OnFocus() {
    if (renaming_) {
        cursor_ = name_.size();
        suggested_ = false;
    } else {
        randomName();
    }
    // Land on DONE: a beginner can press A once and start making music.
    // Down from here wraps straight to the first row of letters.
    row_ = ACTION_ROW;
    col_ = NA_DONE;
};

void NewProjectDialog::GetGuideTopic(const char *&page, const char *&section) {
    page = "controls";
    section = "Naming a new song";
}

void NewProjectDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
	name=title_;
	where="Name keys buttons";
	edit="A types or runs";
	field=renaming_?"Rename":"Name a new song";
	cmd1="Dpad pick a key";
	cmd2="A type the key";
	cmd3="B erase, empty=back";
	cmd4="LB/RB move in name";
	cmd5="SELECT abc/ABC";
	cmd6="START or DONE create";
	cmd7="RANDOM new name";
}

void NewProjectDialog::typeChar(char c) {
    if (suggested_) {
        name_.clear();
        cursor_ = 0;
        suggested_ = false;
    }
    if ((int)name_.size() >= MAX_NAME_LENGTH) {
        View::SetNotification("Name is full", -6);
        return;
    }
    name_.insert(name_.begin() + cursor_, c);
    cursor_++;
}

void NewProjectDialog::erase() {
    if (suggested_) {
        name_.clear();
        cursor_ = 0;
        suggested_ = false;
        return;
    }
    if (cursor_ > 0) {
        name_.erase(cursor_ - 1, 1);
        cursor_--;
    }
}

void NewProjectDialog::randomName() {
    do {
        name_ = getRandomName();
    } while (!renaming_ && currentPath_.Descend(GetName()).Exists());
    cursor_ = name_.size();
    suggested_ = true;
}

void NewProjectDialog::confirm() {
    // An empty rename is allowed: it brings back the automatic name
    if (name_.empty() && !renaming_) {
        View::SetNotification("Type a name first", -6);
    } else if (nameTaken()) {
        View::SetNotification("Name taken", -6);
    } else {
        EndModal(1);
    }
}

void NewProjectDialog::activate() {
    if (row_ < ACTION_ROW) {
        typeChar(gridChar(row_, col_, lower_));
        return;
    }
    switch (col_) {
    case NA_CASE:
        lower_ = !lower_;
        break;
    case NA_RANDOM:
        randomName();
        break;
    case NA_CANCEL:
        EndModal(0);
        break;
    case NA_DONE:
        confirm();
        break;
    }
}

void NewProjectDialog::ProcessButtonMask(unsigned short mask, bool pressed) {

    if (!pressed)
        return;

    switch (mask) {
    case EPBM_A:
        activate();
        break;
    case EPBM_B:
        if (name_.empty()) {
            EndModal(0);
            return;
        }
        erase();
        break;
    case EPBM_SELECT:
        lower_ = !lower_;
        break;
    case EPBM_START:
        confirm();
        break;
    case EPBM_L:
        if (cursor_ > 0) {
            cursor_--;
            suggested_ = false;
        }
        break;
    case EPBM_R:
        if (cursor_ < (int)name_.size() && cursor_ < MAX_NAME_LENGTH - 1) {
            cursor_++;
            suggested_ = false;
        }
        break;
    case EPBM_UP:
    case EPBM_DOWN: {
        // Land on the key nearest to the one we left
        int x = keyCenter(row_, col_);
        row_ = (row_ + (mask == EPBM_UP ? ACTION_ROW : 1)) % (ACTION_ROW + 1);
        int best = 0;
        for (int i = 1; i < rowWidth(row_); i++) {
            if (abs(keyCenter(row_, i) - x) < abs(keyCenter(row_, best) - x))
                best = i;
        }
        col_ = best;
        break;
    }
    case EPBM_LEFT:
        col_ = (col_ + rowWidth(row_) - 1) % rowWidth(row_);
        break;
    case EPBM_RIGHT:
        col_ = (col_ + 1) % rowWidth(row_);
        break;
    default:
        return;
    }
    isDirty_ = true;
};

std::string NewProjectDialog::GetName() {
    return "lgpt_" + name_;
}
