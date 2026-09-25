#ifndef _I_SRP_UPDATER_H_
#define _I_SRP_UPDATER_H_

#include "Application/Utils/fixed.h"

// Destinations only the MOD slots move, in parameter units: drive (0..255),
// crush (bits), shape (0..1), FM (0..255), noise (0..1), sample start and
// loop start (fraction of the trimmed sample), sends (0..1)
enum RUExtra {
	RUX_DRIVE=0,
	RUX_CRUSH,
	RUX_SHAPE,
	RUX_FM,
	RUX_NOISE,
	RUX_START,
	RUX_LOOP,
	RUX_REVERB,
	RUX_DELAY,
	RUX_CHORUS,
	RUX_TIMBRE,   // macro synth timbre / color (0..1)
	RUX_COLOR,
	RUX_LAST
} ;

struct RUParams {
	fixed volumeOffset_ ;
	fixed speedOffset_ ;
	fixed cutOffset_ ;
	fixed resOffset_ ;
	fixed panOffset_ ;
	fixed fbMixOffset_ ;
	fixed fbTunOffset_ ;
	float volumeScale_ ;        // envelope depth on volume, 1 = untouched
	float extra_[RUX_LAST] ;
	void Reset() {
		volumeOffset_=cutOffset_=resOffset_=panOffset_=0 ;
		fbMixOffset_=fbTunOffset_=0 ;
		speedOffset_=FP_ONE ;
		volumeScale_=1.0f ;
		for (int i=0;i<RUX_LAST;i++) extra_[i]=0.0f ;
	}
} ;

class I_SRPUpdater {
public:
	I_SRPUpdater() {} ;
	virtual ~I_SRPUpdater() {} ;
	virtual void Trigger(bool tableTick)=0 ;
	virtual void UpdateSRP(struct RUParams &rup)=0 ;
	void Enable() { enabled_=true ;} ;
	void Disable() { enabled_=false ;} ;
	bool Enabled() { return enabled_ ; } ;
protected:
	bool enabled_ ;
} ;
#endif
