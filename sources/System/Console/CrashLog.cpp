#include "CrashLog.h"
#include "Trace.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define crashWrite(fd, buf, n) _write(fd, buf, (unsigned)(n))
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#define crashWrite(fd, buf, n) write(fd, buf, n)
#endif

#define CRASHLOG_ENTRIES 48
#define CRASHLOG_WIDTH 96
#define HEARTBEAT_SECONDS 30

static char entries_[CRASHLOG_ENTRIES][CRASHLOG_WIDTH];
static volatile unsigned int next_ = 0;
static time_t start_ = time(0);
static char build_[48] = "unknown";

void CrashLog::SetBuild(const char *build) {
	strncpy(build_, build, sizeof(build_) - 1);
}

unsigned long CrashLog::UptimeSeconds() {
	return (unsigned long)(time(0) - start_);
}

void CrashLog::Note(const char *fmt, ...) {
	char text[CRASHLOG_WIDTH - 12];
	va_list args;
	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	// Called from the UI and audio threads: each caller gets its own slot
	unsigned int slot = __sync_fetch_and_add(&next_, 1) % CRASHLOG_ENTRIES;
	snprintf(entries_[slot], CRASHLOG_WIDTH, "%6lus %s", UptimeSeconds(), text);
	Trace::Log("TRAIL", "%s", text);
}

int CrashLog::MemoryKB() {
#ifdef _WIN32
	// K32GetProcessMemoryInfo lives in kernel32 (no psapi link needed)
	typedef struct {
		DWORD cb, PageFaultCount;
		SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage,
		    QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage,
		    PagefileUsage, PeakPagefileUsage;
	} Counters;
	typedef BOOL(WINAPI * Fn)(HANDLE, Counters *, DWORD);
	static Fn fn = (Fn)GetProcAddress(GetModuleHandleA("kernel32"), "K32GetProcessMemoryInfo");
	Counters c;
	c.cb = sizeof(c);
	if (fn && fn(GetCurrentProcess(), &c, sizeof(c))) {
		return (int)(c.PagefileUsage / 1024);  // private bytes: what leaks grow
	}
	return -1;
#else
	FILE *f = fopen("/proc/self/statm", "r");
	if (!f) return -1;
	long size = 0, resident = 0;
	int n = fscanf(f, "%ld %ld", &size, &resident);
	fclose(f);
	if (n != 2) return -1;
	return (int)(resident * (sysconf(_SC_PAGESIZE) / 1024));
#endif
}

static bool heartbeatStarted = false;
static unsigned long heartbeatLast = 0;

bool CrashLog::HeartbeatDue() {
	return !heartbeatStarted || UptimeSeconds() - heartbeatLast >= HEARTBEAT_SECONDS;
}

bool CrashLog::Heartbeat(const char *state) {
	unsigned long now = UptimeSeconds();
	if (heartbeatStarted && now - heartbeatLast < HEARTBEAT_SECONDS) return false;
	heartbeatStarted = true;
	heartbeatLast = now;
	Trace::Log("HEARTBEAT", "up %lus mem %dKB %s", now, MemoryKB(), state ? state : "");
	return true;
}

static void writeText(int fd, const char *text) {
	crashWrite(fd, text, strlen(text));
}

void CrashLog::Dump(int fd) {
	writeText(fd, "last actions (oldest first):\n");
	unsigned int end = next_;
	unsigned int count = end < CRASHLOG_ENTRIES ? end : CRASHLOG_ENTRIES;
	for (unsigned int i = end - count; i != end; i++) {
		const char *e = entries_[i % CRASHLOG_ENTRIES];
		if (!e[0]) continue;
		writeText(fd, "  ");
		writeText(fd, e);
		writeText(fd, "\n");
	}
}

#if !defined(_WIN32)
#include <ucontext.h>

extern "C" char __executable_start;
extern "C" char etext;

static char crashPath_[256];

static void writeHex(int fd, const char *label, unsigned long v) {
	char buf[40];
	static const char *digits = "0123456789abcdef";
	int n = 0;
	for (const char *p = label; *p && n < 20; p++) buf[n++] = *p;
	buf[n++] = '0';
	buf[n++] = 'x';
	for (int shift = (int)sizeof(v) * 8 - 4; shift >= 0; shift -= 4) {
		buf[n++] = digits[(v >> shift) & 0xF];
	}
	buf[n++] = '\n';
	crashWrite(fd, buf, n);
}

static void writeNumber(int fd, const char *label, unsigned long v) {
	char buf[40];
	char digits[20];
	int d = 0;
	do {
		digits[d++] = '0' + (v % 10);
		v /= 10;
	} while (v && d < 20);
	int n = 0;
	for (const char *p = label; *p && n < 20; p++) buf[n++] = *p;
	while (d) buf[n++] = digits[--d];
	buf[n++] = '\n';
	crashWrite(fd, buf, n);
}

static void onCrash(int sig, siginfo_t *info, void *context) {
	int fd = open(crashPath_, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd >= 0) {
		writeText(fd, "=== CRASH ===\nbuild ");
		writeText(fd, build_);
		writeText(fd, "\n");
		writeNumber(fd, "signal ", (unsigned long)sig);
		writeNumber(fd, "uptime s ", CrashLog::UptimeSeconds());
		writeHex(fd, "fault addr ", (unsigned long)(info ? info->si_addr : 0));
		unsigned long *sp = 0;
#if defined(__arm__)
		ucontext_t *uc = (ucontext_t *)context;
		writeHex(fd, "pc ", uc->uc_mcontext.arm_pc);
		writeHex(fd, "lr ", uc->uc_mcontext.arm_lr);
		writeHex(fd, "sp ", uc->uc_mcontext.arm_sp);
		sp = (unsigned long *)uc->uc_mcontext.arm_sp;
#endif
		// Return addresses left on the stack: the likely callers
		if (sp) {
			unsigned long lo = (unsigned long)&__executable_start;
			unsigned long hi = (unsigned long)&etext;
			writeText(fd, "stack code addresses:\n");
			int found = 0;
			for (int i = 0; i < 512 && found < 24; i++) {
				unsigned long v = sp[i];
				if (v >= lo && v < hi) {
					writeHex(fd, "  ", v);
					found++;
				}
			}
		}
		CrashLog::Dump(fd);
		close(fd);
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

void CrashLog::InstallSignalHandlers(const char *path) {
	strncpy(crashPath_, path, sizeof(crashPath_) - 1);
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = onCrash;
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset(&sa.sa_mask);
	int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
	for (unsigned int i = 0; i < sizeof(signals) / sizeof(int); i++) {
		sigaction(signals[i], &sa, 0);
	}
}
#else
void CrashLog::InstallSignalHandlers(const char *) {}
#endif
