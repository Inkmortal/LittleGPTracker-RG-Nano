#ifndef _AUDIO_MIXER_H_
#define _AUDIO_MIXER_H_

#include "Services/Audio/AudioInsert.h"
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
	// Effects run in order on the summed output, before volume and
	// clipping (master EQ, then the limiter). SetInsert replaces them all.
	void SetInsert(AudioInsert *insert) { insertCount_=0 ; AddInsert(insert) ; } ;
	void AddInsert(AudioInsert *insert) {
		if (insert && insertCount_<MAX_INSERTS) inserts_[insertCount_++]=insert ;
	} ;
	static const int MAX_INSERTS = 4 ;
	
private:
  // The soft clip settings for one buffer
  struct SoftClipCurve {
    bool on;
    float posMax, negMax;       // full scale, as float
    float posScale, negScale;   // alphaInv / full scale
    float alpha, alpha23, gain;
  };
  void prepareSoftClip(SoftClipCurve &curve);
  fixed hardClip(fixed sample);
  fixed softClip(fixed sample, const SoftClipCurve &curve);
  void clipScan(fixed *buffer,int frames,int &peak,fixed &sumMin,fixed &sumMax);
  void updateWaveform(fixed sumMin,fixed sumMax,int samplecount,int peak);
  bool enableRendering_;
  std::string renderPath_;
  WavFileWriter *writer_;
  AudioInsert *inserts_[MAX_INSERTS];
  int insertCount_;
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
