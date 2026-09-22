
#include "AudioDriver.h"
#include "System/System/System.h"
#include "System/Console/Trace.h"
#include "System/Console/n_assert.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static double gRenderLoad = 0.0;   // smoothed, 1.0 == real time
static double gRenderLoadPeak = 0.0;
static int gLastBufferSamples = 0;
static volatile unsigned long gUnderruns = 0;

static double nowMicros() {
#ifdef _WIN32
  static LARGE_INTEGER freq = {0};
  if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return (double)t.QuadPart * 1000000.0 / (double)freq.QuadPart;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
#endif
}

#ifdef PLATFORM_RGNANO_SIM
static int gSimAudioPeak = 0;
static unsigned long gSimAudioNonSilentBytes = 0;
static FILE *gSimAudioCaptureFile = 0;
static unsigned long gSimAudioCaptureBytes = 0;
static bool gSimAudioMuted = false;

static void WriteSimWav16(unsigned short value) {
  unsigned char bytes[2];
  bytes[0] = (unsigned char)(value & 0xFF);
  bytes[1] = (unsigned char)((value >> 8) & 0xFF);
  fwrite(bytes, 1, 2, gSimAudioCaptureFile);
}

static void WriteSimWav32(unsigned long value) {
  unsigned char bytes[4];
  bytes[0] = (unsigned char)(value & 0xFF);
  bytes[1] = (unsigned char)((value >> 8) & 0xFF);
  bytes[2] = (unsigned char)((value >> 16) & 0xFF);
  bytes[3] = (unsigned char)((value >> 24) & 0xFF);
  fwrite(bytes, 1, 4, gSimAudioCaptureFile);
}

static void WriteSimWavHeader(unsigned long dataBytes) {
  fwrite("RIFF", 1, 4, gSimAudioCaptureFile);
  WriteSimWav32(36 + dataBytes);
  fwrite("WAVE", 1, 4, gSimAudioCaptureFile);
  fwrite("fmt ", 1, 4, gSimAudioCaptureFile);
  WriteSimWav32(16);
  WriteSimWav16(1);
  WriteSimWav16(2);
  WriteSimWav32(44100);
  WriteSimWav32(44100 * 2 * 2);
  WriteSimWav16(4);
  WriteSimWav16(16);
  fwrite("data", 1, 4, gSimAudioCaptureFile);
  WriteSimWav32(dataBytes);
}
#endif

AudioDriver::AudioDriver(AudioSettings &settings) {
	settings_=settings ;
}

AudioDriver::~AudioDriver() {
}

bool AudioDriver::Init() {

  // Clear all buffers
	
   for (int i=0;i<SOUND_BUFFER_COUNT;i++) {
     pool_[i].buffer_=0 ;
     pool_[i].size_=0 ;
   } ;
   isPlaying_=false;	 

   return InitDriver() ;
}

void AudioDriver::Close() {
	CloseDriver() ;
};

bool AudioDriver::Start() {

    isPlaying_=true ; 

    for (int i=0;i<SOUND_BUFFER_COUNT;i++) {
  	  SAFE_FREE(pool_[i].buffer_) ;
    } ;
	 
    poolQueuePosition_=0 ;
    poolPlayPosition_=0 ;
	hasData_=false ;

    return StartDriver() ;
};

void AudioDriver::Stop() {
     isPlaying_=false ;
	hasData_=false ;
     StopDriver() ;
}

void AudioDriver::AddBuffer(short *buffer,int samplecount) {
  
  int len=samplecount*2*sizeof(short) ;

  if (!isPlaying_) return ;

  if (len>SOUND_BUFFER_MAX) {
      Trace::Error("Alert: buffer size exceeded") ;
  }

  if (pool_[poolQueuePosition_].buffer_!=0) {
  NInvalid ;
  Trace::Error("Audio overrun, please report") ;
  SAFE_FREE(pool_[poolQueuePosition_].buffer_) ;
  return ;
  }	

  pool_[poolQueuePosition_].buffer_=(char*) ((short *)SYS_MALLOC(len)) ;

  SYS_MEMCPY(pool_[poolQueuePosition_].buffer_,(char *)buffer,len) ;
#ifdef PLATFORM_RGNANO_SIM
  int sampleCount = len / (int)sizeof(short);
  for (int i = 0; i < sampleCount; i++) {
    int value = abs((int)buffer[i]);
    if (value > gSimAudioPeak) {
      gSimAudioPeak = value;
    }
    if (value > 0) {
      gSimAudioNonSilentBytes += sizeof(short);
    }
  }
  if (gSimAudioCaptureFile) {
    fwrite(buffer, 1, len, gSimAudioCaptureFile);
    gSimAudioCaptureBytes += len;
  }
  // Measured and captured above; muted runs just don't reach the speakers
  if (gSimAudioMuted) {
    SYS_MEMSET(pool_[poolQueuePosition_].buffer_, 0, len);
  }
#endif
  gLastBufferSamples = samplecount ;
  pool_[poolQueuePosition_].size_=len ;
  poolQueuePosition_=(poolQueuePosition_+1)%SOUND_BUFFER_COUNT ;
	hasData_=true ;
}

void AudioDriver::OnNewBufferNeeded() {
  double start = nowMicros() ;
  gLastBufferSamples = 0 ;
  SetChanged() ;
  Event event(Event::ADET_BUFFERNEEDED);
  NotifyObservers(&event) ;
  if (gLastBufferSamples > 0) {
    double budget = gLastBufferSamples * 1000000.0 / 44100.0 ;
    double load = (nowMicros() - start) / budget ;
    gRenderLoad = gRenderLoad * 0.95 + load * 0.05 ;
    if (load > gRenderLoadPeak) gRenderLoadPeak = load ;
#ifdef PLATFORM_RGNANO_SIM
    static int buffers = 0 ;
    if (++buffers % 400 == 0) {
      Trace::Log("AUDIOLOAD", "avg %d%% peak %d%% underruns %lu",
                 GetRenderLoadPercent(), TakeRenderLoadPeak(), gUnderruns) ;
    }
#endif
  }
} ;

int AudioDriver::GetRenderLoadPercent() {
  return (int)(gRenderLoad * 100.0 + 0.5) ;
}

int AudioDriver::TakeRenderLoadPeak() {
  int peak = (int)(gRenderLoadPeak * 100.0 + 0.5) ;
  gRenderLoadPeak = 0.0 ;
  return peak ;
}

unsigned long AudioDriver::GetUnderrunCount() {
  return gUnderruns ;
}

void AudioDriver::noteUnderrun() {
  gUnderruns++ ;
}

void AudioDriver::onAudioBufferTick()
{
  SetChanged() ;
  Event event(Event::ADET_DRIVERTICK);
  NotifyObservers(&event) ;
}

bool AudioDriver::hasData() {
	return hasData_ ;
}  ;

AudioSettings AudioDriver::GetAudioSettings() {
	return settings_ ;
} ;

#ifdef PLATFORM_RGNANO_SIM
void AudioDriver::ResetSimAudioStats() {
  gSimAudioPeak = 0;
  gSimAudioNonSilentBytes = 0;
}

int AudioDriver::GetSimAudioPeak() { return gSimAudioPeak; }

void AudioDriver::SetSimAudioMuted(bool muted) { gSimAudioMuted = muted; }

unsigned long AudioDriver::GetSimAudioNonSilentBytes() {
  return gSimAudioNonSilentBytes;
}

bool AudioDriver::BeginSimAudioCapture(const char *path) {
  EndSimAudioCapture();
  if (!path || !path[0]) {
    Trace::Error("RGNANO_SIM audio capture missing path");
    return false;
  }
  gSimAudioCaptureFile = fopen(path, "wb");
  if (!gSimAudioCaptureFile) {
    Trace::Error("RGNANO_SIM failed to open audio capture %s", path);
    return false;
  }
  gSimAudioCaptureBytes = 0;
  WriteSimWavHeader(0);
  Trace::Log("RGNANO_SIM", "audio capture started %s", path);
  return true;
}

void AudioDriver::EndSimAudioCapture() {
  if (!gSimAudioCaptureFile) {
    return;
  }
  fseek(gSimAudioCaptureFile, 0, SEEK_SET);
  WriteSimWavHeader(gSimAudioCaptureBytes);
  fclose(gSimAudioCaptureFile);
  gSimAudioCaptureFile = 0;
  Trace::Log("RGNANO_SIM", "audio capture stopped bytes=%lu", gSimAudioCaptureBytes);
}

unsigned long AudioDriver::GetSimAudioCaptureBytes() {
  return gSimAudioCaptureBytes;
}
#endif
