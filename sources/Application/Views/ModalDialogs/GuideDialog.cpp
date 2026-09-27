#include "GuideDialog.h"
#include "Application/AppWindow.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Console/Trace.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#define GUIDE_PIXELS
#endif
#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pixel layout of the 240x240 screen (8x8 font)
#define SCREEN 240
#define CHARS 28             // text columns, as the tool wraps them
#define TEXT_X 5
#define TIP_X 9              // tips sit right of their bar
#define TITLE_Y 4
#define RULE_Y 14
#define BODY_TOP 18
#define BODY_BOTTOM 217      // first pixel row below the text
#define FOOTER_Y 222
#define SCROLL_X 235
#define LINE 10              // body text pitch: 8 px letters, 2 px air
#define GAP 5                // a paragraph break: half a line
#define CODE_LINE 9          // examples sit tight, like the tracker
#define BOX_PAD 3
#define ROW 11               // list rows (contents, index, go to)
#define LIST_ROWS ((BODY_BOTTOM - BODY_TOP) / ROW)

static std::vector<GuideDialog::Page> guidePages;
static std::vector<GuideDialog::IndexEntry> guideIndex;
static bool guideLoaded = false;

static bool lessLabel(const GuideDialog::IndexEntry &a, const GuideDialog::IndexEntry &b) {
    const char *x = a.label.c_str(), *y = b.label.c_str();
    while (*x && tolower(*x) == tolower(*y)) {
        x++;
        y++;
    }
    if (tolower(*x) != tolower(*y))
        return tolower(*x) < tolower(*y);
    return a.page < b.page;
}

static std::string trimmed(const std::string &s) {
    size_t a = s.find_first_not_of(' ');
    if (a == std::string::npos)
        return "";
    size_t b = s.find_last_not_of(' ');
    return s.substr(a, b - a + 1);
}

// How a heading or table key reads in the index: "3. The kick" -> "The
// kick", a key with its value "VOLM  aabb" -> "VOLM"; "" leaves it out
static std::string indexLabel(const std::string &line) {
    std::string label = trimmed(line.substr(1));
    if (line[0] == '*') {
        size_t wide = label.find("  ");
        if (wide != std::string::npos)
            label = label.substr(0, wide);
        // A value (00, 0047, 1-3, C0, 0x40) is not a term to look up: a
        // term has two letters in a row somewhere
        bool word = false;
        for (size_t i = 0; i + 1 < label.size() && !word; i++)
            word = isalpha(label[i]) && isalpha(label[i + 1]);
        if (!word || !isalnum(label[0]))
            return "";
        return label;
    }
    size_t k = 0;
    while (k < label.size() && isdigit(label[k]))
        k++;
    if (k > 0 && k + 1 < label.size() && label[k] == '.' && label[k + 1] == ' ')
        label = label.substr(k + 2);
    return label;
}

// A heading's words wrapped to the text width
static std::vector<std::string> wrapWords(const std::string &text, int width) {
    std::vector<std::string> out;
    std::string line;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find(' ', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string word = text.substr(pos, end - pos);
        pos = end + 1;
        if (word.empty())
            continue;
        while ((int)word.size() > width) {  // longer than a line: split it
            if (!line.empty()) {
                out.push_back(line);
                line = "";
            }
            out.push_back(word.substr(0, width));
            word = word.substr(width);
        }
        if (line.empty())
            line = word;
        else if ((int)(line.size() + 1 + word.size()) <= width)
            line += " " + word;
        else {
            out.push_back(line);
            line = word;
        }
    }
    if (!line.empty() || out.empty())
        out.push_back(line);
    return out;
}

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
        if (line[0] == '~') {
            // ~col,len,page,section: a link on the line above
            GuideDialog::Link link;
            link.line = (int)page.lines.size() - 1;
            char target[64] = "";
            int used = 0;
            if (link.line < 0 ||
                sscanf(line.c_str() + 1, "%d,%d,%63[^,]%n", &link.col, &link.len, target, &used) < 3) {
                Trace::Error("GUIDE bad link line: %s", line.c_str());
                continue;
            }
            link.page = target;
            const char *rest = line.c_str() + 1 + used;
            link.section = (*rest == ',') ? rest + 1 : "";
            page.links.push_back(link);
            continue;
        }
        if (line[0] == '=' || line[0] == '+')
            page.sections.push_back(page.lines.size());
        page.lines.push_back(line);
    }

    // The index: every heading, command and key combo (table keys that are
    // just values, like 00 or 1, aren't worth looking up)
    for (int p = 0; p < (int)guidePages.size(); p++) {
        const GuideDialog::Page &page = guidePages[p];
        for (int l = 0; l < (int)page.lines.size(); l++) {
            char kind = page.lines[l][0];
            if (kind != '=' && kind != '+' && kind != '*')
                continue;
            GuideDialog::IndexEntry entry;
            entry.label = indexLabel(page.lines[l]);
            entry.page = p;
            entry.line = l;
            if (entry.label.empty())
                continue;
            bool seen = false;
            for (int i = 0; i < (int)guideIndex.size() && !seen; i++)
                seen = guideIndex[i].page == p && guideIndex[i].label == entry.label;
            if (!seen)
                guideIndex.push_back(entry);
        }
    }
    std::stable_sort(guideIndex.begin(), guideIndex.end(), lessLabel);

    // Nothing may run off the screen (the tool wraps; an old or hand-made
    // guide.txt shows here)
    for (int p = 0; p < (int)guidePages.size(); p++) {
        const GuideDialog::Page &page = guidePages[p];
        for (int l = 0; l < (int)page.lines.size(); l++) {
            char kind = page.lines[l][0];
            if (kind != '=' && kind != '+' && (int)page.lines[l].size() - 1 > CHARS) {
                Trace::Error("GUIDE line too wide in %s: %s", page.id.c_str(),
                             page.lines[l].c_str());
            }
        }
    }

    // Every link must land somewhere: a renamed heading shows up here
    for (int p = 0; p < (int)guidePages.size(); p++) {
        for (int k = 0; k < (int)guidePages[p].links.size(); k++) {
            const GuideDialog::Link &link = guidePages[p].links[k];
            bool found = false;
            for (int q = 0; q < (int)guidePages.size() && !found; q++) {
                if (guidePages[q].id != link.page)
                    continue;
                found = link.section.empty();
                for (int s = 0; s < (int)guidePages[q].sections.size() && !found; s++) {
                    found = guidePages[q].lines[guidePages[q].sections[s]].substr(1) == link.section;
                }
            }
            if (!found) {
                Trace::Error("GUIDE link to nowhere: %s -> %s / %s", guidePages[p].id.c_str(),
                             link.page.c_str(), link.section.c_str());
            }
        }
    }
    Trace::Log("GUIDE", "loaded %d pages, %d index entries", (int)guidePages.size(),
               (int)guideIndex.size());
}

GuideDialog::GuideDialog(View &view, const char *page, const char *section)
    : ModalView(view), wantedPage_(page ? page : ""),
      wantedSection_(section ? section : ""), mode_(MODE_CONTENTS), page_(0),
      top_(0), selection_(0), listTop_(0), expanded_(-1), goTo_(false),
      goToSelection_(0) {}

GuideDialog::~GuideDialog() {}

void GuideDialog::OnFocus() {
    loadGuide();
    mode_ = MODE_CONTENTS;
    page_ = 0;
    history_.clear();
    buildRows();
    for (int i = 0; i < (int)guidePages.size(); i++) {
        if (guidePages[i].id != wantedPage_)
            continue;
        int line = findSection(i, wantedSection_);
        // (a screen pointing at a heading that no longer exists shows here)
        Trace::Log("GUIDE", "open %s / %s", wantedPage_.c_str(), wantedSection_.c_str());
        if (line < 0) {
            Trace::Error("GUIDE section missing: %s / %s", wantedPage_.c_str(),
                         wantedSection_.c_str());
            line = 0;
        }
        openPage(i, line);
        break;
    }
}

int GuideDialog::findSection(int page, const std::string &section) {
    if (section.empty())
        return 0;
    const Page &p = guidePages[page];
    for (int s = 0; s < (int)p.sections.size(); s++) {
        if (p.lines[p.sections[s]].substr(1) == section)
            return p.sections[s];
    }
    return -1;
}

// ---------------------------------------------------------------- layout

int GuideDialog::lineHeight(const Page &p, int index) const {
    const std::string &s = p.lines[index];
    switch (s[0]) {
    case '=':
        return (index > 0 ? 6 : 0) + LINE * (int)wrapWords(s.substr(1), CHARS).size() + 4;
    case '+':
        return (index > 0 ? 4 : 0) + LINE * (int)wrapWords(s.substr(1), CHARS).size() + 1;
    case '|':
    case '%': {
        int h = CODE_LINE;
        bool first = index == 0 || (p.lines[index - 1][0] != '|' && p.lines[index - 1][0] != '%');
        bool last = index + 1 >= (int)p.lines.size() ||
                    (p.lines[index + 1][0] != '|' && p.lines[index + 1][0] != '%');
        if (first)
            h += BOX_PAD;
        if (last)
            h += BOX_PAD;
        return h;
    }
    case ' ':
        return s.size() <= 1 ? GAP : LINE;
    default:
        return LINE;
    }
}

void GuideDialog::layoutPage() {
    const Page &p = guidePages[page_];
    lineY_.assign(p.lines.size() + 1, 0);
    for (int i = 0; i < (int)p.lines.size(); i++) {
        lineY_[i + 1] = lineY_[i] + lineHeight(p, i);
    }
}

// First line that doesn't fit on the screen from top_
int GuideDialog::visibleEnd() const {
    int count = (int)lineY_.size() - 1;
    int limit = lineY_[top_] + (BODY_BOTTOM - BODY_TOP);
    int end = top_;
    while (end < count && lineY_[end + 1] <= limit)
        end++;
    return end;
}

void GuideDialog::scrollTo(int line) {
    int count = (int)lineY_.size() - 1;
    // Lowest top that still fills the screen
    int last = count;
    while (last > 0 && lineY_[count] - lineY_[last - 1] <= BODY_BOTTOM - BODY_TOP)
        last--;
    if (line > last)
        line = last;
    if (line < 0)
        line = 0;
    top_ = line;
}

// Index in sections of the heading at or above a line, -1 before the first
int GuideDialog::sectionAt(int line) {
    const Page &p = guidePages[page_];
    int found = -1;
    for (int s = 0; s < (int)p.sections.size(); s++) {
        if (p.sections[s] <= line)
            found = s;
    }
    return found;
}

// ------------------------------------------------------------ navigation

void GuideDialog::openPage(int page, int line) {
    page_ = page;
    mode_ = MODE_PAGE;
    goTo_ = false;
    layoutPage();
    scrollTo(line);
    isDirty_ = true;
}

void GuideDialog::go(Mode mode, int page, int line) {
    Place here = {mode_, page_, top_, selection_};
    history_.push_back(here);
    if (mode == MODE_PAGE) {
        Trace::Log("GUIDE", "go %s / %s", guidePages[page].id.c_str(),
                   line > 0 ? guidePages[page].lines[line].substr(1).c_str() : "");
        openPage(page, line);
        return;
    }
    goTo_ = false;
    mode_ = mode;
    selection_ = 0;
    listTop_ = 0;
    if (mode == MODE_CONTENTS) {
        Trace::Log("GUIDE", "contents");
        expanded_ = -1;
        buildRows();
    } else {
        Trace::Log("GUIDE", "index");
        if (page >= 0) {  // the index at this page's first entry
            for (int i = 0; i < (int)guideIndex.size(); i++) {
                if (guideIndex[i].page == page) {
                    selection_ = i;
                    break;
                }
            }
        }
    }
    isDirty_ = true;
}

// B: one step back; from a page nobody led to, the contents; from the
// contents themselves, close
void GuideDialog::back() {
    goTo_ = false;
    if (!history_.empty()) {
        Place place = history_.back();
        history_.pop_back();
        Trace::Log("GUIDE", "back");
        if (place.mode == MODE_PAGE) {
            openPage(place.page, place.top);
        } else {
            mode_ = place.mode;
            page_ = place.page;
            selection_ = place.selection;
            if (mode_ == MODE_CONTENTS)
                buildRows();
        }
        isDirty_ = true;
        return;
    }
    if (mode_ == MODE_PAGE) {
        Trace::Log("GUIDE", "contents");
        mode_ = MODE_CONTENTS;
        expanded_ = -1;
        buildRows();
        for (int r = 0; r < (int)rows_.size(); r++) {
            if (rows_[r].page == page_ && rows_[r].line < 0)
                selection_ = r;
        }
        isDirty_ = true;
        return;
    }
    EndModal(0);
}

void GuideDialog::buildRows() {
    rows_.clear();
    for (int p = 0; p < (int)guidePages.size(); p++) {
        Row row = {p, -1, guidePages[p].title};
        rows_.push_back(row);
        if (p != expanded_)
            continue;
        const Page &page = guidePages[p];
        for (int s = 0; s < (int)page.sections.size(); s++) {
            const std::string &heading = page.lines[page.sections[s]];
            if (heading[0] != '=')
                continue;  // the contents list main sections only
            Row section = {p, page.sections[s], heading.substr(1)};
            rows_.push_back(section);
        }
    }
    Row index = {-1, -1, "Index: keys, commands"};
    rows_.push_back(index);
    if (selection_ >= (int)rows_.size())
        selection_ = (int)rows_.size() - 1;
}

// Go to: the links on screen, then the contents and the index
void GuideDialog::openGoTo() {
    targets_.clear();
    const Page &p = guidePages[page_];
    int end = visibleEnd();
    for (int k = 0; k < (int)p.links.size(); k++) {
        const Link &link = p.links[k];
        if (link.line < top_ || link.line >= end)
            continue;
        for (int q = 0; q < (int)guidePages.size(); q++) {
            if (guidePages[q].id != link.page)
                continue;
            Target t;
            t.label = p.lines[link.line].substr(1 + link.col, link.len);
            // A link that wrapped onto the next line: one choice, whole label
            if (k + 1 < (int)p.links.size() && p.links[k + 1].line == link.line + 1 &&
                p.links[k + 1].col == 0 && p.links[k + 1].page == link.page &&
                p.links[k + 1].section == link.section && link.col + link.len >= (int)p.lines[link.line].size() - 1) {
                t.label += " " + p.lines[link.line + 1].substr(1, p.links[k + 1].len);
                k++;
            }
            t.page = q;
            t.line = std::max(0, findSection(q, link.section));
            bool seen = false;
            for (int i = 0; i < (int)targets_.size(); i++) {
                seen = seen || (targets_[i].page == t.page && targets_[i].line == t.line);
            }
            if (!seen)
                targets_.push_back(t);
        }
    }
    // Then every other page this one points to
    for (int k = 0; k < (int)p.links.size() && targets_.size() < 10; k++) {
        for (int q = 0; q < (int)guidePages.size(); q++) {
            if (guidePages[q].id != p.links[k].page || q == page_)
                continue;
            bool seen = false;
            for (int i = 0; i < (int)targets_.size(); i++)
                seen = seen || targets_[i].page == q;
            if (!seen) {
                Target t = {"See also: " + guidePages[q].title, q, 0};
                targets_.push_back(t);
            }
        }
    }
    Target contents = {"Contents", -1, 0};
    Target index = {"Index: keys, commands", -2, 0};
    targets_.push_back(contents);
    targets_.push_back(index);
    goTo_ = true;
    goToSelection_ = 0;
    isDirty_ = true;
}

void GuideDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    // RB+Select closes from anywhere: it's what opened the guide
    if (mask == (EPBM_R | EPBM_SELECT) || mask == EPBM_START) {
        Trace::Log("GUIDE", "close");
        EndModal(0);
        return;
    }
    if (mask == EPBM_B) {
        if (goTo_) {
            goTo_ = false;
            isDirty_ = true;
        } else {
            back();
        }
        return;
    }
    if (guidePages.empty())
        return;

    if (goTo_) {
        int count = (int)targets_.size();
        switch (mask) {
        case EPBM_UP:
            goToSelection_ = (goToSelection_ + count - 1) % count;
            break;
        case EPBM_DOWN:
            goToSelection_ = (goToSelection_ + 1) % count;
            break;
        case EPBM_A: {
            Target t = targets_[goToSelection_];
            goTo_ = false;
            if (t.page == -1)
                go(MODE_CONTENTS, page_, 0);
            else if (t.page == -2)
                go(MODE_INDEX, page_, 0);
            else
                go(MODE_PAGE, t.page, t.line);
            break;
        }
        default:
            return;
        }
        isDirty_ = true;
        return;
    }

    switch (mode_) {
    case MODE_CONTENTS: {
        int count = (int)rows_.size();
        const Row row = rows_[selection_];
        switch (mask) {
        case EPBM_UP:
            selection_ = (selection_ + count - 1) % count;
            break;
        case EPBM_DOWN:
            selection_ = (selection_ + 1) % count;
            break;
        case EPBM_L | EPBM_UP:
            selection_ = std::max(0, selection_ - (LIST_ROWS - 1));
            break;
        case EPBM_L | EPBM_DOWN:
            selection_ = std::min(count - 1, selection_ + (LIST_ROWS - 1));
            break;
        case EPBM_RIGHT:
            // Show this topic's sections (again: step into them)
            if (row.page >= 0 && row.line < 0) {
                if (expanded_ == row.page) {
                    if (selection_ + 1 < count && rows_[selection_ + 1].page == row.page)
                        selection_++;
                } else {
                    expanded_ = row.page;
                    buildRows();
                    for (int r = 0; r < (int)rows_.size(); r++) {
                        if (rows_[r].page == row.page && rows_[r].line < 0)
                            selection_ = r;
                    }
                }
            }
            break;
        case EPBM_LEFT:
            // Fold the sections away, back on their topic
            if (expanded_ >= 0) {
                int topic = expanded_;
                expanded_ = -1;
                buildRows();
                if (row.page == topic || row.page == -1) {
                    for (int r = 0; r < (int)rows_.size(); r++) {
                        if (rows_[r].page == (row.page == -1 ? -1 : topic) && rows_[r].line < 0)
                            selection_ = r;
                    }
                } else {
                    for (int r = 0; r < (int)rows_.size(); r++) {
                        if (rows_[r].page == row.page && rows_[r].line < 0)
                            selection_ = r;
                    }
                }
            }
            break;
        case EPBM_A:
            if (row.page < 0)
                go(MODE_INDEX, -1, 0);
            else
                go(MODE_PAGE, row.page, row.line < 0 ? 0 : row.line);
            return;
        default:
            return;
        }
        isDirty_ = true;
        return;
    }
    case MODE_INDEX: {
        int count = (int)guideIndex.size();
        if (count == 0)
            return;
        switch (mask) {
        case EPBM_UP:
            selection_ = (selection_ + count - 1) % count;
            break;
        case EPBM_DOWN:
            selection_ = (selection_ + 1) % count;
            break;
        case EPBM_L | EPBM_UP:
            selection_ = std::max(0, selection_ - (LIST_ROWS - 1));
            break;
        case EPBM_L | EPBM_DOWN:
            selection_ = std::min(count - 1, selection_ + (LIST_ROWS - 1));
            break;
        case EPBM_LEFT:
        case EPBM_RIGHT: {
            // The previous / next first letter
            int letter = toupper(guideIndex[selection_].label[0]);
            int i = selection_;
            if (mask == EPBM_RIGHT) {
                while (i < count && toupper(guideIndex[i].label[0]) == letter)
                    i++;
                selection_ = i < count ? i : 0;
            } else {
                while (i > 0 && toupper(guideIndex[i - 1].label[0]) == letter)
                    i--;
                if (i > 0) {  // start of the letter before
                    i--;
                    int before = toupper(guideIndex[i].label[0]);
                    while (i > 0 && toupper(guideIndex[i - 1].label[0]) == before)
                        i--;
                }
                selection_ = i;
            }
            break;
        }
        case EPBM_A:
            go(MODE_PAGE, guideIndex[selection_].page, guideIndex[selection_].line);
            return;
        default:
            return;
        }
        isDirty_ = true;
        return;
    }
    case MODE_PAGE: {
        const Page &p = guidePages[page_];
        switch (mask) {
        case EPBM_UP:
            scrollTo(top_ - 1);
            break;
        case EPBM_DOWN:
            scrollTo(top_ + 1);
            break;
        // LB+Up/Down: a screen, the bigger move as everywhere
        case EPBM_L | EPBM_DOWN:
            scrollTo(visibleEnd());
            break;
        case EPBM_L | EPBM_UP: {
            int line = top_;
            while (line > 0 && lineY_[top_] - lineY_[line - 1] <= BODY_BOTTOM - BODY_TOP)
                line--;
            scrollTo(line);
            break;
        }
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
        // LB+Left/Right: the previous / next page of the guide
        case EPBM_L | EPBM_LEFT:
        case EPBM_L | EPBM_RIGHT: {
            int count = (int)guidePages.size();
            int next = (page_ + (mask & EPBM_RIGHT ? 1 : count - 1)) % count;
            go(MODE_PAGE, next, 0);
            return;
        }
        case EPBM_A:
            openGoTo();
            return;
        default:
            return;
        }
        isDirty_ = true;
        return;
    }
    }
}

// --------------------------------------------------------------- drawing

void GuideDialog::DrawView() {
    // The whole screen is the guide: clear the characters underneath, then
    // paint (pixels on the RG Nano, characters elsewhere)
    View::ClearRect(0, 0, 40, 30);
#ifndef GUIDE_PIXELS
    paint();
#endif
}

void GuideDialog::drawGraphics() {
#ifdef GUIDE_PIXELS
    // Every flush: nothing drawn underneath (a meter, a notification) can
    // stay on top of the guide
    paint();
#endif
}

void GuideDialog::OnPlayerUpdate(PlayerEventType, unsigned int currentTick) {}

void GuideDialog::fill(int x, int y, int w, int h, int color, int blendTo, int percent) {
#ifdef GUIDE_PIXELS
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    GUIColor c = blendTo < 0 ? AppWindow::ThemeColor((ColorDefinition)color)
                             : AppWindow::ThemeBlend((ColorDefinition)color,
                                                     (ColorDefinition)blendTo, percent);
    imp->SetColor(c);
    GUIRect r(x, y, x + w, y + h);
    imp->DrawRect(r);
#endif
}

void GuideDialog::text(int x, int y, const char *s, int color, bool note) {
#ifdef GUIDE_PIXELS
    SDLGUIWindowImp *imp = (SDLGUIWindowImp *)w_.GetImpWindow();
    GUIColor c = AppWindow::ThemeColor((ColorDefinition)color);
    imp->SetColor(c);
    GUIPoint p(x, y);
    imp->DrawTransparentString(s, p);
#ifdef PLATFORM_RGNANO_SIM
    if (note)
        ((AppWindow &)w_).NotePixelText(s);
#endif
#else
    GUITextProperties props;
    SetColor((ColorDefinition)color);
    View::DrawString(x / 8, y / 8, s, props);
#endif
}

// Key hints: {A} is drawn as a key, the rest quietly
void GuideDialog::hint(int x, int y, const char *s) {
    char part[40];
#ifdef PLATFORM_RGNANO_SIM
    // The whole line, as it reads, for the simulator's screen checks
    std::string plain;
    for (const char *c = s; *c; c++) {
        if (*c != '{' && *c != '}')
            plain += *c;
    }
    ((AppWindow &)w_).NotePixelText(plain.c_str());
#endif
    while (*s) {
        bool key = (*s == '{');
        const char *end = strchr(s + 1, key ? '}' : '{');
        int n = end ? (int)(end - s) - (key ? 1 : 0) : (int)strlen(s);
        if (n > (int)sizeof(part) - 1)
            n = sizeof(part) - 1;
        memcpy(part, s + (key ? 1 : 0), n);
        part[n] = 0;
        text(x, y, part, key ? CD_HILITE1 : CD_MUTE, false);
        x += 8 * n;
        s += n + (key ? 2 : 0);
        if (key && !end)
            break;
    }
}

void GuideDialog::paintTitle(const char *left, const char *right) {
    char line[64];
    int room = CHARS - (int)strlen(right) - (right[0] ? 2 : 0);
    snprintf(line, sizeof(line), "%s", left);
    if ((int)strlen(line) > room) {
        line[room - 2] = '.';
        line[room - 1] = '.';
        line[room] = 0;
    }
    text(TEXT_X, TITLE_Y, line, CD_HILITE1);
    text(TEXT_X + 8 * (CHARS - (int)strlen(right)), TITLE_Y, right, CD_MUTE);
    fill(0, RULE_Y, SCREEN, 1, CD_BACKGROUND, CD_BORDER, 70);
}

void GuideDialog::paintFooter(const char *line1, const char *line2) {
    fill(0, BODY_BOTTOM + 1, SCREEN, 1, CD_BACKGROUND, CD_BORDER, 70);
    hint(TEXT_X, FOOTER_Y, line1);
    hint(TEXT_X, FOOTER_Y + 9, line2);
}

void GuideDialog::paint() {
    fill(0, 0, SCREEN, SCREEN, CD_BACKGROUND);
    if (guidePages.empty()) {
        paintTitle("GUIDE", "");
        text(TEXT_X, BODY_TOP, "The guide file is missing.", CD_NORMAL);
        text(TEXT_X, BODY_TOP + LINE, "Reinstall the app.", CD_NORMAL);
        paintFooter("{B} close", "");
        return;
    }
    switch (mode_) {
    case MODE_CONTENTS:
        paintContents();
        break;
    case MODE_INDEX:
        paintIndex();
        break;
    case MODE_PAGE:
        paintPage();
        break;
    }
    if (goTo_)
        paintGoTo();
}

// A scrolling list with a selection bar; notes are right-aligned, muted
void GuideDialog::paintList(const std::vector<std::string> &labels,
                            const std::vector<std::string> &notes,
                            const std::vector<int> &indents, int selection, int &top) {
    int count = (int)labels.size();
    if (selection < top)
        top = selection;
    if (selection >= top + LIST_ROWS)
        top = selection - LIST_ROWS + 1;
    char line[48];
    for (int i = 0; i < LIST_ROWS && top + i < count; i++) {
        int index = top + i;
        int y = BODY_TOP + i * ROW;
        bool on = index == selection;
        if (on)
            fill(2, y - 2, SCROLL_X - 4, ROW, CD_CURSOR);
        int indent = indents.empty() ? 0 : indents[index];
        int room = CHARS - indent - (notes.empty() ? 0 : (int)notes[index].size() + 1);
        snprintf(line, sizeof(line), "%s", labels[index].c_str());
        if ((int)strlen(line) > room && room > 1) {
            line[room - 1] = '.';
            line[room] = 0;
        }
        text(TEXT_X + 8 * indent, y, line, on ? CD_BACKGROUND : (indent ? CD_NORMAL : CD_HILITE2));
#ifdef PLATFORM_RGNANO_SIM
        if (on)
            ((AppWindow &)w_).NotePixelText(labels[index].c_str(), true);
#endif
        if (!notes.empty() && !notes[index].empty()) {
            text(TEXT_X + 8 * (CHARS - (int)notes[index].size()), y, notes[index].c_str(),
                 on ? CD_BACKGROUND : CD_MUTE);
        }
    }
    // Where the list is
    if (count > LIST_ROWS) {
        int track = BODY_BOTTOM - BODY_TOP - 2;
        int thumb = std::max(6, track * LIST_ROWS / count);
        int y = BODY_TOP + (track - thumb) * top / (count - LIST_ROWS);
        fill(SCROLL_X, BODY_TOP, 3, track, CD_BACKGROUND, CD_BORDER, 35);
        fill(SCROLL_X, y, 3, thumb, CD_MUTE);
    }
}

void GuideDialog::paintContents() {
    char right[16];
    snprintf(right, sizeof(right), "%d topics", (int)guidePages.size());
    paintTitle("GUIDE  Contents", right);
    std::vector<std::string> labels, notes;
    std::vector<int> indents;
    for (int r = 0; r < (int)rows_.size(); r++) {
        const Row &row = rows_[r];
        labels.push_back(row.label);
        indents.push_back(row.line >= 0 ? 2 : 0);
        std::string note;
        if (row.page >= 0 && row.line < 0) {
            int sections = 0;
            const Page &p = guidePages[row.page];
            for (int s = 0; s < (int)p.sections.size(); s++)
                sections += p.lines[p.sections[s]][0] == '=';
            if (sections > 0)
                note = row.page == expanded_ ? "-" : ">";
        }
        notes.push_back(note);
    }
    paintList(labels, notes, indents, selection_, listTop_);
    const Row &row = rows_[selection_];
    if (row.page >= 0 && row.line < 0)
        paintFooter("{A} read  {>} sections  {<} fold", "{B} close  {RB+Sel} close");
    else
        paintFooter("{A} read  {<} fold  {Up/Dn} pick", "{B} close  {RB+Sel} close");
}

void GuideDialog::paintIndex() {
    char right[16];
    snprintf(right, sizeof(right), "%d/%d", selection_ + 1, (int)guideIndex.size());
    paintTitle("GUIDE  Index", right);
    std::vector<std::string> labels, notes;
    std::vector<int> indents;
    for (int i = 0; i < (int)guideIndex.size(); i++) {
        labels.push_back(guideIndex[i].label);
        notes.push_back(guidePages[guideIndex[i].page].id);
    }
    paintList(labels, notes, indents, selection_, listTop_);
    paintFooter("{A} read  {L/R} letter", "{B} back  {RB+Sel} close");
}

// One tracker example line, colored like the screens: rows and empty cells
// dim, notes bright, commands and hex values in their highlight colors
void GuideDialog::paintExample(const char *s, int x, int y) {
    int n = (int)strlen(s);
    char part[40];
    int i = 0;
    while (i < n) {
        if (s[i] == ' ') {
            i++;
            continue;
        }
        int start = i;
        int color = CD_MUTE;
        // A note: C-4, D#3, C 3
        if (s[i] >= 'A' && s[i] <= 'G' && i + 2 < n &&
            (s[i + 1] == ' ' || s[i + 1] == '#' || s[i + 1] == '-') && isdigit(s[i + 2]) &&
            (i + 3 >= n || s[i + 3] == ' ')) {
            i += 3;
            color = CD_NORMAL;
        } else {
            while (i < n && s[i] != ' ')
                i++;
            int len = i - start;
            bool hex = true, letters = true, dashes = true;
            for (int k = start; k < i; k++) {
                hex = hex && isxdigit(s[k]) && !islower(s[k]);
                letters = letters && isupper(s[k]);
                dashes = dashes && s[k] == '-';
            }
            if (dashes)
                color = CD_MUTE;
            else if (start == 0 && len == 2 && hex)
                color = CD_MUTE;  // the row number
            else if (letters && len == 4)
                color = CD_HILITE1;  // a command
            else if (hex)
                color = CD_HILITE2;
            else if (s[start] == '(' || s[start] == '<')
                color = CD_MUTE;
            else
                color = CD_NORMAL;
            if (s[start] == '(' || s[start] == '<') {  // a remark: to the end
                i = n;
                color = CD_MUTE;
            }
        }
        int len = std::min(i - start, (int)sizeof(part) - 1);
        memcpy(part, s + start, len);
        part[len] = 0;
        text(x + 8 * start, y, part, color);
    }
}

void GuideDialog::paintLine(const Page &p, int index, int y) {
    const std::string &s = p.lines[index];
    const char *body = s.c_str() + 1;
    switch (s[0]) {
    case '=': {
        if (index > 0)
            y += 6;
        std::vector<std::string> lines = wrapWords(s.substr(1), CHARS);
        for (int i = 0; i < (int)lines.size(); i++) {
            text(TEXT_X, y, lines[i].c_str(), CD_CURSOR);
            y += LINE;
        }
        fill(TEXT_X, y, 8 * CHARS, 1, CD_BACKGROUND, CD_CURSOR, 45);
        return;
    }
    case '+': {
        if (index > 0)
            y += 4;
        std::vector<std::string> lines = wrapWords(s.substr(1), CHARS);
        for (int i = 0; i < (int)lines.size(); i++) {
            text(TEXT_X, y, lines[i].c_str(), CD_CURSOR);
            y += LINE;
        }
        return;
    }
    case '|':
    case '%': {
        bool first = index == 0 || (p.lines[index - 1][0] != '|' && p.lines[index - 1][0] != '%');
        int h = lineHeight(p, index);
        fill(3, y, SCROLL_X - 5, h, CD_BACKGROUND, CD_BORDER, 30);
        int ty = y + (first ? BOX_PAD : 0);
        if (s[0] == '%')
            paintExample(body, TEXT_X, ty);
        else
            text(TEXT_X, ty, body, CD_HILITE2);
        return;
    }
    case '>':
        fill(3, y - 1, 2, LINE, CD_HILITE1);
        text(TIP_X, y, body, CD_NORMAL);
        return;
    case '*':
        text(TEXT_X, y, body, CD_HILITE1);
        return;
    default:
        break;
    }
    // Body text: list markers stand out, links are underlined
    int x = TEXT_X;
    text(x, y, body, CD_NORMAL);
    const char *t = body;
    while (*t == ' ')
        t++;
    int mark = (int)(t - body);
    if (*t == '-' && t[1] == ' ') {
        text(x + 8 * mark, y, "-", CD_HILITE1);
    } else if (isdigit(*t)) {
        int k = 0;
        while (isdigit(t[k]))
            k++;
        if (t[k] == '.' && t[k + 1] == ' ') {
            char num[8];
            snprintf(num, sizeof(num), "%.*s", k + 1, t);
            text(x + 8 * mark, y, num, CD_HILITE1);
        }
    }
    for (int k = 0; k < (int)p.links.size(); k++) {
        const Link &link = p.links[k];
        if (link.line != index)
            continue;
        char part[40];
        int len = std::min(link.len, (int)sizeof(part) - 1);
        memcpy(part, body + link.col, len);
        part[len] = 0;
        text(x + 8 * link.col, y, part, CD_PLAY);
        fill(x + 8 * link.col, y + 8, 8 * link.len, 1, CD_PLAY);
    }
}

void GuideDialog::paintPage() {
    const Page &p = guidePages[page_];
    // Title: the page, then the section you're in
    std::string title = p.title;
    int s = sectionAt(top_);
    if (s >= 0)
        title += " > " + p.lines[p.sections[s]].substr(1);
    paintTitle(title.c_str(), "");

    int count = (int)p.lines.size();
    int end = visibleEnd();
    int base = lineY_[top_];
    for (int i = top_; i < end && i < count; i++) {
        paintLine(p, i, BODY_TOP + lineY_[i] - base);
    }

    // Scroll bar: how far into the page
    int total = lineY_[count];
    int view = BODY_BOTTOM - BODY_TOP;
    if (total > view) {
        int track = view - 2;
        int thumb = std::max(6, track * view / total);
        int y = BODY_TOP + (track - thumb) * base / std::max(1, total - view);
        fill(SCROLL_X, BODY_TOP, 3, track, CD_BACKGROUND, CD_BORDER, 35);
        fill(SCROLL_X, y, 3, thumb, CD_MUTE);
    }

    bool links = false;
    for (int k = 0; k < (int)p.links.size() && !links; k++)
        links = p.links[k].line >= top_ && p.links[k].line < end;
    if (!goTo_) {
        paintFooter("{Up/Dn} scroll  {L/R} section",
                    links ? "{A} links  {B} back  {RB+Sel} close"
                          : "{A} go to  {B} back  {RB+Sel} close");
    }
}

void GuideDialog::paintGoTo() {
    int count = (int)targets_.size();
    int visible = std::min(count, 12);
    int top = 0;
    if (goToSelection_ >= visible)
        top = goToSelection_ - visible + 1;
    int h = 18 + visible * ROW + 4;
    int x = 4, w = SCREEN - 8;  // covers the text beneath edge to edge
    int y = std::max(BODY_TOP + 4, (BODY_TOP + BODY_BOTTOM - h) / 2);
    fill(x - 2, y - 2, w + 4, h + 4, CD_CURSOR);
    fill(x, y, w, h, CD_BACKGROUND);
    text(x + 6, y + 5, "Go to", CD_HILITE1);
    fill(x, y + 15, w, 1, CD_BACKGROUND, CD_BORDER, 70);
    char line[40];
    int chars = w / 8 - 2;
    for (int i = 0; i < visible; i++) {
        int index = top + i;
        const Target &t = targets_[index];
        int ry = y + 19 + i * ROW;
        bool on = index == goToSelection_;
        if (on)
            fill(x + 2, ry - 2, w - 8, ROW, CD_HILITE2);
        std::string label = t.label;
        if (t.page >= 0) {
            std::string where = guidePages[t.page].title;
            if (t.line > 0)
                where = guidePages[t.page].lines[t.line].substr(1);
            if (label.find(where) == std::string::npos)
                label += " - " + where;
        }
        snprintf(line, sizeof(line), "%s", label.c_str());
        if ((int)strlen(line) > chars) {
            line[chars - 1] = '.';
            line[chars] = 0;
        }
        int color = on ? CD_BACKGROUND : (t.page >= 0 ? CD_HILITE2 : CD_NORMAL);
        text(x + 6, ry, line, color);
#ifdef PLATFORM_RGNANO_SIM
        if (on)
            ((AppWindow &)w_).NotePixelText(label.c_str(), true);
#endif
    }
    paintFooter("{Up/Dn} pick  {A} go", "{B} cancel  {RB+Sel} close");
}
