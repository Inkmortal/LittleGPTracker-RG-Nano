#ifndef _SOUND_LIBRARY_H_
#define _SOUND_LIBRARY_H_

#include <string>
#include <vector>

class InstrumentBank ;

// Your own sounds and kits, kept on the card outside any song so they carry
// over to the next one (SOUNDLIB in config.xml, /mnt/Applications/Sounds on
// the Nano):
//   <lib>/<name>.lgs         one sound
//   <lib>/kits/<name>.lgk    a kit: every sound of a song, in its slots
//   <lib>/template.lgk       the kit new songs start with (if there is one)
//   <lib>/samples/*.wav      the samples those sounds play
// The files hold the same <INSTRUMENT><PARAM/> XML as a song file, plus the
// instrument's table and the name of its sample.
class SoundLibrary {
public:
	// Names without the extension, sorted
	static void ListSounds(std::vector<std::string> &names) ;
	static void ListKits(std::vector<std::string> &names) ;

	// false: error says why, in a few words for the screen
	static bool SaveSound(InstrumentBank *bank,int slot,const std::string &name,std::string &error) ;
	static bool LoadSound(InstrumentBank *bank,int slot,const std::string &name,std::string &error) ;
	static bool SaveKit(InstrumentBank *bank,const std::string &name,std::string &error) ;
	// Every sample slot takes the kit's sound, or is emptied
	static bool LoadKit(InstrumentBank *bank,const std::string &name,std::string &error) ;

	// The song's sounds become what every new song starts with
	static bool SetTemplate(InstrumentBank *bank,std::string &error) ;
	static bool HasTemplate() ;
	static bool ClearTemplate() ;
	// A new song: its (empty) slots get the template kit
	static bool LoadTemplate(InstrumentBank *bank,std::string &error) ;

	// A saved sound / kit of that (cleaned) name is on the card
	static bool SoundExists(const std::string &name) ;
	static bool KitExists(const std::string &name) ;

	// A name that is safe as a file name (letters, digits, space, - _ .)
	static std::string CleanName(const std::string &name) ;
	static std::string Folder() ;
} ;

#endif
