#ifndef _RECENT_SONGS_H_
#define _RECENT_SONGS_H_

#include <string>
#include <vector>

// Which songs were opened most recently, kept in a small file on the card
// (root:.lgpt-recent) instead of file dates: the RG Nano has no real-time
// clock, so dates of files saved on the device are meaningless.
namespace RecentSongs {
// Move a song folder (e.g. "lgpt_JadeSword") to the front of the list
void Touch(const std::string &folder);
// Song folders, most recently opened first
std::vector<std::string> List();
// Song list order: most recent first (default) or A-Z
bool SortByRecent();
void SetSortByRecent(bool recent);
} // namespace RecentSongs

#endif
