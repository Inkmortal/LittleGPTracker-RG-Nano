#include "NewProjectDialog.h"
#include "Application/Utils/RandomNames.h"
#include <string.h>

#define DIALOG_WIDTH 26
#define DIALOG_HEIGHT 18

#define GRID_ROWS 4
#define GRID_COLS 10
#define ACTION_ROW GRID_ROWS
#define GRID_X 3
#define GRID_Y 5
#define GRID_STEP 2 // blank line between letter rows

// Alphabetical, not QWERTY: on a D-pad the next letter is always one step
// away. ' ' marks an unused cell.
static const char *gridRows[GRID_ROWS] = {
    "ABCDEFGHIJ",
    "KLMNOPQRST",
    "UVWXYZ-_  ",
    "0123456789",
};

// B already erases, so the first button switches letter case instead
enum NameAction { NA_CASE = 0, NA_RANDOM, NA_OK, NA_CANCEL, NA_COUNT };
static const int actionX[NA_COUNT] = {1, 8, 16, 20};

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

NewProjectDialog::NewProjectDialog(View &view, Path currentPath)
    : ModalView(view), currentPath_(currentPath), cursor_(0), row_(0),
      col_(0), lower_(false), suggested_(false) {}

NewProjectDialog::~NewProjectDialog() {}

bool NewProjectDialog::nameTaken() {
    return !name_.empty() && currentPath_.Descend(GetName()).Exists();
}

void NewProjectDialog::DrawView() {

    SetWindow(DIALOG_WIDTH, DIALOG_HEIGHT);

    GUITextProperties props;
    char buffer[2] = {0, 0};

    SetColor(CD_HILITE1);
    DrawString((DIALOG_WIDTH - 8) / 2, 0, "NEW SONG", props);

    // Name slots: typed letters, cursor block, dots for free space
    SetColor(CD_NORMAL);
    DrawString(1, 2, "NAME", props);
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
        DrawString(6 + i, 2, buffer, props);
    }
    props.invert_ = false;

    const char *status = "";
    if (nameTaken()) {
        status = "name taken, change it";
    } else if (suggested_) {
        status = "type to replace";
    }
    SetColor(nameTaken() ? CD_CURSOR : CD_MUTE);
    DrawString(1, 3, "                        ", props);
    DrawString(6, 3, status, props);

    // Letter grid, one blank column between letters
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            buffer[0] = gridChar(row, col, lower_);
            if (buffer[0] == ' ')
                continue;
            bool on = (row == row_ && col == col_);
            SetColor(on ? CD_CURSOR : CD_NORMAL);
            props.invert_ = on;
            DrawString(GRID_X + col * 2, GRID_Y + row * GRID_STEP, buffer, props);
        }
    }

    for (int i = 0; i < NA_COUNT; i++) {
        bool on = (row_ == ACTION_ROW && col_ == i);
        SetColor(on ? CD_CURSOR : CD_HILITE1);
        props.invert_ = on;
        const char *text = (i == NA_CASE) ? (lower_ ? "ABC" : "abc")
                           : (i == NA_RANDOM) ? "RANDOM"
                           : (i == NA_OK) ? "OK" : "CANCEL";
        DrawString(actionX[i], GRID_Y + GRID_ROWS * GRID_STEP, text, props);
    }
    props.invert_ = false;

    SetColor(CD_MUTE);
    DrawString(1, DIALOG_HEIGHT - 3,
               name_.empty() ? "A type    B cancel" : "A type    B erase ", props);
    DrawString(1, DIALOG_HEIGHT - 2, "LB/RB move  START ok", props);
    SetColor(CD_NORMAL);
    View::EnableNotification();
}

void NewProjectDialog::OnPlayerUpdate(PlayerEventType,
                                      unsigned int currentTick) {};

void NewProjectDialog::OnFocus() {
    randomName();
    // Land on OK: a beginner can press A once and start making music
    row_ = ACTION_ROW;
    col_ = NA_OK;
};

void NewProjectDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
	name="NEW SONG";
	where="Name letters actions";
	edit="A types or runs";
	field="Name a new song";
	cmd1="Dpad pick a letter";
	cmd2="A type letter";
	cmd3="B erase, empty=exit";
	cmd4="LB/RB move in name";
	cmd5="abc/ABC switch case";
	cmd6="START or OK create";
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
    } while (currentPath_.Descend(GetName()).Exists());
    cursor_ = name_.size();
    suggested_ = true;
}

void NewProjectDialog::confirm() {
    if (name_.empty()) {
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
    case NA_OK:
        confirm();
        break;
    case NA_CANCEL:
        EndModal(0);
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
        int from = row_;
        row_ = (row_ + (mask == EPBM_UP ? ACTION_ROW : 1)) % (ACTION_ROW + 1);
        // Keep the column roughly under the same spot between grid and actions
        if (from == ACTION_ROW && row_ != ACTION_ROW) {
            col_ = (actionX[col_] - GRID_X + 1) / 2;
        } else if (from != ACTION_ROW && row_ == ACTION_ROW) {
            int x = GRID_X + col_ * 2;
            int best = 0;
            for (int i = 1; i < NA_COUNT; i++) {
                if (actionX[i] <= x)
                    best = i;
            }
            col_ = best;
        }
        if (col_ < 0)
            col_ = 0;
        if (col_ >= rowWidth(row_))
            col_ = rowWidth(row_) - 1;
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
