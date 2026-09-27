// Golden audio renders, built for the device and run under qemu-arm: every
// SYNTH preset (every engine), every MACRO preset and every sample the
// install ships (projects/resources/samples/<pack>/*.wav, played by the
// real SampleInstrument) plays one note as the instrument screen's preview
// does, and each result is written as a WAV. tools/golden_audio.py turns
// the WAVs into fingerprints (level envelope, pitch, brightness, bands,
// length, pan) and compares them with tests/golden/audio.json.
//
// GOLDEN_OUT    directory for <id>.wav and index.txt (required)
// GOLDEN_SHARD  "k/n": render only every n-th sound starting at k, so
//               several qemu processes share the work
// GOLDEN_ONLY   render only ids containing this text
// Build + run: tools/dsp-harness/golden_audio.sh (called by golden_audio.py)
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Services/Audio/Audio.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "System/FileSystem/FileSystem.h"
#include "Adapters/DINGOO/System/DINGOOSystem.h"
#include "System/Console/Logger.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

class HarnessAudio : public Audio {
public:
	HarnessAudio(AudioSettings &s) : Audio(s) {}
	virtual void Init() {}
	virtual void Close() {}
};

static const int FRAMES = 1199;       // what the device mixes per buffer
static const double RATE = 44100.0;
static const int NOTE = 60;           // the note the instrument screen plays
static const double HOLD = 1.2;       // seconds held (a few phrase rows)
static const double MAX_TAIL = 2.4;   // release: until silent, at most this

static fixed buffer[FRAMES * 2];
static std::string outDir;
// Plain system calls: the app's headers redefine fopen/fclose/fprintf to
// its FileSystem (Externals/TinyXML/Tiny2NosStub.h), so the index file
// (this harness's own bookkeeping, not app data) goes through raw fds.
static int indexFd = -1;
static int shardK = 0, shardN = 1, counter = 0, failures = 0;
static const char *only = 0;

static void writeIndexLine(const std::string &id, const std::string &kind) {
	std::string line = id + "\t" + kind + "\n";
	if (indexFd < 0 || write(indexFd, line.data(), line.size()) != (ssize_t)line.size()) {
		printf("could not write index line for %s\n", id.c_str());
		failures++;
	}
}

static void put32(std::vector<char> &h, unsigned int v) {
	for (int i = 0; i < 4; i++) h.push_back((char)((v >> (8 * i)) & 0xFF));
}
static void put16(std::vector<char> &h, unsigned int v) {
	for (int i = 0; i < 2; i++) h.push_back((char)((v >> (8 * i)) & 0xFF));
}

static void writeWav(const std::string &path, const std::vector<short> &data) {
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
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 || write(fd, &h[0], h.size()) != (ssize_t)h.size() ||
	    (bytes && write(fd, &data[0], bytes) != (ssize_t)bytes)) {
		printf("could not write %s\n", path.c_str());
		failures++;
	}
	if (fd >= 0) close(fd);
}

// Ids are file names: letters, digits, '-' and '.'
static std::string slug(const std::string &s) {
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		char c = s[i];
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.') out += c;
		else if (c >= 'A' && c <= 'Z') out += (char)(c - 'A' + 'a');
		else out += '_';
	}
	return out;
}

// This process renders the sound when it is in its shard (and GOLDEN_ONLY)
static bool mine(const std::string &id) {
	int n = counter++;
	if (only && id.find(only) == std::string::npos) return false;
	return n % shardN == shardK;
}

// Holds the note HOLD seconds, releases, and keeps going until the voice
// ends by itself (Render returns false) or MAX_TAIL has passed
static void playTo(const std::string &id, const std::string &kind, I_Instrument *s) {
	std::vector<short> out;
	s->Start(0, NOTE, true);
	int held = (int)(HOLD * RATE / FRAMES) + 1;
	int tail = (int)(MAX_TAIL * RATE / FRAMES) + 1;
	for (int b = 0; b < held + tail; b++) {
		if (b == held) s->Stop(0);
		memset(buffer, 0, sizeof(buffer));
		bool playing = s->Render(0, buffer, FRAMES, true);
		if (!playing && b >= held) break;
		for (int i = 0; i < FRAMES * 2; i++) {
			int v = playing ? (int)fp2fl(buffer[i]) : 0;
			if (v > 32767) v = 32767;
			if (v < -32768) v = -32768;
			out.push_back((short)v);
		}
	}
	s->StopQuickly(0);
	for (int b = 0; b < 20; b++) s->Render(0, buffer, FRAMES, false);
	writeWav(outDir + "/" + id + ".wav", out);
	writeIndexLine(id, kind);
	printf("rendered %s (%.2f s)\n", id.c_str(), out.size() / 2 / RATE);
	fflush(stdout);
}

static std::vector<std::string> listDir(const std::string &dir, bool dirs) {
	std::vector<std::string> names;
	DIR *d = opendir(dir.c_str());
	if (!d) return names;
	struct dirent *e;
	while ((e = readdir(d))) {
		std::string n = e->d_name;
		if (n[0] == '.') continue;
		std::string full = dir + "/" + n;
		DIR *sub = opendir(full.c_str());
		bool isDir = sub != 0;
		if (sub) closedir(sub);
		if (isDir != dirs) continue;
		if (!dirs && (n.size() < 4 || n.substr(n.size() - 4) != ".wav")) continue;
		names.push_back(n);
	}
	closedir(d);
	std::sort(names.begin(), names.end());
	return names;
}

int main() {
	System::Install(new GPSDLSystem());
	FileSystem::Install(new UnixFileSystem());
	Trace::GetInstance()->SetLogger(*(new StdOutLogger()));
	Path::SetAlias("bin", ".");
	Path::SetAlias("root", ".");
	AudioSettings settings;
	settings.bufferSize_ = 1024;
	settings.preBufferCount_ = 0;
	Audio::Install(new HarnessAudio(settings));

	const char *dir = getenv("GOLDEN_OUT");
	if (!dir) {
		printf("GOLDEN_OUT not set\n");
		return 2;
	}
	outDir = dir;
	if (getenv("GOLDEN_SHARD")) sscanf(getenv("GOLDEN_SHARD"), "%d/%d", &shardK, &shardN);
	if (shardN < 1) shardN = 1;
	only = getenv("GOLDEN_ONLY");
	char indexName[64];
	snprintf(indexName, sizeof(indexName), "/index-%d.txt", shardK);
	std::string indexPath = outDir + indexName;
	indexFd = open(indexPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (indexFd < 0) {
		printf("cannot write %s\n", indexPath.c_str());
		return 2;
	}

	// Every SYNTH preset, as loaded (its own engine, volume, envelope)
	int count = SynthInstrument::GetPresetCount();
	for (int p = 0; p < count; p++) {
		const char *name = SynthInstrument::GetPresetName(p);
		const char *engine = SynthInstrument::GetEngineName(SynthInstrument::GetPresetEngine(p));
		std::string id = "synth-" + slug(engine) + "-" + slug(name);
		if (!mine(id)) continue;
		SynthInstrument *s = new SynthInstrument();
		s->Init();
		s->LoadPreset(name);
		playTo(id, std::string("synth ") + engine, s);
		delete s;
	}

	// Every MACRO preset
	int macros = MacroInstrument::GetPresetCount();
	for (int p = 0; p < macros; p++) {
		const char *name = MacroInstrument::GetPresetName(p);
		std::string id = "macro-" + slug(name);
		if (!mine(id)) continue;
		MacroInstrument *m = new MacroInstrument();
		m->Init();
		m->LoadPreset(name);
		playTo(id, "macro", m);
		delete m;
	}

	// Every shipped sample, one pack at a time (the pool holds 128)
	const char *packs = getenv("GOLDEN_SAMPLES");
	std::string packRoot = packs ? packs : "../projects/resources/samples";
	std::vector<std::string> packNames = listDir(packRoot, true);
	for (size_t k = 0; k < packNames.size(); k++) {
		std::string packDir = packRoot + "/" + packNames[k];
		std::vector<std::string> files = listDir(packDir, false);
		std::vector<std::string> ids;
		bool any = false;
		for (size_t f = 0; f < files.size(); f++) {
			std::string id = "sample-" + slug(packNames[k]) + "-" + slug(files[f].substr(0, files[f].size() - 4));
			ids.push_back(mine(id) ? id : "");
			if (!ids.back().empty()) any = true;
		}
		if (!any) continue;
		SamplePool::GetInstance()->Reset();
		Path::SetAlias("samples", packDir.c_str());
		for (size_t f = 0; f < files.size(); f++) {
			if (ids[f].empty()) continue;
			int index = SamplePool::GetInstance()->AddProjectSample(files[f].c_str());
			if (index < 0) {
				printf("could not load %s/%s\n", packDir.c_str(), files[f].c_str());
				failures++;
				continue;
			}
			SampleInstrument *s = new SampleInstrument();
			s->Init();
			s->AssignSample(index);
			playTo(ids[f], "sample " + packNames[k], s);
			delete s;
		}
	}
	if (indexFd >= 0) close(indexFd);
	printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL OK", failures);
	return failures ? 1 : 0;
}
