#include "RecentSongs.h"
#include "System/FileSystem/FileSystem.h"
#include <string.h>
#include <vector>

#define RECENT_FILE "root:.lgpt-recent"
#define SORT_FILE "root:.lgpt-sort"
#define RECENT_MAX 100

static std::string readFile(const char *name) {
    Path path(name);
    I_File *file =
        FileSystem::GetInstance()->Open(path.GetPath().c_str(), (char *)"r");
    if (!file)
        return "";
    std::string text;
    char buffer[512];
    int n;
    while ((n = file->Read(buffer, 1, sizeof(buffer))) > 0) {
        text.append(buffer, n);
    }
    file->Close();
    delete file;
    return text;
}

static void writeFile(const char *name, const std::string &text) {
    Path path(name);
    I_File *file =
        FileSystem::GetInstance()->Open(path.GetPath().c_str(), (char *)"w");
    if (!file)
        return;
    file->Write(text.c_str(), 1, text.size());
    file->Close();
    delete file;
}

static std::vector<std::string> loadList() {
    std::vector<std::string> list;
    std::string text = readFile(RECENT_FILE);
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (!line.empty())
            list.push_back(line);
        start = end + 1;
    }
    return list;
}

void RecentSongs::Touch(const std::string &folder) {
    if (folder.empty())
        return;
    std::vector<std::string> list = loadList();
    std::string text = folder + "\n";
    int kept = 1;
    for (size_t i = 0; i < list.size() && kept < RECENT_MAX; i++) {
        if (list[i] == folder)
            continue;
        text += list[i] + "\n";
        kept++;
    }
    writeFile(RECENT_FILE, text);
}

std::vector<std::string> RecentSongs::List() { return loadList(); }

// Read once: the song list asks on every redraw
static int sortByRecent = -1;

bool RecentSongs::SortByRecent() {
    if (sortByRecent < 0)
        sortByRecent = readFile(SORT_FILE).compare(0, 4, "name") != 0 ? 1 : 0;
    return sortByRecent == 1;
}

void RecentSongs::SetSortByRecent(bool recent) {
    sortByRecent = recent ? 1 : 0;
    writeFile(SORT_FILE, recent ? "recent\n" : "name\n");
}
