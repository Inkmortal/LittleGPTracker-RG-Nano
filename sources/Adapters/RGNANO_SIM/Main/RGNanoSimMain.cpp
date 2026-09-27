#define SDL_MAIN_HANDLED
#include "System/Console/CrashLog.h"
#include <io.h>
#include "Application/Application.h"
#include "Adapters/RGNANO_SIM/System/RGNanoSimSystem.h"
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

// Crash reporter: writes the fault, the frame-pointer chain and every
// return address found on the stack (the build omits frame pointers, so the
// scan is what finds the callers) to rgnano-sim-crash.txt next to the exe,
// plus the recent-actions trail. tools/symbolize_crash.py names them.
// A vectored handler sees faults on every thread (audio included), before
// any other handler can swallow them.
static volatile LONG crashing_=0 ;
// Set when the hang watchdog, not a fault, wrote the report
static char hangLine_[96]="" ;

static void writeCrash(EXCEPTION_POINTERS *info)
{
	char path[MAX_PATH] ;
	DWORD n=GetModuleFileNameA(NULL,path,MAX_PATH) ;
	while (n>0 && path[n-1]!='\\' && path[n-1]!='/') n-- ;
	path[n]=0 ;
	strcat(path,"rgnano-sim-crash.txt") ;
	FILE *out=fopen(path,"w") ;
	FILE *targets[2]={out,stderr} ;
	HMODULE base=GetModuleHandle(NULL) ;
	IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)base ;
	IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS *)((char *)base+dos->e_lfanew) ;
	DWORD lo=(DWORD)base,hi=lo+nt->OptionalHeader.SizeOfImage ;
	for (int t=0;t<2;t++) {
		FILE *f=targets[t] ;
		if (!f) continue ;
		fprintf(f,"RGNANO_SIM_CRASH code=0x%08lX address=%p base=%p thread=%lu\n",
		        info->ExceptionRecord->ExceptionCode,
		        info->ExceptionRecord->ExceptionAddress,(void *)base,GetCurrentThreadId()) ;
		if (hangLine_[0]) {
			fprintf(f,"%s\n",hangLine_) ;
		}
#ifdef _X86_
		CONTEXT *ctx=info->ContextRecord ;
		fprintf(f,"frame 0 %p\n",(void *)ctx->Eip) ;
		DWORD *frame=(DWORD *)ctx->Ebp ;
		for (int i=1;i<40 && frame;i++) {
			if (IsBadReadPtr(frame,2*sizeof(DWORD))) break ;
			DWORD ret=frame[1] ;
			if (!ret) break ;
			fprintf(f,"frame %d %p\n",i,(void *)ret) ;
			DWORD *next=(DWORD *)frame[0] ;
			if (next<=frame) break ;
			frame=next ;
		}
		DWORD *sp=(DWORD *)ctx->Esp ;
		int found=0 ;
		for (int i=0;i<2048 && found<30;i++) {
			if (IsBadReadPtr(sp+i,sizeof(DWORD))) break ;
			DWORD v=sp[i] ;
			if (v>=lo && v<hi) {
				fprintf(f,"frame s%d %p\n",found,(void *)v) ;
				found++ ;
			}
		}
#endif
		fflush(f) ;
	}
	if (out) {
		CrashLog::Dump(_fileno(out)) ;
		fclose(out) ;
	}
}

static LONG WINAPI simVectoredHandler(EXCEPTION_POINTERS *info)
{
	switch (info->ExceptionRecord->ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_STACK_OVERFLOW:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			if (InterlockedExchange(&crashing_,1)==0) {
				writeCrash(info) ;
			}
			break ;
		default:
			break ;
	}
	return EXCEPTION_CONTINUE_SEARCH ;
}

static LONG WINAPI simCrashHandler(EXCEPTION_POINTERS *info)
{
	if (InterlockedExchange(&crashing_,1)==0) {
		writeCrash(info) ;
	}
	return EXCEPTION_EXECUTE_HANDLER ;
}

// The hang watchdog found a thread stuck in one key press: freeze it, write
// its registers and stack like a crash, and exit so a scripted run fails
// instead of hanging forever
static void simHangHandler(unsigned long threadId,const char *what,unsigned int seconds)
{
	snprintf(hangLine_,sizeof(hangLine_),"RGNANO_SIM_HANG stuck %us in %s",seconds,what) ;
	HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT,FALSE,(DWORD)threadId) ;
	CONTEXT ctx ;
	memset(&ctx,0,sizeof(ctx)) ;
	ctx.ContextFlags=CONTEXT_FULL ;
	if (thread) {
		SuspendThread(thread) ;
		GetThreadContext(thread,&ctx) ;
	}
	EXCEPTION_RECORD record ;
	memset(&record,0,sizeof(record)) ;
	record.ExceptionCode=0xE0000001 ;
#ifdef _X86_
	record.ExceptionAddress=(void *)ctx.Eip ;
#endif
	EXCEPTION_POINTERS info={&record,&ctx} ;
	if (InterlockedExchange(&crashing_,1)==0) {
		writeCrash(&info) ;
	}
	// No Trace here: the stuck thread may hold the logger
	TerminateProcess(GetCurrentProcess(),3) ;
}

int main(int argc,char *argv[])
{
	AddVectoredExceptionHandler(1,simVectoredHandler) ;
	SetUnhandledExceptionFilter(simCrashHandler) ;
	RGNanoSimSystem::Boot(argc,argv) ;
	CrashLog::SetHangHandler(simHangHandler) ;
	CrashLog::StartWatchdog(HANG_LIMIT_SECONDS) ;

	SDLCreateWindowParams params ;
	params.title="LittleGPTracker RG Nano Simulator" ;
	params.cacheFonts_=true ;
	params.framebuffer_=false ;

	Application::GetInstance()->Init(params) ;

	int retval=RGNanoSimSystem::MainLoop() ;
	RGNanoSimSystem::Shutdown() ;
	return retval ;
}
