#ifndef _CRASH_LOG_H_
#define _CRASH_LOG_H_

// What the app was doing just before a crash. Note() keeps the last few
// dozen actions (keys, screens, dialogs, play/stop) in memory and mirrors
// them to the log; a crash handler writes them out with Dump(). Heartbeat()
// logs uptime and memory use every 30 s so slow growth shows in the log.
class CrashLog {
public:
	static void Note(const char *fmt, ...);
	// Returns true when it wrote a line (every 30 s)
	static bool Heartbeat(const char *state);
	// True when the next Heartbeat() call will write a line
	static bool HeartbeatDue();
	// Async-signal-safe: plain write() calls, no allocation
	static void Dump(int fd);
	// Resident memory of the process in KB, -1 if unknown
	static int MemoryKB();
	static unsigned long UptimeSeconds();
	// Device: SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT write a report to path
	static void InstallSignalHandlers(const char *path);
	// Which build crashed (the commit, so the report can be symbolized)
	static void SetBuild(const char *build);
};

#endif
