#include "System/Console/Trace.h"
#include "AudioMixer.h"
#include "AudioProfiler.h"
#include "System/System/System.h"
#include <math.h>
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#define MAX_POSITIVE_FIXED i2fp(32767)
#define MAX_NEGATIVE_FIXED i2fp(-32768)

AudioMixer::AudioMixer(const char *name):
	T_SimpleList<AudioModule>(false),
	enableRendering_(0),
	writer_(0),
	insertCount_(0),
	capture_(0),
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
        int slot = current.GetProfileSlot();
        if (!gotData) {
            AudioProfiler::Enter(slot);
            gotData=current.Render(buffer,samplecount) ;
            AudioProfiler::Leave(slot);
         } else {
            if (!mixBuffer) {
               mixBuffer=(fixed *)malloc(samplecount*2*sizeof(fixed)) ;
            }
            AudioProfiler::Enter(slot);
            bool rendered=current.Render(mixBuffer,samplecount) ;
            AudioProfiler::Leave(slot);
            if (rendered) {
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

     // The mixer's insert effects (master EQ, limiter) on the sum
     for (int i=0;i<insertCount_;i++) {
         int slot = inserts_[i]->GetProfileSlot();
         AudioProfiler::Enter(slot);
         if (gotData) {
             inserts_[i]->Process(buffer,samplecount) ;
         } else {
             inserts_[i]->Silence() ;
         }
         AudioProfiler::Leave(slot);
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

         // Apply soft/hard clipping before recording (and find the peak
         // for the meters). Runs on every bus and the master each buffer:
         // the constants are worked out once here, and a unity master
         // volume skips its multiply (and the float round trip with it).
         c = buffer;
         int peak = 0;
         const int count = samplecount * 2;
         const bool unity = (damp == 1.0f);
         // The meters' waveform: lowest and highest L+R of the buffer
         fixed sumMin = 0, sumMax = 0;
         if (softclip_ == -1 && unity) {
             // Hard clip only (every channel bus): clamp, peak and the
             // waveform's min/max in one pass
             clipScan(buffer, samplecount, peak, sumMin, sumMax);
         } else {
             SoftClipCurve curve;
             prepareSoftClip(curve);
             for (int i = 0; i < count; i++) {
                 fixed sample = hardClip(softClip(*c, curve));
                 if (!unity) {
                     sample = fl2fp(damp * fp2fl(sample));
                 }
                 *c++ = sample;
                 int absSample = sample < 0 ? -sample : sample;
                 if (absSample > peak) {
                     peak = absSample;
                 }
             }
             for (int frame = 0; frame < samplecount; frame++) {
                 fixed sum = buffer[frame * 2] + buffer[frame * 2 + 1];
                 if (frame == 0 || sum < sumMin) sumMin = sum;
                 if (frame == 0 || sum > sumMax) sumMax = sum;
             }
         }
         int percent = (int)(((long long)peak * 100) / MAX_POSITIVE_FIXED);
         if (percent > 100) {
             percent = 100;
         }
         peakPercent_ = percent;
         updateWaveform(sumMin, sumMax, samplecount, peak);
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

// Clamp to 16 bits, and find the peak and the lowest/highest L+R: one pass,
// four frames at a time in NEON on the device (this runs on every channel
// bus for every buffer, for the meters)
void AudioMixer::clipScan(fixed *buffer, int frames, int &peak, fixed &sumMin, fixed &sumMax) {
    int frame = 0;
    int pk = 0;
    fixed mn = 0, mx = 0;
    bool clip = false;
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
    if (frames >= 4) {
        const int32x4_t hi = vdupq_n_s32(MAX_POSITIVE_FIXED);
        const int32x4_t lo = vdupq_n_s32(MAX_NEGATIVE_FIXED);
        int32x4_t vpk = vdupq_n_s32(0);
        int32x4_t vmn = vdupq_n_s32(0x7FFFFFFF);
        int32x4_t vmx = vdupq_n_s32((int)0x80000000);
        uint32x4_t vclip = vdupq_n_u32(0);
        for (; frame + 4 <= frames; frame += 4) {
            int32x4x2_t v = vld2q_s32(buffer + frame * 2);
            int32x4_t l = vminq_s32(vmaxq_s32(v.val[0], lo), hi);
            int32x4_t r = vminq_s32(vmaxq_s32(v.val[1], lo), hi);
            vclip = vorrq_u32(vclip, vmvnq_u32(vceqq_s32(l, v.val[0])));
            vclip = vorrq_u32(vclip, vmvnq_u32(vceqq_s32(r, v.val[1])));
            v.val[0] = l;
            v.val[1] = r;
            vst2q_s32(buffer + frame * 2, v);
            vpk = vmaxq_s32(vpk, vmaxq_s32(vabsq_s32(l), vabsq_s32(r)));
            int32x4_t sum = vaddq_s32(l, r);
            vmn = vminq_s32(vmn, sum);
            vmx = vmaxq_s32(vmx, sum);
        }
        int32x2_t p2 = vpmax_s32(vget_low_s32(vpk), vget_high_s32(vpk));
        pk = vget_lane_s32(vpmax_s32(p2, p2), 0);
        int32x2_t n2 = vpmin_s32(vget_low_s32(vmn), vget_high_s32(vmn));
        mn = vget_lane_s32(vpmin_s32(n2, n2), 0);
        int32x2_t x2 = vpmax_s32(vget_low_s32(vmx), vget_high_s32(vmx));
        mx = vget_lane_s32(vpmax_s32(x2, x2), 0);
        uint32x2_t c2 = vorr_u32(vget_low_u32(vclip), vget_high_u32(vclip));
        clip = (vget_lane_u32(c2, 0) | vget_lane_u32(c2, 1)) != 0;
    }
#endif
    for (; frame < frames; frame++) {
        fixed l = buffer[frame * 2];
        fixed r = buffer[frame * 2 + 1];
        if (l > MAX_POSITIVE_FIXED) { l = MAX_POSITIVE_FIXED; clip = true; }
        else if (l < MAX_NEGATIVE_FIXED) { l = MAX_NEGATIVE_FIXED; clip = true; }
        if (r > MAX_POSITIVE_FIXED) { r = MAX_POSITIVE_FIXED; clip = true; }
        else if (r < MAX_NEGATIVE_FIXED) { r = MAX_NEGATIVE_FIXED; clip = true; }
        buffer[frame * 2] = l;
        buffer[frame * 2 + 1] = r;
        int al = l < 0 ? -l : l;
        int ar = r < 0 ? -r : r;
        if (al > pk) pk = al;
        if (ar > pk) pk = ar;
        fixed sum = l + r;
        if (frame == 0 || sum < mn) mn = sum;
        if (frame == 0 || sum > mx) mx = sum;
    }
    if (clip) clipped_ = true;
    peak = pk;
    sumMin = mn;
    sumMax = mx;
}

void AudioMixer::updateWaveform(fixed sumMin,fixed sumMax,int samplecount,int peak) {
    if (samplecount<=0 || peak<=0) {
        return;
    }
    // (L+R)/2 of the lowest / highest frame (halving keeps the order)
    fixed minSample=sumMin/2;
    fixed maxSample=sumMax/2;

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
void AudioMixer::prepareSoftClip(SoftClipCurve &curve) {
    curve.on = (softclip_ >= 0 && softclip_ < 4);
    SoftClipData *data = &softClipData_[curve.on ? softclip_ : 0];
    curve.posMax = fp2fl(MAX_POSITIVE_FIXED);
    curve.negMax = fp2fl(MAX_NEGATIVE_FIXED);
    curve.posScale = data->alphaInv / curve.posMax;
    curve.negScale = data->alphaInv / curve.negMax;
    curve.alpha = data->alpha;
    curve.alpha23 = data->alpha23;
    curve.gain = softclipGain_ ? data->gainCmp : 1.0f;
}

/* Implements standard cubic algorithm (the multiplies by reciprocals and
 * x*x*x instead of a division and powf per sample: the same curve)
 */
fixed AudioMixer::softClip(fixed sample, const SoftClipCurve &curve) {
    if (!curve.on || sample == 0)
        return sample;

    float sampleFloat = fp2fl(sample);
    bool positive = sampleFloat > 0;
    float maxFloat = positive ? curve.posMax : curve.negMax;
    float x = sampleFloat * (positive ? curve.posScale : curve.negScale);
    if (x > -1.0f && x < 1.0f) {
        sampleFloat = maxFloat * (curve.alpha * (x - x * x * x * (1.0f / 3.0f)));
    } else {
        sampleFloat = maxFloat * curve.alpha23;
    }
    sampleFloat = sampleFloat * curve.gain;

    return fl2fp(sampleFloat);
}
