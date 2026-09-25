#include "GuideDialog.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Console/Trace.h"
#include <stdio.h>
#include <string.h>

#define GUIDE_WIDTH 26
#define GUIDE_HEIGHT 26
#define TEXT_Y 2
#define TEXT_ROWS 21
#define FOOTER_Y (GUIDE_HEIGHT - 2)

static std::vector<GuideDialog::Page> guidePages;
static bool guideLoaded = false;

// Parse bin:guide.txt once; it ships next to the binary
static void loadGuide() {
    if (guideLoaded)
        return;
    guideLoaded = true;
    Path path("bin:guide.txt");
    I_File *file = FileSystem::GetInstance()->Open(path.GetPath().c_str(), (char *)"r");
    if (!file) {
        Trace::Error("Guide file missing: %s", path.GetPath().c_str());
        return;
    }
    std::string text;
    char buffer[1024];
    int n;
    while ((n = file->Read(buffer, 1, sizeof(buffer))) > 0) {
        text.append(buffer, n);
    }
    file->Close();
    delete file;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty() || line[0] == '#')
            continue;
        if (line[0] == '@') {
            GuideDialog::Page page;
            size_t bar = line.find('|');
            page.id = line.substr(1, bar == std::string::npos ? std::string::npos : bar - 1);
            page.title = bar == std::string::npos ? page.id : line.substr(bar + 1);
            guidePages.push_back(page);
            continue;
        }
        if (guidePages.empty())
            continue;
        GuideDialog::Page &page = guidePages.back();
        if (line[0] == '=')
            page.sections.push_back(page.lines.size());
        page.lines.push_back(line);
    }
    Trace::Log("GUIDE", "loaded %d pages", (int)guidePages.size());
}

GuideDialog::GuideDialog(View &view, const char *page, const char *section)
    : ModalView(view), wantedPage_(page ? page : ""),
      wantedSection_(section ? section : ""), reading_(false), page_(0),
      listTop_(0), top_(0) {}

GuideDialog::~GuideDialog() {}

void GuideDialog::OnFocus() {
    loadGuide();
    reading_ = false;
    page_ = 0;
    for (int i = 0; i < (int)guidePages.size(); i++) {
        if (guidePages[i].id != wantedPage_)
            continue;
        int line = 0;
        const Page &p = guidePages[i];
        for (int s = 0; s < (int)p.sections.size(); s++) {
            if (p.lines[p.sections[s]].substr(1) == wantedSection_) {
                line = p.sections[s];
                break;
            }
        }
        openPage(i, line);
        break;
    }
}

void GuideDialog::openPage(int page, int line) {
    page_ = page;
    reading_ = true;
    scrollTo(line);
}

void GuideDialog::scrollTo(int line) {
    int count = (int)guidePages[page_].lines.size();
    int last = count - TEXT_ROWS;
    if (line > last)
        line = last;
    if (line < 0)
        line = 0;
    top_ = line;
}

// Index in sections_ of the heading at or above a line, -1 before the first
int GuideDialog::sectionAt(int line) {
    const Page &p = guidePages[page_];
    int found = -1;
    for (int s = 0; s < (int)p.sections.size(); s++) {
        if (p.sections[s] <= line)
            found = s;
    }
    return found;
}

void GuideDialog::DrawView() {
    SetWindow(GUIDE_WIDTH, GUIDE_HEIGHT);
    GUITextProperties props;
    char line[GUIDE_WIDTH + 8];

    if (guidePages.empty()) {
        SetColor(CD_HILITE1);
        DrawString(0, 0, "GUIDE", props);
        SetColor(CD_NORMAL);
        DrawString(0, TEXT_Y, "The guide file is missing.", props);
        DrawString(0, TEXT_Y + 1, "Reinstall the app.", props);
        SetColor(CD_MUTE);
        DrawString(0, FOOTER_Y + 1, "B close", props);
        SetColor(CD_NORMAL);
        return;
    }

    if (!reading_) {
        SetColor(CD_HILITE1);
        DrawString(0, 0, "GUIDE", props);
        SetColor(CD_MUTE);
        DrawString(7, 0, "pick a topic", props);
        int count = (int)guidePages.size();
        if (page_ < listTop_)
            listTop_ = page_;
        if (page_ >= listTop_ + TEXT_ROWS)
            listTop_ = page_ - TEXT_ROWS + 1;
        for (int i = 0; i < TEXT_ROWS && listTop_ + i < count; i++) {
            int index = listTop_ + i;
            bool on = (index == page_);
            snprintf(line, sizeof(line), "%-*s", GUIDE_WIDTH, guidePages[index].title.c_str());
            line[GUIDE_WIDTH] = 0;
            SetColor(on ? CD_HILITE2 : CD_NORMAL);
            props.invert_ = on;
            DrawString(0, TEXT_Y + i, line, props);
        }
        props.invert_ = false;
        SetColor(CD_MUTE);
        DrawString(0, FOOTER_Y, "Up/Dn pick   A open", props);
        DrawString(0, FOOTER_Y + 1, "B close", props);
        SetColor(CD_NORMAL);
        return;
    }

    const Page &p = guidePages[page_];
    int count = (int)p.lines.size();
    SetColor(CD_HILITE1);
    snprintf(line, sizeof(line), "%s", p.title.c_str());
    line[GUIDE_WIDTH - 5] = 0;
    DrawString(0, 0, line, props);
    int percent = count <= TEXT_ROWS ? 100 : (top_ * 100) / (count - TEXT_ROWS);
    snprintf(line, sizeof(line), "%3d%%", percent);
    SetColor(CD_MUTE);
    DrawString(GUIDE_WIDTH - 4, 0, line, props);

    for (int i = 0; i < TEXT_ROWS; i++) {
        int index = top_ + i;
        if (index >= count)
            break;
        const std::string &text = p.lines[index];
        switch (text[0]) {
        case '=':
            SetColor(CD_CURSOR);
            break;
        case '*':
            SetColor(CD_HILITE1);
            break;
        case '|':
            SetColor(CD_HILITE2);
            break;
        default:
            SetColor(CD_NORMAL);
            break;
        }
        snprintf(line, sizeof(line), "%s", text.c_str() + 1);
        line[GUIDE_WIDTH] = 0;
        DrawString(0, TEXT_Y + i, line, props);
    }

    SetColor(CD_MUTE);
    DrawString(0, FOOTER_Y, "Up/Dn scroll  L/R section", props);
    DrawString(0, FOOTER_Y + 1, "B topics  B+Up/Dn page", props);
    SetColor(CD_NORMAL);
}

void GuideDialog::OnPlayerUpdate(PlayerEventType, unsigned int currentTick) {}

void GuideDialog::CustomizeContextOverlay(
    const char *&name, const char *&where, const char *&edit,
    const char *&field, const char *&cmd1, const char *&cmd2,
    const char *&cmd3, const char *&cmd4, const char *&cmd5,
    const char *&cmd6, const char *&cmd7) {
    name = "GUIDE";
    where = "Topics > page";
    edit = "B back, START close";
    field = "The full user guide";
    cmd1 = "Up/Down pick/scroll";
    cmd2 = "A open / page down";
    cmd3 = "Left/Right section";
    cmd4 = "B+Up/Down page";
    cmd5 = "B back to topics";
    cmd6 = "START close guide";
    cmd7 = "RB+Select helper";
}

void GuideDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    // B tapped: back to the topics, or close from the topics. B+Up/Down
    // pages, as in every list.
    if (backTapped(mask, pressed)) {
        if (reading_ && !guidePages.empty()) {
            reading_ = false;
            isDirty_ = true;
        } else {
            EndModal(0);
        }
        return;
    }
    if (!pressed)
        return;
    if (mask == EPBM_START || guidePages.empty()) {
        if (mask == EPBM_START)
            EndModal(0);
        return;
    }

    if (!reading_) {
        int count = (int)guidePages.size();
        switch (mask) {
        case EPBM_UP:
            page_ = (page_ + count - 1) % count;
            break;
        case EPBM_DOWN:
            page_ = (page_ + 1) % count;
            break;
        case EPBM_A:
            openPage(page_, 0);
            break;
        default:
            return;
        }
        isDirty_ = true;
        return;
    }

    const Page &p = guidePages[page_];
    switch (mask) {
    case EPBM_UP:
        scrollTo(top_ - 1);
        break;
    case EPBM_DOWN:
        scrollTo(top_ + 1);
        break;
    case EPBM_B | EPBM_DOWN:
        scrollTo(top_ + TEXT_ROWS - 1);
        break;
    case EPBM_B | EPBM_UP:
        scrollTo(top_ - (TEXT_ROWS - 1));
        break;
    case EPBM_LEFT: {
        // Start of this section, or the previous one if already there
        int s = sectionAt(top_);
        if (s >= 0 && p.sections[s] == top_)
            s--;
        scrollTo(s >= 0 ? p.sections[s] : 0);
        break;
    }
    case EPBM_RIGHT: {
        // Next heading below the top line
        for (int s = 0; s < (int)p.sections.size(); s++) {
            if (p.sections[s] > top_) {
                scrollTo(p.sections[s]);
                break;
            }
        }
        break;
    }
    default:
        return;
    }
    isDirty_ = true;
}
