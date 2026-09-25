#ifndef _SAMPLE_PROCESSOR_H_
#define _SAMPLE_PROCESSOR_H_

#include <string>

// Sample editing, M8 style "processes" on the S..E range, without ever
// touching the original: the result is a new WAV next to it in the song's
// samples folder (see SampleEditDialog).
//
// Every process is a mapping: output frame i plays source frame
// SourceFrame(i) at Gain(). The same mapping draws the "after" picture and
// writes the file, so what you see is what you get.
enum SampleEditOp {
	SEO_NORMALIZE=0, // S..E as loud as it gets without clipping
	SEO_CROP,        // keep only S..E
	SEO_FADE_IN,     // S..E rises from silence
	SEO_FADE_OUT,    // S..E falls to silence
	SEO_REVERSE,     // S..E backwards
	SEO_TRIM,        // cut the silence at both ends
	SEO_COUNT
} ;

struct SampleEdit {
	int op ;
	const short *src ;  // interleaved 16-bit frames
	int frames ;
	int channels ;
	int rate ;
	bool ok ;           // false: nothing to do, why says so
	const char *why ;
	int outFrames ;
	int offset ;        // output frame 0 = source frame offset
	int rangeStart ;    // source frames the process works on
	int rangeEnd ;
	float gain ;        // normalize
	// S, L, E for the new sample (same places in the sound)
	int newStart ;
	int newLoop ;
	int newEnd ;
} ;

class SampleProcessor {
public:
	static const char *Name(int op) ;
	static const char *Suffix(int op) ;
	// What it will do, one short line
	static const char *Describe(int op) ;
	// Work out the edit for these markers; false (and edit.why) when it
	// would change nothing
	static bool Plan(int op,const short *src,int frames,int channels,int rate,
	                 int S,int L,int E,SampleEdit &edit) ;
	static int SourceFrame(const SampleEdit &e,int outFrame) ;
	static float Gain(const SampleEdit &e,int outFrame) ;
	static short Value(const SampleEdit &e,int outFrame,int channel) ;
	// Write the result as a 16-bit WAV (path may use an alias: samples:x)
	static bool Write(const SampleEdit &e,const char *path) ;
	// A free file name in the song's samples folder: kick.wav -> kick_rev.wav
	// (kick_rev2.wav ... when taken); empty if none is free
	static std::string OutputName(const char *sourceName,int op) ;
} ;

#endif
