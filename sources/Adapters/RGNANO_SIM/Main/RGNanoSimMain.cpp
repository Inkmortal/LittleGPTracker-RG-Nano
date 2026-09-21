#define SDL_MAIN_HANDLED
#include "Application/Application.h"
#include "Adapters/RGNANO_SIM/System/RGNanoSimSystem.h"
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"

#include <windows.h>
#include <stdio.h>

#ifdef main
#undef main
#endif

// Crash reporter: writes the faulting address and the frame-pointer chain to
// rgnano-sim-crash.txt so tools/symbolize_crash.py can turn it into
// function:line with addr2line. Agents cannot attach a debugger here.
static LONG WINAPI simCrashHandler(EXCEPTION_POINTERS *info)
{
	FILE *out=fopen("rgnano-sim-crash.txt","w");
	FILE *targets[2]={out,stderr};
	HMODULE base=GetModuleHandle(NULL);
	for (int t=0;t<2;t++) {
		FILE *f=targets[t];
		if (!f) continue;
		fprintf(f,"RGNANO_SIM_CRASH code=0x%08lX address=%p base=%p\n",
		        info->ExceptionRecord->ExceptionCode,
		        info->ExceptionRecord->ExceptionAddress,(void *)base);
#ifdef _X86_
		CONTEXT *ctx=info->ContextRecord;
		fprintf(f,"frame 0 %p\n",(void *)ctx->Eip);
		DWORD *frame=(DWORD *)ctx->Ebp;
		for (int i=1;i<40 && frame;i++) {
			if (IsBadReadPtr(frame,2*sizeof(DWORD))) break;
			DWORD ret=frame[1];
			if (!ret) break;
			fprintf(f,"frame %d %p\n",i,(void *)ret);
			DWORD *next=(DWORD *)frame[0];
			if (next<=frame) break;
			frame=next;
		}
#endif
		fflush(f);
	}
	if (out) fclose(out);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc,char *argv[])
{
	SetUnhandledExceptionFilter(simCrashHandler) ;
	RGNanoSimSystem::Boot(argc,argv) ;

	SDLCreateWindowParams params ;
	params.title="LittleGPTracker RG Nano Simulator" ;
	params.cacheFonts_=true ;
	params.framebuffer_=false ;

	Application::GetInstance()->Init(params) ;

	int retval=RGNanoSimSystem::MainLoop() ;
	RGNanoSimSystem::Shutdown() ;
	return retval ;
}
