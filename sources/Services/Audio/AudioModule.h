#ifndef _AUDIO_MODULE_H_
#define _AUDIO_MODULE_H_

#include "Application/Utils/fixed.h"

class AudioModule {
public:
      AudioModule():profileSlot_(-1) {} ;
      virtual ~AudioModule() {} ;
      virtual bool Render(fixed *buffer,int samplecount)=0 ;
      // What the audio profiler charges this module's render to (an
      // AudioProfileSlot), -1 = whoever renders it
      void SetProfileSlot(int slot) { profileSlot_=slot ; } ;
      int GetProfileSlot() { return profileSlot_ ; } ;
private:
      int profileSlot_ ;
} ;

#endif
