#include "AudioProfiler.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define PROFILE_DEPTH 8

bool AudioProfiler::enabled_=false ;

static AudioProfiler::MarkHook gHook=0 ;
static int gStack[PROFILE_DEPTH] ;
static int gDepth=0 ;
static int gCurrent=APS_OTHER ;
static double gLast=0.0 ;
static double gBufferStart=0.0 ;
static double gCurrentUs[APS_COUNT] ;
static double gLastUs[APS_COUNT] ;
static double gLastTotal=0.0 ;
static double gTotalUs[APS_COUNT] ;
static unsigned long gBuffers=0 ;
static unsigned long gFrames=0 ;
static int gPeak=0 ;
static int gPeakShares[APS_COUNT] ;

static const char *slotNames[APS_COUNT]={
	"other","seq","ch1","ch2","ch3","ch4","ch5","ch6","ch7","ch8",
	"bus","fx","modclk","meq","lim","mst"
} ;

const char *AudioProfiler::SlotName(int slot) {
	if (slot<0 || slot>=APS_COUNT) return "?" ;
	return slotNames[slot] ;
}

double AudioProfiler::NowMicros() {
#ifdef _WIN32
	static LARGE_INTEGER freq={0} ;
	if (freq.QuadPart==0) QueryPerformanceFrequency(&freq) ;
	LARGE_INTEGER t ;
	QueryPerformanceCounter(&t) ;
	return (double)t.QuadPart*1000000.0/(double)freq.QuadPart ;
#else
	struct timespec ts ;
	clock_gettime(CLOCK_MONOTONIC,&ts) ;
	return ts.tv_sec*1000000.0+ts.tv_nsec/1000.0 ;
#endif
}

double AudioProfiler::ThreadMicros() {
#if defined(_WIN32) || !defined(CLOCK_THREAD_CPUTIME_ID)
	return NowMicros() ;
#else
	struct timespec ts ;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts)!=0) return NowMicros() ;
	return ts.tv_sec*1000000.0+ts.tv_nsec/1000.0 ;
#endif
}

void AudioProfiler::Enable(bool on) {
	enabled_=on ;
	gDepth=0 ;
	gCurrent=APS_OTHER ;
}

void AudioProfiler::SetMarkHook(MarkHook hook) {
	gHook=hook ;
}

void AudioProfiler::switchTo(int slot) {
	if (gHook) {
		gHook(slot) ;
	} else {
		double now=NowMicros() ;
		gCurrentUs[gCurrent]+=now-gLast ;
		gLast=now ;
	}
	gCurrent=slot ;
}

void AudioProfiler::enter(int slot) {
	if (slot>=APS_COUNT) return ;
	if (gDepth<PROFILE_DEPTH) gStack[gDepth]=gCurrent ;
	gDepth++ ;
	switchTo(slot) ;
}

void AudioProfiler::leave() {
	if (gDepth<=0) return ;
	gDepth-- ;
	switchTo(gDepth<PROFILE_DEPTH?gStack[gDepth]:APS_OTHER) ;
}

void AudioProfiler::BeginBuffer() {
	if (!enabled_) return ;
	memset(gCurrentUs,0,sizeof(gCurrentUs)) ;
	gDepth=0 ;
	gCurrent=APS_OTHER ;
	if (gHook) {
		gHook(-2) ;
	} else {
		gLast=gBufferStart=NowMicros() ;
	}
}

void AudioProfiler::EndBuffer(int frames) {
	if (!enabled_) return ;
	if (gHook) {
		gHook(-1) ;
		return ;
	}
	double now=NowMicros() ;
	gCurrentUs[gCurrent]+=now-gLast ;
	gLast=now ;
	memcpy(gLastUs,gCurrentUs,sizeof(gLastUs)) ;
	gLastTotal=now-gBufferStart ;
	for (int i=0;i<APS_COUNT;i++) gTotalUs[i]+=gCurrentUs[i] ;
	gBuffers++ ;
	gFrames+=frames ;
	if (frames>0) {
		double budget=frames*1000000.0/44100.0 ;
		int percent=(int)(gLastTotal*100.0/budget+0.5) ;
		if (percent>gPeak) {
			gPeak=percent ;
			for (int i=0;i<APS_COUNT;i++) {
				gPeakShares[i]=(int)(gCurrentUs[i]*100.0/budget+0.5) ;
			}
		}
	}
}

double AudioProfiler::LastMicros(int slot) {
	if (slot<0 || slot>=APS_COUNT) return 0.0 ;
	return gLastUs[slot] ;
}

double AudioProfiler::LastTotalMicros() {
	return gLastTotal ;
}

double AudioProfiler::TotalMicros(int slot) {
	if (slot<0 || slot>=APS_COUNT) return 0.0 ;
	return gTotalUs[slot] ;
}

unsigned long AudioProfiler::TotalBuffers() {
	return gBuffers ;
}

unsigned long AudioProfiler::TotalFrames() {
	return gFrames ;
}

int AudioProfiler::TakePeak(int shares[APS_COUNT]) {
	int peak=gPeak ;
	if (shares) memcpy(shares,gPeakShares,sizeof(gPeakShares)) ;
	gPeak=0 ;
	memset(gPeakShares,0,sizeof(gPeakShares)) ;
	return peak ;
}

void AudioProfiler::Reset() {
	memset(gTotalUs,0,sizeof(gTotalUs)) ;
	gBuffers=0 ;
	gFrames=0 ;
	gPeak=0 ;
	memset(gPeakShares,0,sizeof(gPeakShares)) ;
}
