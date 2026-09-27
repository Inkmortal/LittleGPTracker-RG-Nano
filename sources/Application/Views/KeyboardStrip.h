#ifndef _KEYBOARD_STRIP_H_
#define _KEYBOARD_STRIP_H_

// One octave of piano keys, drawn in pixels: the Scale screen's keyboard
// and the Rack's. Notes are 0-11 from C.

// Which white key each note sits on, or the white key left of a black one
extern const int KeyboardWhiteIndex[12];
extern const bool KeyboardIsBlack[12];

struct KeyboardStrip {
  int x;       // left edge, pixels
  int top;     // top edge, pixels
  int whiteW;  // white key width (7 of them)
  int height;  // white key height
  int blackW;
  int blackH;
};

class SDLGUIWindowImp;

// key: the scale's key note (-1: no scale, every key plain); mask: the
// scale's notes from the key (bit 0 = key); cursor: a bar under that note
// (-1: none)
void DrawKeyboardStrip(SDLGUIWindowImp *imp, const KeyboardStrip &k, int key,
                       int mask, int cursor);

#endif
