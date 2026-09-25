#ifndef _WAV_FILE_WRITER_H_
#define _WAV_FILE_WRITER_H_

#include "System/FileSystem/FileSystem.h"
#include "Application/Utils/fixed.h"

// 16-bit PCM WAV writer. The default is the mixer's stereo 44.1 kHz
// (renders); sample editing keeps a sample's own channels and rate.
class WavFileWriter {
public:
	WavFileWriter(const char *path,int channels=2,int sampleRate=44100) ;
	~WavFileWriter() ;
	bool IsOpen() { return file_!=0 ; }
	void AddBuffer(fixed *,int size) ; // stereo fixed frames (channels must be 2)
	void AddFrames(const short *frames,int count) ; // interleaved, this writer's channels
	void Close() ;
private:
	int sampleCount_ ; // frames written
	int channels_ ;
	short *buffer_ ;
	int bufferSize_ ;
	I_File *file_ ;
} ;
#endif
