#include "System/Console/Trace.h"
#include "AudioMixer.h"
#include "System/System/System.h"
#include <math.h>

#define MAX_POSITIVE_FIXED i2fp(32767)
#define MAX_NEGATIVE_FIXED i2fp(-32768)

AudioMixer::AudioMixer(const char *name):
	T_SimpleList<AudioModule>(false),
	enableRendering_(0),
	writer_(0),
	capture_(0),
	insert_(0),
	captureLeft_(0),
	captureTotal_(0),
	name_(name)
{
	volume_=(i2fp(1)) ;
	softclip_ = -1;
    softclipGain_ = 0 ;
	masterVolume_ = 100 ;
	clipped_ = false ;
	peakPercent_ = 0 ;
    for (int i=0; i<WAVEFORM_SIZE; i++) {
        waveform_[i]=0;
        waveformMin_[i]=0;
        waveformMax_[i]=0;
    }
	
	// Precalculate constant values for softclipping algorithm
	softClipData_[0].alpha = 1.45f; // -1.5db (approx.)
	softClipData_[1].alpha = 1.07f; // -3db (approx.)
	softClipData_[2].alpha = 0.75f; // -6db (approx.)
	softClipData_[3].alpha = 0.53f; // -9db (approx.)

	for (int i = 0; i < 4; i++) {
		softClipData_[i].alpha23 = softClipData_[i].alpha * (2.0f / 3.0f);
		softClipData_[i].alphaInv = 1.0f / softClipData_[i].alpha;

		if (softClipData_[i].alpha > 1.0f) {
			/* calculates gain compensation differently for
			 * modes with alpha > 1, so there's no drop in loudness
			 * and we can still drive the hard clipper when the input
			 * goes over 1.0
			 */
			softClipData_[i].gainCmp = 1.0f / (1.0f - (pow(softClipData_[i].alphaInv, 2.0f) / 3.0f));
		} else {
			softClipData_[i].gainCmp = 1.0f / softClipData_[i].alpha23;
		}
	}
} ;

AudioMixer::~AudioMixer() {
}

void AudioMixer::SetFileRenderer(const char *path) {
	renderPath_=path ;
} ;

void AudioMixer::EnableRendering(bool enable) {

	if (enable==enableRendering_) {
		return ;
	}

	if (enable) {
		writer_=new WavFileWriter(renderPath_.c_str()) ;
	} 

	enableRendering_=enable ;
	if (!enable) {
		writer_->Close() ;
		SAFE_DELETE(writer_) ;
	}
} ;

bool AudioMixer::Render(fixed *buffer,int samplecount) {
    clipped_ = false;

    fixed *mixBuffer = 0;
    bool gotData = false;
    IteratorPtr<AudioModule> it(GetIterator());
    for (it->Begin(); !it->IsDone(); it->Next()) {
        AudioModule &current = it->CurrentItem();
        if (!gotData) {
            gotData=current.Render(buffer,samplecount) ;           
         } else {
            if (!mixBuffer) {
               mixBuffer=(fixed *)malloc(samplecount*2*sizeof(fixed)) ;
            } 
            if (current.Render(mixBuffer,samplecount)) {
               fixed *dst=buffer ;
               fixed *src=mixBuffer ;
               int count=samplecount*2 ;
               while (count--) {
                 *dst+=*src ;
                 dst++ ;
                 src++ ;
               }
            }
         }
     }

     // The mixer's insert effect (master EQ) on the sum
     if (gotData && insert_) {
         insert_->Process(buffer,samplecount) ;
     }

     //  Apply volume

     if (gotData) {
         fixed *c = buffer;
         float damp = pow((float)masterVolume_ / 100, 4.0f);

         if (volume_ != i2fp(1)) {
             for (int i = 0; i < samplecount * 2; i++) {
                 fixed v = fp_mul(*c, volume_);
                 *c++ = v;
             }
         }

         // Apply soft/hard clipping before recording
         c = buffer;
         int peak = 0;
         for (int i = 0; i < samplecount * 2; i++) {
             fixed sample = *c;
             sample = fl2fp(damp * fp2fl(hardClip(softClip(sample))));
             *c++ = sample;
             int absSample = sample < 0 ? -sample : sample;
             if (absSample > peak) {
                 peak = absSample;
             }
         }
         int percent = (int)(((long long)peak * 100) / MAX_POSITIVE_FIXED);
         if (percent > 100) {
             percent = 100;
         }
         peakPercent_ = percent;
         updateWaveform(buffer,samplecount,peak);
     } else if (peakPercent_ > 0) {
         peakPercent_ -= 8;
         if (peakPercent_ < 0) {
             peakPercent_ = 0;
         }
         for (int i=0; i<WAVEFORM_SIZE; i++) {
             waveform_[i]=(waveform_[i]*3)/4;
             waveformMin_[i]=(waveformMin_[i]*3)/4;
             waveformMax_[i]=(waveformMax_[i]*3)/4;
         }
     }
    if (enableRendering_&&writer_) {
		if (!gotData) {
			memset(buffer,0,samplecount*2*sizeof(fixed)) ;
		} ;
		writer_->AddBuffer(buffer,samplecount) ;
	}
    if (capture_) {
		if (!gotData) {
			memset(buffer,0,samplecount*2*sizeof(fixed)) ;
		}
		int n=samplecount<captureLeft_?samplecount:captureLeft_ ;
		capture_->AddBuffer(buffer,n) ;
		captureLeft_-=n ;
		if (captureLeft_<=0) {
			Trace::Log("RENDER","capture complete, %d frames",captureTotal_) ;
			capture_->Close() ;
			SAFE_DELETE(capture_) ;
		}
	}
     SAFE_FREE(mixBuffer) ;
     return gotData ;
} ;

void AudioMixer::SetVolume(fixed volume) { volume_ = volume; }

bool AudioMixer::StartCapture(const char *path,int frames) {
	if (capture_ || frames<=0) return false ;
	capture_=new WavFileWriter(path) ;
	captureLeft_=captureTotal_=frames ;
	return true ;
}

void AudioMixer::CancelCapture() {
	if (!capture_) return ;
	capture_->Close() ;
	SAFE_DELETE(capture_) ;
	captureLeft_=0 ;
}

int AudioMixer::CaptureProgress() {
	if (captureTotal_<=0) return 0 ;
	return ((captureTotal_-captureLeft_)*100)/captureTotal_ ;
}

void AudioMixer::SetSoftclip(int clip, int gain) {
    softclip_ = clip - 1;
	softclipGain_ = gain;
}

void AudioMixer::SetMasterVolume(int volume) {
	masterVolume_ = volume;
}

bool AudioMixer::Clipped() { return clipped_; }

int AudioMixer::GetPeakPercent() { return peakPercent_; }

int AudioMixer::GetWaveformSample(int index) {
    if (index<0 || index>=WAVEFORM_SIZE) {
        return 0;
    }
    return waveform_[index];
}

int AudioMixer::GetWaveformMin(int index) {
    if (index<0 || index>=WAVEFORM_SIZE) {
        return 0;
    }
    return waveformMin_[index];
}

int AudioMixer::GetWaveformMax(int index) {
    if (index<0 || index>=WAVEFORM_SIZE) {
        return 0;
    }
    return waveformMax_[index];
}

void AudioMixer::updateWaveform(fixed *buffer,int samplecount,int peak) {
    if (!buffer || samplecount<=0 || peak<=0) {
        return;
    }
    fixed minSample=0;
    fixed maxSample=0;
    for (int frame=0; frame<samplecount; frame++) {
        fixed left=buffer[frame*2];
        fixed right=buffer[frame*2+1];
        fixed mono=(left+right)/2;
        if (frame==0 || mono<minSample) {
            minSample=mono;
        }
        if (frame==0 || mono>maxSample) {
            maxSample=mono;
        }
    }

    fixed midpoint=(minSample+maxSample)/2;
    int sample=(int)(((long long)midpoint*100)/peak);
    int minValue=(int)(((long long)minSample*100)/peak);
    int maxValue=(int)(((long long)maxSample*100)/peak);
    if (sample>100) sample=100;
    if (sample<-100) sample=-100;
    if (minValue>100) minValue=100;
    if (minValue<-100) minValue=-100;
    if (maxValue>100) maxValue=100;
    if (maxValue<-100) maxValue=-100;

    for (int i=0; i<WAVEFORM_SIZE-1; i++) {
        waveform_[i]=waveform_[i+1];
        waveformMin_[i]=waveformMin_[i+1];
        waveformMax_[i]=waveformMax_[i+1];
    }
    waveform_[WAVEFORM_SIZE-1]=sample;
    waveformMin_[WAVEFORM_SIZE-1]=minValue;
    waveformMax_[WAVEFORM_SIZE-1]=maxValue;
}

fixed AudioMixer::hardClip(fixed sample) {
    if (sample > MAX_POSITIVE_FIXED || sample < MAX_NEGATIVE_FIXED) {
        clipped_ = true;
		return sample > 0 ? MAX_POSITIVE_FIXED : MAX_NEGATIVE_FIXED;
    }
    return sample;
}

/* Implements standard cubic algorithm
 * https://wiki.analog.com/resources/tools-software/sigmastudio/toolbox/nonlinearprocessors/standardcubic
 */
fixed AudioMixer::softClip(fixed sample) {
    if (softclip_ == -1 || sample == 0)
        return sample;

    float x;
    float sampleFloat = fp2fl(sample);
	float maxFloat = fp2fl(sampleFloat > 0 ? MAX_POSITIVE_FIXED : MAX_NEGATIVE_FIXED);
	SoftClipData* data = &softClipData_[softclip_];

    x = data->alphaInv * (sampleFloat / maxFloat);
    if (x > -1.0f && x < 1.0f) {
        sampleFloat = maxFloat * (data->alpha * (x - (pow(x, 3.0f) / 3.0f)));
    } else {
        sampleFloat = maxFloat * data->alpha23;
    }

    if (softclipGain_) {
        sampleFloat = sampleFloat * data->gainCmp;
    }

    return fl2fp(sampleFloat);
}
