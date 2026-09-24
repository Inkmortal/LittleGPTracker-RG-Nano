// The device crash reporter, built for the device and run under qemu-arm:
// a child process notes a few actions and dereferences null; the report it
// leaves must name the signal, the program counter, the stack and the
// last actions.
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
#include "System/Console/CrashLog.h"
#include "System/Console/Logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static int failures=0;

static void expect(bool ok,const char *what) {
	printf("%-44s %s\n",what,ok?"ok":"WRONG");
	fflush(stdout);
	if (!ok) failures++;
}

static char text[8192];

int main() {
	const char *path="crash_check_report.txt";
	unlink(path);
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	fflush(stdout);
	pid_t child=fork();
	if (child==0) {
		CrashLog::InstallSignalHandlers(path);
		CrashLog::SetBuild("test123");
		CrashLog::Note("keys 0001 view 2");
		CrashLog::Note("screen 4");
		volatile int *nowhere=0;
		*nowhere=1;
		_exit(0);
	}
	int status=0;
	waitpid(child,&status,0);
	expect(WIFSIGNALED(status) && WTERMSIG(status)==11,"child still dies with SIGSEGV");
	// Plain syscalls: the app's headers can redefine fopen to its FileSystem
	int fd=open(path,O_RDONLY);
	int n=fd>=0?read(fd,text,sizeof(text)-1):0;
	if (n<0) n=0;
	text[n]=0;
	if (fd>=0) close(fd);
	printf("%s",text);
	expect(strstr(text,"=== CRASH ===")!=0,"report written");
	expect(strstr(text,"build test123")!=0,"names the build");
	expect(strstr(text,"signal 11")!=0,"names the signal");
	expect(strstr(text,"pc 0x")!=0,"has the program counter");
	expect(strstr(text,"stack code addresses")!=0,"has stack addresses");
	expect(strstr(text,"screen 4")!=0,"has the last actions");
	unlink(path);
	printf("%s\n",failures?"FAILED":"ALL OK");
	fflush(stdout);
	// Skip global destructors: the app objects linked in are not set up here
	_exit(failures?1:0);
}
