// from: https://github.com/xiphonics/picoTracker
#ifndef SCALE_VIEW_H
#define SCALE_VIEW_H

const int scaleCount = 47;
const int scaleNoteCount = 12;
// The last scale is the song's own: the notes ticked on the Scale screen
// (Project variable "scale custom", bit n = n semitones above the key)
const int scaleCustom = scaleCount - 1;
// Major, the notes a new custom scale starts with
const int scaleCustomDefault = 0xAB5;
extern const char *scaleNames[scaleCount];
extern const char *scaleNotes[scaleNoteCount];
extern const bool scaleSteps[scaleCount][scaleNoteCount];

// 12-bit mask of a scale's notes (bit n = n semitones above the key)
int scaleMask(int scale, int customMask);
// Is the note 'step' semitones above the key (0-11) in the scale?
bool scaleHasStep(int scale, int step, int customMask);

#endif
