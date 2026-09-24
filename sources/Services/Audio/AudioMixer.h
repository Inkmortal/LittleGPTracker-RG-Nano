#ifndef _AUDIO_MIXER_H_
#define _AUDIO_MIXER_H_

#include "AudioModule.h"
#include "Foundation/T_SimpleList.h"
#include "Application/Instruments/WavFileWriter.h"
#include <string>

struct SoftClipData {
    float alpha;
	float alpha23;
	float alphaInv;
	float gainCmp;
};

class AudioMixer: public AudioModule,public T_SimpleList<AudioModule> {
public:
	AudioMixer(const char *name) ;
	virtual ~AudioMixer() ;
	virtual bool Render(fixed *buffer,int samplecount) ;
	void SetFileRenderer(const char *path) ;
	void EnableRendering(bool enable) ;
	void SetVolume(fixed volume) ;
    virtual void SetSoftclip(int clip, int gain);
    virtual void SetMasterVolume(int volume) ;
	virtual bool Clipped() ;
	int GetPeakPercent() ;
	static const int WAVEFORM_SIZE = 48 ;
	int GetWaveformSample(int index) ;
	int GetWaveformMin(int index) ;
	int GetWaveformMax(int index) ;
	// Write the next 'frames' of this mixer's output to a WAV file, while
	// playing normally (render to sample)
	bool StartCapture(const char *path,int frames) ;
	void CancelCapture() ;
	bool CaptureActive() { return capture_!=0 ; } ;
	// 0..100 of the frames asked for
	int CaptureProgress() ;
	
private:
  fixed hardClip(fixed sample);
  fixed softClip(fixed sample);
  void updateWaveform(fixed *buffer,int samplecount,int peak);
  bool enableRendering_;
  std::string renderPath_;
  WavFileWriter *writer_;
  WavFileWriter *capture_;
  int captureLeft_;
  int captureTotal_;
  fixed volume_;
  std::string name_;
  SoftClipData softClipData_[4];
  int softclip_;
  int softclipGain_;
  int masterVolume_;
  bool clipped_;
  int peakPercent_;
  int waveform_[WAVEFORM_SIZE];
  int waveformMin_[WAVEFORM_SIZE];
  int waveformMax_[WAVEFORM_SIZE];
} ;
#endif
