# LittleGPTracker (LGPT) - Legacy UX/UI Notes

> RG Nano note: this file is older project analysis and may describe pre-fork controls. For current user-facing RG Nano behavior, use the guide in [docs/rgnano-wiki](docs/rgnano-wiki/Home.md) (also the in-app Help and the website).

**Platform**: RG Nano (Miyoo Mini compatible)
**Date**: 2025-10-05
**Version**: Based on current codebase analysis

---

## Table of Contents

1. [Overview](#overview)
2. [Button Layout](#button-layout)
3. [Screen Navigation](#screen-navigation)
4. [Main Screens](#main-screens)
5. [Button Combinations](#button-combinations)
6. [Common UX Patterns](#common-ux-patterns)
7. [View System Architecture](#view-system-architecture)

---

## Overview

LGPT uses a tracker-style interface inspired by Little Sound DJ (LSDJ). The UI is optimized for handheld devices with limited screen space and button-based input. Navigation follows a hierarchical pattern: **Song → Chain → Phrase**, with additional screens for instruments, tables, project management, grooves, and mixer.

### Design Philosophy

- **Button combinations** for advanced functions
- **Modal editing** - each screen has specific edit modes
- **Clipboard operations** - cut/copy/paste throughout
- **Real-time playback** - visual feedback during playback
- **Minimal UI** - text-based, monospace font

---

## Button Layout

### Physical Buttons (RG Nano / Miyoo Mini)

Based on `projects/resources/MIYOO/mapping.xml`:

| Physical Button | LGPT Mapping | Description |
|----------------|--------------|-------------|
| **D-Pad Up** | UP | Navigate up / Increase values |
| **D-Pad Down** | DOWN | Navigate down / Decrease values |
| **D-Pad Left** | LEFT | Navigate left |
| **D-Pad Right** | RIGHT | Navigate right |
| **A Button** (Space) | A | Confirm / Edit / Clone |
| **B Button** (Ctrl) | B | Cancel / Quick actions |
| **Start** (Return) | START | Play/Stop |
| **L1** (E key) | L (Left Shoulder) | Modifier / Extend selection |
| **R1** (T key) | R (Right Shoulder) | Modifier / Switch views |

### Button Masks (Internal)

Defined in `sources/Application/Views/BaseClasses/View.h`:

```cpp
enum GUIEventPadButtonMasks {
    EPBM_LEFT   = 1,     // 0x001
    EPBM_DOWN   = 2,     // 0x002
    EPBM_RIGHT  = 4,     // 0x004
    EPBM_UP     = 8,     // 0x008
    EPBM_L      = 16,    // 0x010
    EPBM_B      = 32,    // 0x020
    EPBM_A      = 64,    // 0x040
    EPBM_R      = 128,   // 0x080
    EPBM_START  = 256,   // 0x100
    EPBM_SELECT = 512,   // 0x200
    EPBM_DOUBLE_A = 1024,  // 0x400
    EPBM_DOUBLE_B = 2048   // 0x800
};
```

---

## Screen Navigation

### View Types

Defined in `sources/Application/Views/BaseClasses/View.h`:

```cpp
enum ViewType {
    VT_SONG,       // Song sequencer (chain arrangement)
    VT_CHAIN,      // Chain editor (phrase sequences)
    VT_PHRASE,     // Phrase editor (note/command data)
    VT_PROJECT,    // Project settings
    VT_INSTRUMENT, // Instrument parameters
    VT_TABLE,      // Table editor (under phrase)
    VT_TABLE2,     // Table editor (under instrument)
    VT_GROOVE,     // Groove settings
    VT_MIXER       // Mixer view
};
```

### Navigation Flow

```
PROJECT (start)
    ↓
    ├── SONG (RT+Left)
    │   ├── CHAIN (RT+Down from song row)
    │   │   └── PHRASE (RT+Down from chain row)
    │   │       └── TABLE (RT+Down from phrase with table)
    │   │
    │   └── INSTRUMENT (RT+Right)
    │       └── TABLE2 (RT+Down from instrument table param)
    │
    ├── GROOVE (RT+Up)
    └── MIXER (TBD)
```

### Primary Navigation (R shoulder combinations)

| Combo | Action | Context |
|-------|--------|---------|
| **RT + Left** | Go to Song View | From any view |
| **RT + Right** | Go to Instrument View | From Song/Chain/Phrase |
| **RT + Up** | Go to Groove View | From Song |
| **RT + Down** | Navigate down hierarchy | Song→Chain, Chain→Phrase, Phrase→Table |
| **RT + Start** | Stop playback immediately | Any view during playback |

---

## Main Screens

### 1. Song View

**Purpose**: Arrange chains into a song structure

**Display**:
```
SONG
00│00 01 02 03 04 05 06 07
01│-- -- -- -- -- -- -- --
02│-- -- -- -- -- -- -- --
[...]
```

**Columns**: 8 channels (0-7)
**Rows**: Song positions (00-FF)

**Navigation**:
- **Arrows**: Move cursor
- **A + Arrows**: Increment/decrement chain number
- **A (press once)**: Enter value directly
- **B**: Quick actions / Cancel
- **B + Arrows**: Jump by configurable amount

**Editing**:
- **A + Down/Up**: Change chain number
- **A + L**: Clone chain (copy to next position)
- **LT + A**: Deep clone (clone chain AND all phrases within)

**Playback**:
- **START**: Play from cursor position
- **RT + START**: Stop immediately
- **LT + START**: Play from row 00
- **RT + Down**: Queue phrase for live mode

**Selection Mode**:
- **B + A**: Enter selection mode
- **Arrows**: Extend selection
- **A + L**: Copy selection
- **A + R**: Cut selection
- **A (alone)**: Paste

**Muting**:
- **RT + L**: Toggle mute on current channel
- **RT + A + L**: Unmute all channels
- **RT + A + R**: Solo mode (only current channel)

### 2. Chain View

**Purpose**: Sequence phrases within a chain

**Display**:
```
CHAIN 00
00│PH TSP
01│00 +00
02│01 +00
[...]
```

**Columns**:
- `PH`: Phrase number (00-FF)
- `TSP`: Transpose value (-7F to +7F)

**Navigation**:
- **Arrows**: Move cursor
- **A + Up/Down**: Change value
- **RT + Up/Down**: Navigate to different chain

**Editing**:
- **A + Down/Up**: Inc/dec phrase or transpose
- **A + L**: Clone row
- **B + A**: Selection mode

**Playback**:
- **START**: Play current phrase/chain
- **RT + START**: Stop

### 3. Phrase View

**Purpose**: Edit note and command data

**Display**:
```
PHRASE 00  INST 00
00│C-4 00 ---- -- ---- --
01│... .. .... .. .... ..
[...]
```

**Columns**:
- `NOTE`: Note value (C-0 to B-9, or ---)
- `INST`: Instrument number (00-7F)
- `CMD1`: First command (FourCC)
- `VAL1`: First command value
- `CMD2`: Second command
- `VAL2`: Second command value

**Navigation**:
- **Arrows**: Move cursor between columns
- **A + Up/Down**: Change value in current column

**Note Entry**:
- **A + Up/Down**: Chromatic note change
- **LT + A**: Trigger note (audition)

**Commands**:
Common commands (FourCC format):
- `VOLM` - Volume
- `PAN_` - Panning
- `PTCH` - Pitch
- `FLTR` - Filter cutoff
- `RCUT` - Filter resonance
- `KILL` - Kill note
- `TBLE` - Table reference

**Editing**:
- **A + Left/Right**: Quick edit mode
- **B + A**: Selection mode
- **A + L**: Clone row
- **B + L**: Cut
- **B + R**: Paste

### 4. Instrument View

**Purpose**: Configure instrument parameters

**Display** (for sample instruments):
```
INST 00  SAMPLE
sample:  [browse]
volume:  80
pan:     80
start:   00000
end:     FFFFF
loopst:  00000
loopend: FFFFF
[...]
```

**Navigation**:
- **Up/Down**: Select parameter
- **A + Up/Down**: Change value
- **LT/RT + Up/Down**: Fast scroll

**Special Actions**:
- **A (on sample param)**: Browse samples
- **LT + A (on sample param)**: **RECORD SAMPLE** (new feature!)
- **RT + Left/Right**: Switch instruments

**Sample Recording** (New Feature):
- **LT + A** on `sample` parameter opens recording dialog
- See RecordSampleDialog section below

### 5. Table View

**Purpose**: Automated parameter modulation

**Display**:
```
TABLE 00
00│VOL TCK TSP CMD1   VAL1
01│80  01  00  ----   --
02│7F  01  00  PTCH   +01
[...]
```

**Columns**:
- `VOL`: Volume (00-80)
- `TCK`: Tick speed
- `TSP`: Transpose
- `CMD1-3`: Up to 3 commands per step
- `VAL1-3`: Command values

**Usage**:
- Applied per-note when referenced in phrase `TBLE` command
- Loops/one-shot configurable

### 6. Project View

**Purpose**: Project-level settings

**Parameters**:
- `tempo`: BPM (40-999)
- `transpose`: Global transpose
- `masterVol`: Master volume
- `clockSource`: Internal/MIDI
- Various MIDI settings

**Actions**:
- **RT + Down**: Load project
- **RT + Start**: Save project

### 7. Groove View

**Purpose**: Define swing/groove patterns

**Display**:
```
GROOVE 00
01│06
02│06
03│06
[...]
```

Each row = tick length for that step in pattern.
Standard: `06 06 06 06` = straight 4/4

Swing example: `08 04 08 04` = shuffle

---

## Button Combinations

### Global Combinations

| Combo | Action | Notes |
|-------|--------|-------|
| **RT + Left** | Song View | Works from any view |
| **RT + Right** | Instrument View | Context-sensitive |
| **RT + Up** | Groove View | From song |
| **RT + Down** | Navigate down / Load Project | Context-sensitive |
| **RT + START** | Stop playback | Immediate stop |
| **LT + START** | Play from 00 | Song mode |
| **B + A** | Selection mode | Most views |
| **A + L** | Clone / Copy | Context-sensitive |
| **LT + A** | Audition / Deep clone / **Record** | Context-sensitive |

### Song View Specific

| Combo | Action |
|-------|--------|
| **A + Down/Up** | Change chain value |
| **A + Left/Right** | Change channel value |
| **LT + A** (hold A first) | Deep clone chain |
| **RT + L** | Mute channel |
| **RT + A + L** | Unmute all |
| **RT + A + R** | Solo mode |
| **B + Left/Right** | Jump columns |
| **B + Up/Down** | Jump rows |

### Chain/Phrase View Specific

| Combo | Action |
|-------|--------|
| **RT + Up/Down** | Navigate to neighbor chain/phrase |
| **RT + Left** | Return to song |
| **START** | Play current chain/phrase |
| **B + L** | Cut |
| **B + R** | Paste |

### Instrument View Specific

| Combo | Action | Location |
|-------|--------|----------|
| **LT + A** | **Open Record Dialog** | On `sample` parameter |
| **A** | Browse samples | On `sample` parameter |
| **RT + Left/Right** | Switch instruments | Any parameter |

---

## Common UX Patterns

### 1. Modal Editing

Most screens use **modal editing**:
1. Navigate to parameter with arrows
2. Press **A** to enter edit mode
3. Use arrows to change value
4. Press **B** or navigate away to exit

### 2. Selection System

**Clipboard operations** work across Song, Chain, Phrase, Table views:

1. **Enter selection**: Press **B + A**
2. **Extend selection**: Use arrows
3. **Copy**: Press **A + L** (while in selection mode)
4. **Cut**: Press **A + R** (while in selection mode)
5. **Paste**: Exit selection, move cursor, press **A**

### 3. Clone vs Deep Clone

**Clone** (A + L or A alone):
- Copies chain number only
- New chain references same phrases

**Deep Clone** (LT + A, hold A first):
- Copies chain AND all phrases
- Creates independent copies of everything

### 4. Playback Modes

Defined in `ViewData.h`:
```cpp
enum PlayMode {
    PM_SONG,     // Play song from current position
    PM_CHAIN,    // Play single chain
    PM_PHRASE,   // Play single phrase
    PM_LIVE,     // Queue phrases for live performance
    PM_AUDITION  // Preview note/instrument
};
```

### 5. Live Mode (RT + Down in Song View)

- **RT + Down** on a song row: Queue that row for next pattern
- Visual feedback shows queued position
- Seamless switching between song sections

### 6. Muting System

3 modes:
- **Normal**: All channels play
- **Mute**: Specific channels muted (RT + L to toggle)
- **Solo**: Only selected channel plays (RT + A + R)

**Unmute all**: RT + A + L (or RT + L)

---

## View System Architecture

### Class Hierarchy

```
View (abstract base class)
    ├── SongView
    ├── ChainView
    ├── PhraseView
    ├── InstrumentView (extends FieldView)
    ├── TableView
    ├── ProjectView (extends FieldView)
    ├── GrooveView
    ├── MixerView
    └── ConsoleView
```

### View Modes

```cpp
enum ViewMode {
    VM_NORMAL,      // Standard navigation/editing
    VM_NEW,         // Creating new data
    VM_CLONE,       // Cloning existing data
    VM_SELECTION,   // Clipboard selection active
    VM_MUTEON,      // Muting mode
    VM_SOLOON       // Solo mode
};
```

### Key Methods

Every view implements:

```cpp
virtual void ProcessButtonMask(unsigned short mask, bool pressed);
virtual void DrawView();
virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick);
virtual void OnFocus();
```

### ViewData Structure

Shared data across all views (`sources/Application/Views/ViewData.h`):

```cpp
class ViewData {
    Project *project_;           // Current project
    Song *song_;                 // Current song

    // Song view state
    int songX_, songY_;          // Cursor position
    int songOffset_;             // Scroll position

    // Chain view state
    int chainRow_, chainCol_;
    int currentChain_;

    // Phrase view state
    int currentPhrase_;

    // Other view states
    int currentInstrument_;
    int currentTable_;
    int currentGroove_;

    // Playback state
    PlayMode playMode_;
    int songPlayPos_[8];         // Per-channel playback position
    unsigned char currentPlayChain_[8];
    // ... more playback state
};
```

This shared structure allows seamless navigation between views while preserving cursor positions and edit state.

---

## Modal Dialogs

### RecordSampleDialog (NEW)

**Trigger**: **LT + A** on Instrument View `sample` parameter

**Display**:
```
RECORD SAMPLE

Duration: [10] seconds
          (1-60)

Status: Ready
        Press START to record
```

**Controls**:
- **Up/Down**: Adjust duration (1-60 seconds)
- **START**: Begin/stop recording
- **A**: Save and import (after recording)
- **B**: Cancel

**Flow**:
1. LT + A → Dialog opens
2. Adjust duration
3. START → Recording begins
4. START again (or auto-stop at duration) → Recording stops
5. A → Save as `recorded_XXX.wav` and auto-import to current instrument
6. Returns to Instrument View

### ImportSampleDialog

**Trigger**: **A** on Instrument View `sample` parameter

**Purpose**: Browse and select sample files from SD card

### MessageBox

**Purpose**: Display notifications, errors, confirmations

**Example**: "Sample imported successfully"

### SelectProjectDialog

**Trigger**: **RT + Down** in Project View

**Purpose**: Load existing project

### NewProjectDialog

**Trigger**: **RT + Start** in Project View (when creating new)

**Purpose**: Create new project with name

---

## Color Definitions

Defined in `View.h`:

```cpp
enum ColorDefinition {
    CD_BACKGROUND,   // Background color
    CD_NORMAL,       // Normal text
    CD_BORDER,       // Window borders
    CD_HILITE1,      // Highlight 1 (headers)
    CD_HILITE2,      // Highlight 2 (selected)
    CD_CONSOLE,      // Console text
    CD_CURSOR,       // Current cursor position
    CD_PLAY,         // Playback position
    CD_MUTE,         // Muted channel
    CD_SONGVIEWFE,   // Song view FE (empty)
    CD_SONGVIEW00,   // Song view 00 (first chain)
    CD_ROW,          // Row numbers
    CD_ROW2,         // Alternate row numbers
    CD_MAJORBEAT     // Major beat indicator
};
```

---

## UX Philosophy & Design Patterns

### 1. **Consistency Across Views**

- **A button** always means "edit/confirm"
- **B button** always means "cancel/quick action"
- **RT combinations** always for view switching
- **LT combinations** always for extensions/modifiers

### 2. **Non-Destructive Editing**

- Selection system allows copy before cut
- Deep clone creates independent copies
- Undo not implemented, so careful workflows encouraged

### 3. **Real-Time Feedback**

- Playback position highlighted during play
- Muted channels visually distinct
- Queued positions shown in live mode

### 4. **Minimal Visual Clutter**

- Text-based interface
- Monospace font for alignment
- Limited colors for clarity
- No icons, all text labels

### 5. **Keyboard-First Design**

- Every function accessible via buttons
- No mouse/touch required
- Button combinations for advanced features
- Fast navigation with B + arrows

---

## Platform-Specific Notes (RG Nano / Miyoo Mini)

### Button Mapping

The RG Nano uses Miyoo Mini button mappings:
- Physical A/B/X/Y mapped to Space/Ctrl/Shift/Alt
- L1/R1 mapped to E/T keys
- D-pad mapped to arrow keys
- Start/Select mapped to Return/RightCtrl

### Screen Layout

- **Resolution**: 240×240 pixels (1:1 aspect ratio)
- **Font**: Monospace, sized for readability
- **Rows visible**: Varies by view (~12-16 rows typical)
- **Scroll**: Automatic when cursor moves off screen

### Performance Considerations

- **Redraw on dirty flag**: Views only redraw when `isDirty_` set
- **Efficient text rendering**: Character-based, not pixel-based
- **Minimal UI updates**: Only active view redraws

---

## Common Workflows

### Creating a Song

1. **Start** in Project View
2. **RT + Left** → Song View
3. **A + Down** → Select chain 00 for channel 0
4. **RT + Down** → Enter Chain View
5. **A + Down** → Select phrase 00
6. **RT + Down** → Enter Phrase View
7. **Edit notes** with A + Up/Down
8. **START** → Play to hear
9. **RT + Left** → Back to Song
10. **Continue** arranging chains

### Recording Audio (NEW Workflow)

1. **RT + Right** → Instrument View
2. **Navigate** to `sample` parameter
3. **LT + A** → Open Record Dialog
4. **Adjust** duration with Up/Down
5. **START** → Begin recording (speak/play into USB audio input)
6. **START** again to stop (or wait for auto-stop)
7. **A** → Save and import
8. **Sample** now loaded in instrument, ready to use!

### Live Performance

1. **Arrange** song with different sections (verse, chorus, etc.)
2. **START** → Begin playback
3. **RT + Down** on song rows → Queue next section
4. **RT + L** → Mute/unmute channels on the fly
5. **RT + A + R** → Solo specific channel

### Copying Pattern Sections

1. **Song View** → Position cursor at start of selection
2. **B + A** → Enter selection mode
3. **Arrows** → Extend selection
4. **A + L** → Copy
5. **Move** cursor to destination
6. **A** → Paste

---

## Advanced Tips

### Navigation Speed

- Hold **B + arrows** to jump by larger increments
- **B + Left/Right** in Song View jumps between channels
- **B + Up/Down** jumps by configurable step size

### Value Entry

- **A (single press)** enters direct value entry mode
- Type hex values directly (on platforms with keyboard)
- **A + arrows** for incremental changes

### Tempo Control

- **RT + Up/Down** in Song View nudges tempo
- Useful for live tempo changes

### Table Automation

1. Create table with **CMD** values (e.g., `PTCH +01`)
2. Reference in Phrase with `TBLE 00` command
3. Table executes per-note, modulating parameters

---

## Synthesis UX (When Implemented)

### Future Wavetable Instrument View

**Expected Layout**:
```
INST 00  WAVETABLE
wave:    SINE
volume:  80
pan:     80
attack:  10
decay:   20
sustain: 60
release: 40
cutoff:  FF
reso:    00
[...]
```

**Navigation**: Same as Sample Instrument
**Parameters**:
- `wave`: Waveform selector (SINE, SAW, SQUARE, TRI, etc.)
- `attack`, `decay`, `sustain`, `release`: ADSR envelope
- `cutoff`, `reso`: Filter parameters
- Standard LGPT effects apply

### Future FM Instrument View

**Expected Layout**:
```
INST 00  FM_SYNTH
algo:    01 (4-op)
op1_lvl: 80
op1_rat: 1.00
op1_fb:  00
op2_lvl: 60
[...]
```

**Navigation**: Standard FieldView navigation
**Parameters**:
- `algo`: Algorithm (routing configuration)
- Per-operator: level, ratio, feedback
- Global envelope, filter

---

## Files Reference

### View Implementation Files

| View | Header | Implementation |
|------|--------|----------------|
| Song | `SongView.h` | `SongView.cpp` |
| Chain | `ChainView.h` | `ChainView.cpp` |
| Phrase | `PhraseView.h` | `PhraseView.cpp` |
| Instrument | `InstrumentView.h` | `InstrumentView.cpp` |
| Table | `TableView.h` | `TableView.cpp` |
| Project | `ProjectView.h` | `ProjectView.cpp` |
| Groove | `GrooveView.h` | `GrooveView.cpp` |
| Mixer | `MixerView.h` | `MixerView.cpp` |

### Base Classes

| Class | Purpose | File |
|-------|---------|------|
| `View` | Abstract base | `View.h/cpp` |
| `FieldView` | Field-based editing | `FieldView.h/cpp` |
| `ModalView` | Modal dialogs | `ModalView.h/cpp` |
| `UIField` | Individual field | `UIField.h/cpp` |

### Data Models

| Model | File |
|-------|------|
| `ViewData` | Shared view state | `ViewData.h/cpp` |
| `Project` | Project data | `Project.h/cpp` |
| `Song` | Song structure | `Song.h/cpp` |
| `Chain` | Chain data | `Chain.h/cpp` |
| `Phrase` | Phrase data | `Phrase.h/cpp` |

---

## Conclusion

LGPT's UX is built around:
- **Efficiency**: Button combinations for fast workflows
- **Consistency**: Same patterns across all views
- **Flexibility**: Selection/clipboard system
- **Performance-Friendly**: Live cueing and muting
- **Tracker Heritage**: Follows LSDJ/tracker conventions

The button-based interface is optimized for handheld devices, with thoughtful combinations that avoid conflicts and maintain muscle memory across contexts.

**New Recording Feature** integrates seamlessly: **LT + A** follows the existing pattern of LT = "extended action", fitting naturally into the Instrument View workflow.

---

**Document Version**: 1.0
**Last Updated**: 2025-10-05
**Based On**: Code analysis of LGPT source tree
**Platform**: RG Nano (Miyoo Mini compatible)
