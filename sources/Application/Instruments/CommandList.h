
#ifndef _COMMAND_LIST_H_
#define _COMMAND_LIST_H_

#include "Foundation/Types/Types.h"

#define I_CMD_NONE MAKE_FOURCC('-','-','-','-')
#define I_CMD_KILL MAKE_FOURCC('K','I','L','L')
#define I_CMD_LPOF MAKE_FOURCC('L','P','O','F')
#define I_CMD_ARPG MAKE_FOURCC('A','R','P','G')
#define I_CMD_VOLM MAKE_FOURCC('V','O','L','M')
#define I_CMD_PTCH MAKE_FOURCC('P','T','C','H')
#define I_CMD_HOP  MAKE_FOURCC('H','O','P',' ')
#define I_CMD_LEGA MAKE_FOURCC('L','E','G','A')
#define I_CMD_RTRG MAKE_FOURCC('R','T','R','G')
#define I_CMD_TMPO MAKE_FOURCC('T','M','P','O')
#define I_CMD_MDCC MAKE_FOURCC('M','D','C','C')
#define I_CMD_MDPG MAKE_FOURCC('M','D','P','G')
#define I_CMD_MVEL MAKE_FOURCC('M','V','E','L')
#define I_CMD_PLOF MAKE_FOURCC('P','L','O','F')
// PLAY 00bb: sample play mode for this note (00 forward, 01 reverse ...),
// the M8's PLY
#define I_CMD_PLAY MAKE_FOURCC('P','L','A','Y')
#define I_CMD_FLTR MAKE_FOURCC('F','L','T','R')
#define I_CMD_TABL MAKE_FOURCC('T','A','B','L')
#define I_CMD_CRSH MAKE_FOURCC('C','R','S','H')
#define I_CMD_FCUT MAKE_FOURCC('F','C','U','T')
#define I_CMD_FRES MAKE_FOURCC('F','R','E','S')
#define I_CMD_PAN_ MAKE_FOURCC('P','A','N',' ')
#define I_CMD_GROV MAKE_FOURCC('G','R','O','V')
#define I_CMD_FBTU MAKE_FOURCC('F','B','T','U')
#define I_CMD_FBAM MAKE_FOURCC('F','B','A','M')
#define I_CMD_IRTG MAKE_FOURCC('I','R','T','G')
#define I_CMD_PFIN MAKE_FOURCC('P','F','I','N')
#define I_CMD_DLAY MAKE_FOURCC('D','L','A','Y')
#define I_CMD_FBMX MAKE_FOURCC('F','B','M','X')
#define I_CMD_FBTN MAKE_FOURCC('F','B','T','N')
#define I_CMD_SLCE MAKE_FOURCC('S','L','C','E')
#define I_CMD_STOP MAKE_FOURCC('S','T','O','P')
#define I_CMD_CHRD MAKE_FOURCC('C','H','R','D')
// Randomness (as on the M8): RAND 00bb randomizes the other command on the
// step by up to bb (on its own: the note, up to bb semitones, in scale);
// CHNC 00bb plays the note with probability bb/FF
#define I_CMD_RAND MAKE_FOURCC('R','A','N','D')
#define I_CMD_CHNC MAKE_FOURCC('C','H','N','C')
// Sequencer commands after the M8's (handled by the Player, so they work
// with every instrument type):
// ROLL xy  re-strike the note every y ticks, volume moving by x each hit
//          (1-7 quieter, 9-F louder); y=0: one re-strike after x ticks (RET)
// VIBR xy  vibrato, speed x, depth y (PVB)
// SEED --bb  restart this track's random numbers from seed bb (SED)
// NTH  --xy  the note plays on pass x of every y passes of its phrase;
//          x=0: on every pass but the y-th (NTH / trig conditions)
// TICK --bb  the track's table moves one row every bb ticks (TIC)
// THOP --0b  the track's table jumps to row b (THO)
// TRSP --bb  transpose the whole song by bb semitones (signed) (TSP)
// SCAL aabb  song Key aa (0C = off) and Scale bb (SCG)
#define I_CMD_ROLL MAKE_FOURCC('R','O','L','L')
#define I_CMD_VIBR MAKE_FOURCC('V','I','B','R')
#define I_CMD_SEED MAKE_FOURCC('S','E','E','D')
#define I_CMD_NTH_ MAKE_FOURCC('N','T','H',' ')
#define I_CMD_TICK MAKE_FOURCC('T','I','C','K')
#define I_CMD_THOP MAKE_FOURCC('T','H','O','P')
#define I_CMD_TRSP MAKE_FOURCC('T','R','S','P')
#define I_CMD_SCAL MAKE_FOURCC('S','C','A','L')
// Macro synth: timbre / color to bb at speed aa (like FCUT)
#define I_CMD_TIMB MAKE_FOURCC('T','I','M','B')
#define I_CMD_COLR MAKE_FOURCC('C','O','L','R')

class CommandList {
public:
	static FourCC GetNext(FourCC current) ;
	static FourCC GetPrev(FourCC current) ;
	static FourCC GetNextAlpha(FourCC current) ;
    static FourCC GetPrevAlpha(FourCC current);
    static int GetCount() ;
	static FourCC GetAt(int index) ;
	static int IndexOf(FourCC current) ;
	static FourCC GetFirst() ;
	static FourCC GetLast() ;
	static bool IsFirst(FourCC current) ;
	static bool IsLast(FourCC current) ;
};
#endif

