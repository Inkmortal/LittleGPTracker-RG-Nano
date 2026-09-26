// harness: a7cost
// The whole audio engine on the device build, under qemu-arm: the demo song
// "Engine Room" (every heavy engine on all 8 tracks, sends, master EQ and
// limiter) loads from its song file and plays through the real player and
// mixer exactly as on the RG Nano, one buffer at a time.
//
//  - cost per part of the mix (each track's instrument, channel buses, send
//    effects, master EQ, limiter, master mix, sequencer), from the a7cost
//    qemu plugin: instructions weighted by what they cost on the Cortex-A7
//    (deterministic; qemu's wall time is noisy and skewed by its softfloat)
//  - the same parts in wall time (median of several passes) for comparison,
//    with a fixed reference workload to express it as a ratio
//  - the estimated device load: cost per second of audio against the A7's
//    1.2 GHz, as the busiest ~20 buffers average (what the app's load meter
//    and the log's "load peak" show)
//  - the mix is written to buildRGNANO/harness/engine_room.wav (compare it
//    with tools/dsp-harness/compare_wav.py)
//  - regression guard: the song's cost per second must stay within 25% of
//    the figure recorded below
// Build + run: wsl bash tools/dsp-harness/run.sh <this file>
// Environment: ENGINE_ROOM_SECONDS (default 19.2 = two rows of the song),
// ENGINE_ROOM_WAV (output path), ENGINE_ROOM_PASSES (wall-time passes, 3)
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "Adapters/Dummy/Midi/DummyMidi.h"
#include "Adapters/SDL/Process/SDLProcess.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Model/Project.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Player/Player.h"
#include "Application/Player/TablePlayback.h"
#include "Application/Views/ViewData.h"
#include "Foundation/Variables/WatchedVariable.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioDriver.h"
#include "Services/Audio/AudioOutDriver.h"
#include "Services/Audio/AudioProfiler.h"
#include "Services/Midi/MidiService.h"
#include "System/Console/Logger.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Process/Process.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

// Recorded cost of the song (a7cost units per second of audio) and the
// regression margin. Update the figure when the engine gets cheaper.
static const double RECORDED_COST_PER_SECOND = 233660123.0;
static const double REGRESSION_MARGIN = 1.25;
// The same song before the engine was optimised (commit 1755be2), which
// the device measured at 75% average / 95% peak load
static const double BEFORE_COST_PER_SECOND = 404421574.0;
// The device: Cortex-A7 at 1.2 GHz, 44.1 kHz output
static const double DEVICE_HZ = 1.2e9;
static const double RATE = 44100.0;

#define SYS_MARK 0x7AB0
#define SYS_SNAPSHOT 0x7AB1
#define SYS_RESET 0x7AB2
#define SYS_SLOTS 0x7AB3

// ENGINE_ROOM_WORST=N: the per-part costs of every buffer of the first
// pass are kept, and the N most expensive buffers are shown part by part
static std::vector<std::vector<double> > gBufferSlots;
static bool gKeepBufferSlots = false;

static int failures = 0;

static void expect(bool ok, const char *what, double got, double want) {
	printf("%-52s got=%12.4f want=%12.4f %s\n", what, got, want, ok ? "ok" : "WRONG");
	fflush(stdout);
	if (!ok) failures++;
}

// Log lines (sample loading, the player...) would drown the report
class QuietLogger : public Trace::Logger {
	virtual void AddLine(const char *line) {
		if (strstr(line, "rror") || getenv("ENGINE_ROOM_LOG")) printf("log: %s\n", line);
	}
};

// The device's audio driver without a device: buffers are asked for by
// Pull() and handed back instead of played
class HarnessDriver : public AudioDriver {
public:
	HarnessDriver(AudioSettings &s) : AudioDriver(s), frames_(0) {}
	virtual bool InitDriver() { return true; }
	virtual void CloseDriver() {}
	virtual bool StartDriver() { return true; }
	virtual void StopDriver() {}
	virtual bool Interlaced() { return true; }
	virtual int GetPlayedBufferPercentage() { return GetRenderLoadPercent(); }
	virtual double GetStreamTime() { return frames_ / RATE; }

	// One buffer, as the SDL driver's render thread asks for it; the
	// interleaved 16-bit frames are appended to out. Returns frames.
	int Pull(std::vector<short> *out) {
		OnNewBufferNeeded();
		int frames = 0;
		for (int i = 0; i < SOUND_BUFFER_COUNT; i++) {
			if (!pool_[i].buffer_) continue;
			int n = pool_[i].size_ / 4;
			if (out) {
				short *s = (short *)pool_[i].buffer_;
				out->insert(out->end(), s, s + n * 2);
			}
			frames += n;
			SYS_FREE(pool_[i].buffer_);
			pool_[i].buffer_ = 0;
		}
		poolQueuePosition_ = 0;
		frames_ += frames;
		return frames;
	}

private:
	double frames_;
};

static HarnessDriver *gDriver = 0;

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {
		gDriver = new HarnessDriver(settings_);
		Insert(new AudioOutDriver(*gDriver));
	}
	virtual void Close() {}
};

/* a7cost plugin interface */

static void a7Mark(int slot) { syscall(SYS_MARK, slot); }

struct A7Snapshot {
	bool valid;
	double slotCost[64];
	double slotInsn[64];
	std::vector<double> buffers;
};

static bool a7Read(A7Snapshot &snap, bool withBlocks = true) {
	snap.valid = false;
	memset(snap.slotCost, 0, sizeof(snap.slotCost));
	memset(snap.slotInsn, 0, sizeof(snap.slotInsn));
	snap.buffers.clear();
	const char *path = getenv("A7COST_OUT");
	if (!path) return false;
	unlink(path);
	syscall(withBlocks ? SYS_SNAPSHOT : SYS_SLOTS);
	int fd = open(path, O_RDONLY);
	if (fd < 0) return false;
	std::string text;
	char chunk[4096];
	int got;
	while ((got = read(fd, chunk, sizeof(chunk))) > 0) text.append(chunk, got);
	close(fd);
	size_t pos = 0;
	while (pos < text.size()) {
		size_t nl = text.find('\n', pos);
		if (nl == std::string::npos) nl = text.size();
		std::string lineText = text.substr(pos, nl - pos);
		pos = nl + 1;
		const char *line = lineText.c_str();
		int slot;
		unsigned long long cost, insn;
		if (sscanf(line, "slot %d %llu %llu", &slot, &cost, &insn) == 3 && slot >= 0 &&
		    slot < 64) {
			snap.slotCost[slot] = (double)cost;
			snap.slotInsn[slot] = (double)insn;
		} else if (sscanf(line, "buf %llu", &cost) == 1) {
			snap.buffers.push_back((double)cost);
		}
	}
	snap.valid = true;
	return true;
}

/* A fixed workload to express wall times as a ratio (qemu's speed varies
   from run to run and machine to machine): a float filter and integer
   mixing, like the engine's inner loops */
static float baselineData[4096];
__attribute__((noinline)) static float baselineWorkload() {
	float z1 = 0.0f, z2 = 0.0f, acc = 0.0f;
	int iacc = 0;
	for (int pass = 0; pass < 200; pass++) {
		for (int i = 0; i < 4096; i++) {
			float x = baselineData[i];
			float y = 0.2f * x + z1;
			z1 = 0.3f * x - 0.5f * y + z2;
			z2 = 0.1f * x - 0.25f * y;
			acc += y;
			iacc += (int)(y * 32767.0f) >> 3;
		}
	}
	return acc + iacc * 1e-9f;
}

static double median(std::vector<double> v) {
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	size_t n = v.size();
	return (n & 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

static bool isDir(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Plain system calls: the app's headers redefine fopen to its FileSystem
static void put32(std::vector<char> &b, unsigned int v) {
	for (int i = 0; i < 4; i++) b.push_back((char)((v >> (8 * i)) & 0xFF));
}
static void put16(std::vector<char> &b, unsigned int v) {
	b.push_back((char)(v & 0xFF));
	b.push_back((char)((v >> 8) & 0xFF));
}

static void writeWav(const char *path, const std::vector<short> &data) {
	unsigned int bytes = data.size() * 2;
	std::vector<char> h;
	h.insert(h.end(), "RIFF", "RIFF" + 4);
	put32(h, 36 + bytes);
	h.insert(h.end(), "WAVEfmt ", "WAVEfmt " + 8);
	put32(h, 16);
	put16(h, 1);
	put16(h, 2);
	put32(h, 44100);
	put32(h, 44100 * 4);
	put16(h, 4);
	put16(h, 16);
	h.insert(h.end(), "data", "data" + 4);
	put32(h, bytes);
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 || write(fd, &h[0], h.size()) != (ssize_t)h.size() ||
	    write(fd, &data[0], bytes) != (ssize_t)bytes) {
		printf("could not write %s\n", path);
		failures++;
	}
	if (fd >= 0) close(fd);
}

// Plays the song from the top for 'seconds'; frames of each buffer go to
// bufferFrames, the audio to out (when given)
static void playSong(Player *player, double seconds, std::vector<int> &bufferFrames,
                     std::vector<short> *out) {
	player->Start(PM_SONG, false);
	long total = (long)(seconds * RATE);
	long done = 0;
	bufferFrames.clear();
	while (done < total) {
		int n = gDriver->Pull(out);
		if (n <= 0) {
			printf("the mixer rendered nothing\n");
			failures++;
			break;
		}
		bufferFrames.push_back(n);
		done += n;
		if (gKeepBufferSlots) {
			A7Snapshot b;
			a7Read(b, false);
			gBufferSlots.push_back(std::vector<double>(b.slotCost, b.slotCost + APS_COUNT));
		}
		if (getenv("ENGINE_ROOM_LOG") && bufferFrames.size() % 50 == 1) {
			printf("buffer %d frames %d playing", (int)bufferFrames.size(), n);
			for (int c = 0; c < 8; c++) printf(" %d", player->IsChannelPlaying(c) ? 1 : 0);
			printf("\n");
		}
	}
	player->Stop();
	// let the stop settle (fades, tails) without measuring it
	for (int i = 0; i < 4; i++) gDriver->Pull(0);
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new QuietLogger()));
	MidiService::Install(new DummyMidi());
	SysProcessFactory::Install(new SDLProcessFactory());
	AudioSettings settings;
	settings.bufferSize_ = 1024;
	settings.preBufferCount_ = 0;
	Audio::Install(new HarnessAudio(settings));
	Audio::GetInstance()->Init();

	const char *root = isDir("resources/demos/lgpt_EngineRoom") ? "." : "projects";
	std::string demo = std::string(root) + "/resources/demos/lgpt_EngineRoom";
	if (!isDir(demo.c_str())) {
		printf("demo song not found (run from the worktree or projects/)\n");
		return 1;
	}
	std::string outDir = std::string(root) + "/buildRGNANO/harness";
	const char *wavEnv = getenv("ENGINE_ROOM_WAV");
	std::string wavPath = wavEnv ? wavEnv : outDir + "/engine_room.wav";
	double seconds = getenv("ENGINE_ROOM_SECONDS") ? atof(getenv("ENGINE_ROOM_SECONDS")) : 19.2;
	int passes = getenv("ENGINE_ROOM_PASSES") ? atoi(getenv("ENGINE_ROOM_PASSES")) : 3;

	// Load the song as AppWindow::LoadProject does
	Path::SetAlias("bin", ".");
	Path::SetAlias("root", ".");
	Path::SetAlias("project", demo.c_str());
	Path::SetAlias("samples", "project:samples");
	// The service first (Application::Init): song parts register with it
	// as they are created
	PersistencyService *persist = PersistencyService::GetInstance();
	TablePlayback::Reset();
	SamplePool::GetInstance()->Load();
	Project *project = new Project();
	bool loaded = persist->Load();
	expect(project->GetTempo() == 100, "song tempo read from the file", project->GetTempo(), 100);
	expect(loaded, "Engine Room song file loads", loaded, 1);
	WatchedVariable::Disable();
	project->GetInstrumentBank()->Init();
	WatchedVariable::Enable();
	ViewData *viewData = new ViewData(project);
	Player *player = Player::GetInstance();
	bool started = player->Init(project, viewData);
	expect(started, "player and mixer start", started, 1);

	bool a7 = getenv("A7COST_OUT") != 0;
	printf("cost model: %s\n", a7 ? "a7cost plugin (weighted instructions)" :
	                                "NONE - plain qemu, wall time only");

	/* Pass 1: the reference render, charged per part by the plugin */
	AudioProfiler::Enable(true);
	if (a7) {
		AudioProfiler::SetMarkHook(a7Mark);
		syscall(SYS_RESET);
	}
	std::vector<short> audio;
	std::vector<int> frames;
	int worstCount = getenv("ENGINE_ROOM_WORST") ? atoi(getenv("ENGINE_ROOM_WORST")) : 0;
	gKeepBufferSlots = a7 && worstCount > 0;
	playSong(player, seconds, frames, &audio);
	gKeepBufferSlots = false;
	A7Snapshot snap;
	a7Read(snap);
	if (a7) {
		// the cost per code block of this pass, for tools/dsp-harness/a7profile.py
		std::string blocks = std::string(getenv("A7COST_OUT")) + ".blocks";
		rename(blocks.c_str(), (outDir + "/engine_room.blocks").c_str());
		// ... and where the shared libraries sit, to name their code
		int in = open("/proc/self/maps", O_RDONLY);
		int outFd = open((outDir + "/engine_room.blocks.maps").c_str(),
		                 O_WRONLY | O_CREAT | O_TRUNC, 0644);
		char chunk[4096];
		int got;
		while (in >= 0 && outFd >= 0 && (got = read(in, chunk, sizeof(chunk))) > 0) {
			if (write(outFd, chunk, got) != got) break;
		}
		if (in >= 0) close(in);
		if (outFd >= 0) close(outFd);
	}
	AudioProfiler::SetMarkHook(0);
	writeWav(wavPath.c_str(), audio);
	printf("wrote %s (%.1f s)\n", wavPath.c_str(), audio.size() / 2 / RATE);

	// The sound itself: loud, not clipped flat, no silence gaps
	double peak = 0, sum = 0;
	for (size_t i = 0; i < audio.size(); i++) {
		double a = fabs((double)audio[i]);
		if (a > peak) peak = a;
		sum += a * a;
	}
	double rms = audio.empty() ? 0 : sqrt(sum / audio.size());
	expect(rms > 1000.0, "the song plays (RMS of the mix)", rms, 1000.0);
	expect(peak < 32767.0, "limiter keeps it under full scale (peak)", peak, 32767.0);

	/* Wall time: several passes from the top, median per part */
	float seed = 0.1f;
	for (int i = 0; i < 4096; i++) {
		seed = seed * 3.7f * (1.0f - seed);
		baselineData[i] = seed - 0.5f;
	}
	std::vector<double> wallSlot[APS_COUNT];
	std::vector<double> wallTotal, wallBaseline;
	std::vector<int> passFrames;
	double passSeconds = seconds < 9.6 ? seconds : 9.6;
	for (int p = 0; p < passes; p++) {
		AudioProfiler::Reset();
		playSong(player, passSeconds, passFrames, 0);
		double total = 0;
		double audioSeconds = AudioProfiler::TotalFrames() / RATE;
		for (int s = 0; s < APS_COUNT; s++) {
			double us = AudioProfiler::TotalMicros(s) / audioSeconds;   // per second of audio
			wallSlot[s].push_back(us);
			total += us;
		}
		wallTotal.push_back(total);
		double t0 = AudioProfiler::NowMicros();
		volatile float r = baselineWorkload();
		(void)r;
		wallBaseline.push_back(AudioProfiler::NowMicros() - t0);
	}
	double baseUs = median(wallBaseline);

	/* The baseline in the cost model too */
	double baseCost = 0;
	if (a7) {
		syscall(SYS_RESET);
		syscall(SYS_MARK, 40);
		volatile float r = baselineWorkload();
		(void)r;
		syscall(SYS_MARK, 0);
		A7Snapshot b;
		a7Read(b);
		baseCost = b.slotCost[40];
	}

	/* Report */
	double audioSeconds = 0;
	for (size_t i = 0; i < frames.size(); i++) audioSeconds += frames[i];
	audioSeconds /= RATE;
	double costTotal = 0, insnTotal = 0;
	for (int s = 0; s < APS_COUNT; s++) {
		costTotal += snap.slotCost[s];
		insnTotal += snap.slotInsn[s];
	}
	printf("\n%-8s %14s %8s %9s %12s %8s\n", "part", "cost/s", "share", "dev.load", "wall us/s",
	       "share");
	double wallSum = median(wallTotal);
	for (int s = 0; s < APS_COUNT; s++) {
		double c = snap.slotCost[s] / audioSeconds;
		double w = median(wallSlot[s]);
		printf("%-8s %14.0f %7.1f%% %8.1f%% %12.0f %7.1f%%\n", AudioProfiler::SlotName(s), c,
		       costTotal > 0 ? 100.0 * snap.slotCost[s] / costTotal : 0.0,
		       100.0 * c / DEVICE_HZ, w, wallSum > 0 ? 100.0 * w / wallSum : 0.0);
	}
	double costPerSecond = costTotal / audioSeconds;
	printf("%-8s %14.0f %8s %8.1f%% %12.0f\n", "total", costPerSecond, "",
	       100.0 * costPerSecond / DEVICE_HZ, wallSum);
	printf("instructions per second of audio: %.0f (cost/instruction %.3f)\n",
	       insnTotal / audioSeconds, insnTotal > 0 ? costTotal / insnTotal : 0.0);

	// The device's load meter: each buffer's time over its play time,
	// smoothed 0.95/0.05 (AudioDriver::OnNewBufferNeeded)
	// (the plugin also saw the few buffers after Stop: not part of the song)
	if (snap.buffers.size() >= frames.size() && !frames.empty()) {
		double smooth = 0, smoothPeak = 0, worst = 0;
		for (size_t i = 0; i < frames.size(); i++) {
			double load = snap.buffers[i] / (frames[i] / RATE * DEVICE_HZ);
			smooth = smooth * 0.95 + load * 0.05;
			if (i > 40 && smooth > smoothPeak) smoothPeak = smooth;
			if (load > worst) worst = load;
		}
		printf("estimated device load (cost model at 1.2 GHz): average %.1f%%, "
		       "smoothed peak %.1f%%, worst buffer %.1f%%\n",
		       100.0 * costPerSecond / DEVICE_HZ, 100.0 * smoothPeak, 100.0 * worst);
	} else if (a7) {
		printf("buffer costs missing (%d of %d)\n", (int)snap.buffers.size(), (int)frames.size());
		failures++;
	}
	printf("wall: engine %.0f us per second of audio, baseline workload %.0f us, "
	       "ratio %.3f (median of %d passes)\n",
	       wallSum, baseUs, baseUs > 0 ? wallSum / baseUs : 0.0, passes);
	if (a7) {
		printf("cost model: baseline workload %.0f, ratio %.3f\n", baseCost,
		       baseCost > 0 ? costPerSecond / baseCost : 0.0);
	}

	// The most expensive buffers, part by part
	if (!gBufferSlots.empty()) {
		std::vector<std::pair<double, int> > order;
		std::vector<double> prev(APS_COUNT, 0.0);
		std::vector<std::vector<double> > delta;
		for (size_t b = 0; b < gBufferSlots.size(); b++) {
			std::vector<double> d(APS_COUNT);
			double sum = 0;
			for (int s = 0; s < APS_COUNT; s++) {
				d[s] = gBufferSlots[b][s] - prev[s];
				sum += d[s];
			}
			prev = gBufferSlots[b];
			delta.push_back(d);
			order.push_back(std::make_pair(sum / (frames[b] / RATE * DEVICE_HZ), (int)b));
		}
		std::sort(order.rbegin(), order.rend());
		for (int w = 0; w < worstCount && w < (int)order.size(); w++) {
			int b = order[w].second;
			double at = 0;
			for (int i = 0; i < b; i++) at += frames[i];
			printf("worst buffer %d at %.3f s: %.1f%%:", b, at / RATE, 100.0 * order[w].first);
			double budget = frames[b] / RATE * DEVICE_HZ;
			for (int s = 0; s < APS_COUNT; s++) {
				if (delta[b][s] > 0.005 * budget) {
					printf(" %s %.1f", AudioProfiler::SlotName(s), 100.0 * delta[b][s] / budget);
				}
			}
			printf("\n");
		}
	}

	/* Regression guard */
	if (!a7) {
		printf("a7cost plugin not loaded: the cost guard cannot run\n");
		failures++;
	} else if (RECORDED_COST_PER_SECOND > 0) {
		printf("cost against the unoptimised engine: %.1f%% (%.1f%% less)\n",
		       100.0 * costPerSecond / BEFORE_COST_PER_SECOND,
		       100.0 - 100.0 * costPerSecond / BEFORE_COST_PER_SECOND);
		expect(costPerSecond <= RECORDED_COST_PER_SECOND * REGRESSION_MARGIN,
		       "song cost per second within 25% of the recorded", costPerSecond,
		       RECORDED_COST_PER_SECOND);
	} else {
		printf("no recorded cost yet: record %.0f\n", costPerSecond);
	}

	printf("%s\n", failures ? "FAILED" : "ALL OK");
	fflush(stdout);
	_exit(failures ? 1 : 0);
}
