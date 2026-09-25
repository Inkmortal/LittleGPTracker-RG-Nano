#ifndef _UNDO_HISTORY_H_
#define _UNDO_HISTORY_H_

#include <string>
#include <vector>

class Project;
class Song;
class Variable;
class VariableContainer;

// Undo / redo for everything a button press can change: the song, chains,
// phrases, tables, grooves, mixer levels, project settings and the
// instrument being edited. A snapshot is taken before each press and kept
// only if the press changed something; one A-hold of edits is one step.
class UndoHistory {
public:
	struct Snapshot {
		std::vector<unsigned char> data;
		struct Value {
			Variable *var;
			int number;
			std::string text;
		};
		std::vector<Value> values;
		VariableContainer *instrument; // the instrument whose values are in 'values'
		std::string label;             // what was edited, for the notification
		int generation;                // Clear() count when taken
	};

	static void Capture(Project *project, Song *song, VariableContainer *instrument,
	                    const char *label, Snapshot &out);
	static bool Same(const Snapshot &a, const Snapshot &b);

	// After a press: 'before' is kept if the state changed. merge: the press
	// continues the previous step (A still held), so no new step is made.
	// Returns true if the press changed something.
	static bool Record(Project *project, Song *song, const Snapshot &before,
	                   bool merge);

	// Returns the label of what was undone/redone, or 0 if nothing to do
	static const char *Undo(Project *project, Song *song);
	static const char *Redo(Project *project, Song *song);
	static void Clear();
	static int UndoCount();

private:
	static void restore(Project *project, Song *song, const Snapshot &s);
	static std::vector<Snapshot> undo_;
	static std::vector<Snapshot> redo_;
	static std::string lastLabel_;
	static int generation_;  // bumped by Clear(): a new song, old pointers
};

#endif
