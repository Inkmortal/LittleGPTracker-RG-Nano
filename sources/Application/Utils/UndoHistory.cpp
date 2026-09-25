#include "UndoHistory.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Model/Groove.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "Application/Model/Song.h"
#include "Application/Model/Table.h"
#include <string.h>

// Enough to back out of a burst of mistakes; each step is ~100 KB
#define UNDO_DEPTH 32

std::vector<UndoHistory::Snapshot> UndoHistory::undo_;
std::vector<UndoHistory::Snapshot> UndoHistory::redo_;
std::string UndoHistory::lastLabel_;
int UndoHistory::generation_ = 0;

namespace {
// Every block of pattern data, in a fixed order, as (pointer, bytes)
struct Block {
	void *ptr;
	size_t bytes;
};

int blocks(Song *song, Block *out) {
	int n = 0;
	Chain *c = song->chain_;
	Phrase *p = song->phrase_;
	const size_t steps = PHRASE_COUNT * 16;
	TableHolder *tables = TableHolder::GetInstance();
	out[n].ptr = song->data_; out[n++].bytes = SONG_CHANNEL_COUNT * SONG_ROW_COUNT;
	out[n].ptr = c->data_; out[n++].bytes = CHAIN_COUNT * 16;
	out[n].ptr = c->transpose_; out[n++].bytes = CHAIN_COUNT * 16;
	out[n].ptr = c->UsedFlags(); out[n++].bytes = CHAIN_COUNT * sizeof(bool);
	out[n].ptr = p->note_; out[n++].bytes = steps;
	out[n].ptr = p->instr_; out[n++].bytes = steps;
	out[n].ptr = p->cmd1_; out[n++].bytes = steps * sizeof(FourCC);
	out[n].ptr = p->param1_; out[n++].bytes = steps * sizeof(ushort);
	out[n].ptr = p->cmd2_; out[n++].bytes = steps * sizeof(FourCC);
	out[n].ptr = p->param2_; out[n++].bytes = steps * sizeof(ushort);
	out[n].ptr = p->UsedFlags(); out[n++].bytes = PHRASE_COUNT * sizeof(bool);
	out[n].ptr = tables->Tables(); out[n++].bytes = TABLE_COUNT * sizeof(Table);
	out[n].ptr = tables->Allocation(); out[n++].bytes = TABLE_COUNT * sizeof(bool);
	out[n].ptr = Groove::GetInstance()->GetGrooveData(0); out[n++].bytes = MAX_GROOVES * 16;
	return n;
}

void addValues(VariableContainer *container, std::vector<UndoHistory::Snapshot::Value> &out) {
	if (!container) return;
	IteratorPtr<Variable> it(container->GetIterator());
	for (it->Begin(); !it->IsDone(); it->Next()) {
		Variable &v = it->CurrentItem();
		UndoHistory::Snapshot::Value value;
		value.var = &v;
		if (v.GetType() == Variable::STRING) {
			value.number = 0;
			value.text = v.GetString() ? v.GetString() : "";
		} else {
			value.number = v.GetInt();
		}
		out.push_back(value);
	}
}

// The instrument object can be replaced (synth <-> sample): only touch its
// variables if it is still in the bank
bool instrumentAlive(Project *project, VariableContainer *instrument) {
	if (!instrument) return false;
	InstrumentBank *bank = project->GetInstrumentBank();
	for (int i = 0; i < MAX_INSTRUMENT_COUNT; i++) {
		if ((VariableContainer *)bank->GetInstrument(i) == instrument) return true;
	}
	return false;
}
} // namespace

void UndoHistory::Capture(Project *project, Song *song, VariableContainer *instrument,
                          const char *label, Snapshot &out) {
	Block b[16];
	int n = blocks(song, b);
	size_t total = MIXER_LEVELS;
	for (int i = 0; i < n; i++) total += b[i].bytes;
	out.data.resize(total);
	unsigned char *dst = &out.data[0];
	for (int i = 0; i < n; i++) {
		memcpy(dst, b[i].ptr, b[i].bytes);
		dst += b[i].bytes;
	}
	Mixer *mixer = Mixer::GetInstance();
	for (int i = 0; i < MIXER_LEVELS; i++) *dst++ = (unsigned char)mixer->GetLevel(i);
	out.values.clear();
	addValues(project, out.values);
	addValues(instrument, out.values);
	out.instrument = instrument;
	out.label = label ? label : "edit";
	out.generation = generation_;
}

bool UndoHistory::Same(const Snapshot &a, const Snapshot &b) {
	if (a.data.size() != b.data.size() || a.values.size() != b.values.size()) return false;
	if (memcmp(&a.data[0], &b.data[0], a.data.size())) return false;
	for (size_t i = 0; i < a.values.size(); i++) {
		const Snapshot::Value &x = a.values[i];
		const Snapshot::Value &y = b.values[i];
		if (x.var != y.var || x.number != y.number || x.text != y.text) return false;
	}
	return true;
}

static size_t projectValueCount(Project *project) {
	size_t n = 0;
	IteratorPtr<Variable> it(project->GetIterator());
	for (it->Begin(); !it->IsDone(); it->Next()) n++;
	return n;
}

// The snapshot's instrument values still belong to a live instrument: the
// object is in the bank and its first variable is the one we stored (a new
// object at a recycled address would have different variables)
static bool instrumentValid(Project *project, const UndoHistory::Snapshot &s) {
	if (!instrumentAlive(project, s.instrument)) return false;
	size_t first = projectValueCount(project);
	if (s.values.size() <= first) return true;
	IteratorPtr<Variable> it(s.instrument->GetIterator());
	it->Begin();
	return !it->IsDone() && &it->CurrentItem() == s.values[first].var;
}

// The instrument object was replaced (type changed): its variables are gone,
// so no snapshot may keep pointers to them
static void forgetInstrument(Project *project, UndoHistory::Snapshot &s,
                             VariableContainer *gone) {
	if (s.instrument != gone) return;
	size_t keep = projectValueCount(project);
	if (s.values.size() > keep) s.values.resize(keep);
	s.instrument = 0;
}

bool UndoHistory::Record(Project *project, Song *song, const Snapshot &before, bool merge) {
	// The press loaded another song: 'before' belongs to the old one
	if (before.generation != generation_ || before.data.empty()) return false;
	Snapshot kept = before;
	if (kept.instrument && !instrumentValid(project, kept)) {
		VariableContainer *gone = kept.instrument;
		forgetInstrument(project, kept, gone);
		for (size_t i = 0; i < undo_.size(); i++) forgetInstrument(project, undo_[i], gone);
		for (size_t i = 0; i < redo_.size(); i++) forgetInstrument(project, redo_[i], gone);
	}
	Snapshot after;
	Capture(project, song, kept.instrument, kept.label.c_str(), after);
	if (Same(kept, after)) return false;
	redo_.clear();
	if (merge && !undo_.empty()) return true; // same A-hold: still one step
	undo_.push_back(kept);
	if (undo_.size() > UNDO_DEPTH) undo_.erase(undo_.begin());
	return true;
}

void UndoHistory::restore(Project *project, Song *song, const Snapshot &s) {
	Block b[16];
	int n = blocks(song, b);
	const unsigned char *src = &s.data[0];
	for (int i = 0; i < n; i++) {
		memcpy(b[i].ptr, src, b[i].bytes);
		src += b[i].bytes;
	}
	Mixer *mixer = Mixer::GetInstance();
	for (int i = 0; i < MIXER_LEVELS; i++) mixer->SetLevel(i, *src++);
	bool instrumentOk = instrumentValid(project, s);
	// Project values come first in the list, then the instrument's
	size_t projectCount = projectValueCount(project);
	for (size_t i = 0; i < s.values.size(); i++) {
		if (i >= projectCount && !instrumentOk) break;
		const Snapshot::Value &v = s.values[i];
		if (v.var->GetType() == Variable::STRING) {
			if (v.text != (v.var->GetString() ? v.var->GetString() : "")) v.var->SetString(v.text.c_str());
		} else if (v.var->GetInt() != v.number) {
			v.var->SetInt(v.number);
		}
	}
}

const char *UndoHistory::Undo(Project *project, Song *song) {
	if (undo_.empty()) return 0;
	Snapshot target = undo_.back();
	undo_.pop_back();
	Snapshot now;
	Capture(project, song, target.instrument, target.label.c_str(), now);
	redo_.push_back(now);
	restore(project, song, target);
	lastLabel_ = target.label;
	return lastLabel_.c_str();
}

const char *UndoHistory::Redo(Project *project, Song *song) {
	if (redo_.empty()) return 0;
	Snapshot target = redo_.back();
	redo_.pop_back();
	Snapshot now;
	Capture(project, song, target.instrument, target.label.c_str(), now);
	undo_.push_back(now);
	restore(project, song, target);
	lastLabel_ = target.label;
	return lastLabel_.c_str();
}

void UndoHistory::Clear() {
	generation_++;
	undo_.clear();
	redo_.clear();
}

int UndoHistory::UndoCount() {
	return (int)undo_.size();
}
