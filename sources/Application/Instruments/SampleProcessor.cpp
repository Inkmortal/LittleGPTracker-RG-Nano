#include "SampleProcessor.h"
#include "WavFileWriter.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Console/Trace.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const char *opNames[SEO_COUNT]={
	"normalize","crop to S..E","fade in S..E","fade out S..E","reverse S..E","trim silence"
} ;
static const char *opSuffix[SEO_COUNT]={"nrm","crop","fin","fout","rev","trim"} ;
static const char *opHelp[SEO_COUNT]={
	"S..E as loud as it gets",
	"keep only S..E",
	"S..E rises from silence",
	"S..E falls to silence",
	"S..E plays backwards",
	"cut quiet start and end"
} ;

const char *SampleProcessor::Name(int op) {
	return (op>=0 && op<SEO_COUNT)?opNames[op]:"?" ;
}

const char *SampleProcessor::Suffix(int op) {
	return (op>=0 && op<SEO_COUNT)?opSuffix[op]:"edit" ;
}

const char *SampleProcessor::Describe(int op) {
	return (op>=0 && op<SEO_COUNT)?opHelp[op]:"" ;
}

static int clampInt(int v,int lo,int hi) {
	if (v<lo) return lo ;
	if (v>hi) return hi ;
	return v ;
}

static int absPeak(const short *src,int channels,int from,int to) {
	int peak=0 ;
	for (int f=from;f<to;f++) {
		const short *p=src+f*channels ;
		for (int c=0;c<channels;c++) {
			int v=p[c]<0?-p[c]:p[c] ;
			if (v>peak) peak=v ;
		}
	}
	return peak ;
}

bool SampleProcessor::Plan(int op,const short *src,int frames,int channels,int rate,
                           int S,int L,int E,SampleEdit &e) {
	e.op=op ;
	e.src=src ;
	e.frames=frames ;
	e.channels=channels<1?1:channels ;
	e.rate=rate>0?rate:44100 ;
	e.ok=false ;
	e.why="" ;
	e.gain=1.0f ;
	if (!src || frames<2) {
		e.why="no sample to edit" ;
		return false ;
	}
	// The markers as the instrument plays them
	if (E<=0 || E>frames) E=frames ;
	S=clampInt(S,0,frames) ;
	if (S>E) S=E ;
	L=clampInt(L,S,E) ;
	e.rangeStart=S ;
	e.rangeEnd=E ;
	e.offset=0 ;
	e.outFrames=frames ;
	e.newStart=S ;
	e.newLoop=L ;
	e.newEnd=E ;
	int len=E-S ;

	switch(op) {
		case SEO_NORMALIZE: {
			if (len<2) { e.why="S..E is too short" ; return false ; }
			int peak=absPeak(src,e.channels,S,E) ;
			if (peak==0) { e.why="S..E is silent" ; return false ; }
			e.gain=32767.0f/peak ;
			if (e.gain<1.001f) { e.why="already as loud as it gets" ; return false ; }
			break ;
		}
		case SEO_CROP:
			if (len<2) { e.why="S..E is too short" ; return false ; }
			if (S==0 && E==frames) { e.why="move S or E in first" ; return false ; }
			e.offset=S ;
			e.outFrames=len ;
			e.newStart=0 ;
			e.newLoop=L-S ;
			e.newEnd=len ;
			break ;
		case SEO_FADE_IN:
		case SEO_FADE_OUT:
		case SEO_REVERSE:
			if (len<2) { e.why="S..E is too short" ; return false ; }
			break ;
		case SEO_TRIM: {
			int peak=absPeak(src,e.channels,0,frames) ;
			if (peak==0) { e.why="the sample is silent" ; return false ; }
			// quieter than 48 dB under the peak counts as silence
			int threshold=peak/256 ;
			if (threshold<8) threshold=8 ;
			int firstLoud=-1,lastLoud=-1 ;
			for (int f=0;f<frames && firstLoud<0;f++) {
				for (int c=0;c<e.channels;c++) {
					int v=src[f*e.channels+c] ;
					if (v>threshold || v<-threshold) { firstLoud=f ; break ; }
				}
			}
			for (int f=frames-1;f>=0 && lastLoud<0;f--) {
				for (int c=0;c<e.channels;c++) {
					int v=src[f*e.channels+c] ;
					if (v>threshold || v<-threshold) { lastLoud=f ; break ; }
				}
			}
			// keep 2 ms before the sound starts and 10 ms after it ends
			int from=firstLoud-e.rate/500 ;
			int to=lastLoud+1+e.rate/100 ;
			if (from<0) from=0 ;
			if (to>frames) to=frames ;
			if (from==0 && to==frames) { e.why="no silence at the ends" ; return false ; }
			e.offset=from ;
			e.outFrames=to-from ;
			e.rangeStart=from ;
			e.rangeEnd=to ;
			e.newStart=clampInt(S-from,0,e.outFrames) ;
			e.newEnd=(E>=frames)?e.outFrames:clampInt(E-from,0,e.outFrames) ;
			if (e.newStart>e.newEnd) e.newStart=e.newEnd ;
			e.newLoop=clampInt(L-from,e.newStart,e.newEnd) ;
			break ;
		}
		default:
			e.why="unknown edit" ;
			return false ;
	}
	e.ok=true ;
	return true ;
}

int SampleProcessor::SourceFrame(const SampleEdit &e,int outFrame) {
	int f=e.offset+outFrame ;
	if (e.op==SEO_REVERSE && f>=e.rangeStart && f<e.rangeEnd) {
		f=e.rangeStart+e.rangeEnd-1-f ;
	}
	return f ;
}

float SampleProcessor::Gain(const SampleEdit &e,int outFrame) {
	int f=e.offset+outFrame ;
	if (f<e.rangeStart || f>=e.rangeEnd) return 1.0f ;
	int len=e.rangeEnd-e.rangeStart ;
	switch(e.op) {
		case SEO_NORMALIZE:
			return e.gain ;
		case SEO_FADE_IN:
			return len>1?(float)(f-e.rangeStart)/(float)(len-1):1.0f ;
		case SEO_FADE_OUT:
			return len>1?1.0f-(float)(f-e.rangeStart)/(float)(len-1):0.0f ;
		default:
			return 1.0f ;
	}
}

short SampleProcessor::Value(const SampleEdit &e,int outFrame,int channel) {
	int f=SourceFrame(e,outFrame) ;
	if (f<0 || f>=e.frames) return 0 ;
	float g=Gain(e,outFrame) ;
	int v=e.src[f*e.channels+channel] ;
	if (g==1.0f) return (short)v ;
	float x=v*g ;
	int r=(int)(x<0?x-0.5f:x+0.5f) ;
	if (r>32767) r=32767 ;
	if (r<-32768) r=-32768 ;
	return (short)r ;
}

bool SampleProcessor::Write(const SampleEdit &e,const char *path) {
	if (!e.ok || e.outFrames<=0) return false ;
	WavFileWriter writer(path,e.channels,e.rate) ;
	if (!writer.IsOpen()) {
		Trace::Error("SampleProcessor: can't write %s",path) ;
		return false ;
	}
	const int CHUNK=1024 ;
	short buffer[CHUNK*2] ;
	int perChunk=CHUNK*2/e.channels ;
	for (int done=0;done<e.outFrames;) {
		int n=e.outFrames-done ;
		if (n>perChunk) n=perChunk ;
		short *p=buffer ;
		for (int i=0;i<n;i++) {
			for (int c=0;c<e.channels;c++) {
				*p++=Value(e,done+i,c) ;
			}
		}
		writer.AddFrames(buffer,n) ;
		done+=n ;
	}
	writer.Close() ;
	return true ;
}

static bool sampleFileExists(const std::string &name) {
	// Exists() checks the literal path, so resolve the alias first
	Path alias(std::string("samples:")+name) ;
	Path file(alias.GetPath()) ;
	return file.Exists() ;
}

std::string SampleProcessor::OutputName(const char *sourceName,int op) {
	std::string base=sourceName?sourceName:"sample" ;
	size_t len=base.size() ;
	if (len>4) {
		std::string ext=base.substr(len-4) ;
		for (size_t i=0;i<ext.size();i++) ext[i]=(char)tolower((unsigned char)ext[i]) ;
		if (ext==".wav") base=base.substr(0,len-4) ;
	}
	if (base.empty()) base="sample" ;
	char name[256] ;
	for (int n=1;n<100;n++) {
		if (n==1) {
			snprintf(name,sizeof(name),"%s_%s.wav",base.c_str(),Suffix(op)) ;
		} else {
			snprintf(name,sizeof(name),"%s_%s%d.wav",base.c_str(),Suffix(op),n) ;
		}
		if (!sampleFileExists(name)) return name ;
	}
	return "" ;
}
