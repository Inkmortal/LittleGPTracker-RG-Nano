#ifndef _VOICE_OUTPUT_H_
#define _VOICE_OUTPUT_H_

// The last stage of the synth and macro voices, shared: the runaway check,
// then level, the +-2 safety clamp, pan and the conversion to the mixer's
// fixed-point samples (and the copy for the send effects). On the device
// four frames at a time in NEON; the operations per sample are the ones of
// the plain loops (min/max for the clamp only differ on NaN, which the
// check has already cut off), so the output is the same.

#include "Application/Utils/fixed.h"
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

// The first sample that is NaN or runaway (|x| > 1e6, or a NaN level), n if
// none
static inline int voiceFirstBroken(const float *sig,const float *sigR,const float *lev,int n) {
	int k=0 ;
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
	const float32x4_t limit=vdupq_n_f32(1e6f) ;
	for (;k+4<=n;k+=4) {
		float32x4_t s=vld1q_f32(sig+k) ;
		float32x4_t r=vld1q_f32(sigR+k) ;
		float32x4_t l=vld1q_f32(lev+k) ;
		// |x| <= 1e6 is false for NaN too
		uint32x4_t ok=vandq_u32(vcleq_f32(vabsq_f32(s),limit),vcleq_f32(vabsq_f32(r),limit)) ;
		ok=vandq_u32(ok,vceqq_f32(l,l)) ;
		uint32x2_t both=vand_u32(vget_low_u32(ok),vget_high_u32(ok)) ;
		if ((vget_lane_u32(both,0)&vget_lane_u32(both,1))!=0xFFFFFFFFu) break ;
	}
#endif
	for (;k<n;k++) {
		float s=sig[k],r=sigR[k],l=lev[k] ;
		if (s!=s || s>1e6f || s<-1e6f || r!=r || r>1e6f || r<-1e6f || l!=l) return k ;
	}
	return n ;
}

// n frames: (sig, sigR) * lev * ampMod, clamped to +-2, times the pan
// gains, into out (interleaved fixed) and, when send is not null, send
// (interleaved float, the same values before the conversion)
static inline void voiceOutput(const float *sig,const float *sigR,const float *lev,float ampMod,
                               float gainL,float gainR,fixed *out,float *send,int n) {
	int k=0 ;
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
	const float32x4_t vampMod=vdupq_n_f32(ampMod) ;
	const float32x4_t two=vdupq_n_f32(2.0f) ;
	const float32x4_t minusTwo=vdupq_n_f32(-2.0f) ;
	const float32x4_t vgL=vdupq_n_f32(gainL) ;
	const float32x4_t vgR=vdupq_n_f32(gainR) ;
	const float32x4_t full=vdupq_n_f32(32767.0f) ;
	for (;k+4<=n;k+=4) {
		float32x4_t amp=vmulq_f32(vld1q_f32(lev+k),vampMod) ;
		float32x4_t s=vmulq_f32(vld1q_f32(sig+k),amp) ;
		float32x4_t r=vmulq_f32(vld1q_f32(sigR+k),amp) ;
		s=vmaxq_f32(vminq_f32(s,two),minusTwo) ;
		r=vmaxq_f32(vminq_f32(r,two),minusTwo) ;
		float32x4x2_t o ;
		o.val[0]=vmulq_f32(s,vgL) ;
		o.val[1]=vmulq_f32(r,vgR) ;
		int32x4x2_t f ;
		f.val[0]=vcvtq_n_s32_f32(vmulq_f32(o.val[0],full),FIXED_SHIFT) ;
		f.val[1]=vcvtq_n_s32_f32(vmulq_f32(o.val[1],full),FIXED_SHIFT) ;
		vst2q_s32(out+k*2,f) ;
		if (send) vst2q_f32(send+k*2,o) ;
	}
#endif
	for (;k<n;k++) {
		float amp=lev[k]*ampMod ;
		float s=sig[k]*amp ;
		float r=sigR[k]*amp ;
		if (s>2.0f) s=2.0f ;
		if (s<-2.0f) s=-2.0f ;
		if (r>2.0f) r=2.0f ;
		if (r<-2.0f) r=-2.0f ;
		float outL=s*gainL ;
		float outR=r*gainR ;
		out[k*2]=fl2fp(outL*32767.0f) ;
		out[k*2+1]=fl2fp(outR*32767.0f) ;
		if (send) {
			send[k*2]=outL ;
			send[k*2+1]=outR ;
		}
	}
}

#endif
