"""What every key does on every screen, for the key sweep (make_sweep.py).

Every screen gets every key and two-key combo (KEY_UNIVERSE). A key not
listed for a screen must do nothing visible: "nothing is mapped twice on a
screen" and unmapped keys stay quiet (docs/rgnano-wiki/Controls.md).

Effects:
  N          nothing visible changes (the default)
  C          the screen changes, same screen, nothing opens
  V:<view>   goes to that screen (song, chain, phrase, instrument, table,
             groove, project, scale, mixer, fx, eq, limit)
  L:<layer>  opens something on top (helper, power, modal:<Dialog>, or
             "modal" for any dialog); the screen's `exits` must close it
  P          starts playback; pressing the same keys again stops it, then
             the sound must die away (no stuck notes)
  A:<layer>  like L, for a dialog that animates (a progress bar)
  X          depends on the song's state: only the generic checks (no
             crash, no hang, no clipped text, you can get back out)

Combos are written held-first: "B+A" holds B, then presses A. A combo is
tested in both orders; the reverse order is only held to the same effect
when the key held first does nothing on its own (otherwise it is X).

Every case starts from the Afterglow demo as saved (sim_reload_project) and
walks to the screen with real key presses (`enter`, from the Song screen,
cursor on row 00 track 1).

The effect above is what Controls.md promises. On top of it every case
records the exact state it ends in (state_record: screen text, cursor,
view, layer, notification, what plays, and every line of the song that
changed) and tools/sweep/states.py compares that with tests/golden/sweep:
the table says a key must change the screen, the golden says exactly how.
"""

SINGLE = ["U", "D", "L", "R", "A", "B", "LB", "RB", "START", "SEL", "X", "Y"]
MODIFIERS = ["A", "B", "LB", "RB"]
PARTNERS = ["U", "D", "L", "R", "A", "B", "LB", "RB", "START", "SEL", "X", "Y"]


def key_universe():
    keys = list(SINGLE)
    for m in MODIFIERS:
        for p in PARTNERS:
            if p != m:
                keys.append(m + "+" + p)
    return keys


KEY_UNIVERSE = key_universe()

# Shared by the tracker grids (Song, Chain, Phrase, Table, Groove)
MOVE = {"U": "C", "D": "C", "L": "C", "R": "C"}
HELPER = {"RB+SEL": "L:helper"}
UNDO = {"B+SEL": "X", "LB+SEL": "X"}  # nothing to undo/redo: may say so
# X copy, Y paste, LB+Y paste new copies / duplicate: every screen says what
# it did ("Copied chain 00", "Nothing to copy here"), so the screen changes
COPY = {"X": "C", "Y": "C", "LB+Y": "C"}

SCREENS = {
    "song": {
        "view": "song",
        "enter": ["R"],
        "cursor": True,
        # The CPU-load/elapsed-time HUD is drawn by a queued player update
        # (AppWindow::queuePlayerUpdate) the render thread hands to the UI
        # thread; whether that draw has landed by the time a stop is
        # captured is real scheduling, not app state worth pinning exactly
        "stop_noscreen": True,
        "keys": {
            **COPY,
            "U": "C",  # row 00: track moving
            "D": "C", "L": "C", "R": "C",
            "A": "X",
            "START": "P",
            "SEL": "C",  # Live mode
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "A+RB": "C",  # solo
            "A+SEL": "C",  # bookmark
            "A+LB": "X",
            "B+U": "N",  # already at the top
            "B+D": "C",
            "B+A": "C",  # delete the chain
            "B+RB": "C",  # mute
            "B+LB": "X",  # selection starts on the next move
            "LB+B": "X",
            "B+START": "X",
            "LB+U": "X", "LB+D": "X",
            "LB+L": "X", "LB+R": "X",  # tempo nudge: only while playing
            "LB+START": "X",  # launch the row live
            "RB+U": "V:project", "RB+D": "V:mixer", "RB+R": "V:chain",
            "RB+L": "V:rack",  # Rack: build and play your sounds
            "RB+START": "P",
            "RB+LB": "X", "RB+A": "X", "RB+B": "X",
            **UNDO, **HELPER,
        },
    },
    "chain": {
        "view": "chain",
        "enter": ["D", "RB+R"],  # chain 00, cursor on row 00: phrase 00
        "cursor": True,
        "keys": {
            **COPY,
            **MOVE,
            "U": "N", "L": "N",  # top-left edge
            "A": "X",
            "START": "P",
            "A+U": "C", "A+R": "C",
            "A+D": "N", "A+L": "N",  # phrase 00 is the lowest
            "A+RB": "C", "A+LB": "X",
            "B+U": "X", "B+D": "X", "B+L": "X", "B+R": "X",  # next chain
            "B+A": "C", "B+RB": "C",
            "B+LB": "X", "LB+B": "X",  # a selection shows once you move
            "LB+START": "A:modal:RenderToSampleDialog",
            "RB+L": "V:song", "RB+R": "V:phrase",
            "RB+START": "P", "RB+LB": "X", "RB+A": "X", "RB+B": "X",
            **UNDO, **HELPER,
        },
        "exits": {"modal:RenderToSampleDialog": ["B"]},
    },
    "phrase": {
        "view": "phrase",
        "enter": ["D", "RB+R", "RB+R"],
        "cursor": True,
        "keys": {
            **COPY,
            **MOVE,
            "U": "N", "L": "N",  # top-left edge
            "A": "X",
            "START": "P",
            "SEL": "X",  # command picker on a command column only
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "A+RB": "C", "A+LB": "X",
            "B+U": "X", "B+D": "X", "B+L": "X", "B+R": "X",
            "B+A": "X",
            "B+RB": "X",  # mute: shows on the strip only while playing
            "B+LB": "X",  # a selection shows once you move
            "LB+U": "X", "LB+D": "X", "LB+L": "X", "LB+R": "X",
            "LB+START": "A:modal:RenderToSampleDialog",
            "RB+L": "V:chain", "RB+R": "V:instrument",
            "RB+U": "V:groove", "RB+D": "V:table",
            "RB+START": "P", "RB+LB": "X", "RB+A": "X", "RB+B": "X",
            "LB+B": "X",  # grows a selection to rows; none here
            **UNDO, **HELPER,
        },
        "exits": {"modal:RenderToSampleDialog": ["B"]},
    },
    "instrument": {
        "view": "instrument",
        "enter": ["D", "RB+R", "RB+R", "RB+R"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "C", "D": "C", "L": "X", "R": "X",
            "A": "X",
            "START": "P",
            "SEL": "X",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "A+START": "P",  # hear it
            "B+L": "C", "B+R": "C", "B+U": "C", "B+D": "C",
            "B+A": "X",
            "LB+L": "C", "LB+R": "C",  # pages
            "LB+U": "X", "LB+D": "X",
            "RB+L": "V:phrase",
            "RB+U": "L:modal:InstrumentListDialog",
            "RB+D": "V:table",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
        "exits": {"modal:InstrumentListDialog": ["B"]},
    },
    "table": {
        "view": "table",
        "enter": ["D", "RB+R", "RB+R", "RB+D"],
        "cursor": True,
        "keys": {
            **COPY,
            **MOVE,
            "U": "N", "L": "N",  # top-left edge
            "A": "X",
            "START": "X",
            "SEL": "X",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "A+LB": "X",
            "B+U": "C", "B+D": "C", "B+L": "C", "B+R": "C",
            "A+D": "X",  # an empty command can't go lower
            "B+A": "X", "B+LB": "X", "LB+B": "X",
            "RB+U": "V:phrase", "RB+R": "V:table",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    "groove": {
        "view": "groove",
        "enter": ["D", "RB+R", "RB+R", "RB+U"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "C", "D": "C",
            "A": "X",
            "START": "X",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "B+L": "C", "B+R": "C", "B+U": "X", "B+D": "X",
            "B+A": "X",
            "RB+D": "V:phrase",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    "project": {
        "view": "project",
        "enter": ["RB+U"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "X", "D": "C",
            "A": "X",  # Save Song
            "START": "P",
            "A+L": "X", "A+R": "X", "A+U": "X", "A+D": "X",
            "B+A": "X",
            "RB+D": "V:song", "RB+R": "V:scale",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    "scale": {
        "view": "scale",
        "enter": ["RB+U", "RB+R"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "X", "D": "C", "L": "X", "R": "X",
            "A": "X",
            "START": "P",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "B+A": "C",
            "RB+L": "V:project",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    "mixer": {
        "view": "mixer",
        "enter": ["RB+D"],
        "cursor": True,
        # Same queued-draw CPU/timer HUD as "song" (MixerView.cpp draws its
        # own copy): real scheduling, not app state.
        "stop_noscreen": True,
        "keys": {
            **COPY,
            "L": "X", "R": "C",
            "START": "P",
            "A+U": "C",  # track 1 starts at C0/100%: Up raises it
            "A+D": "C", "A+L": "X", "A+R": "X",
            "A+RB": "C", "B+RB": "C",
            "B+A": "X",
            "RB+U": "V:song", "RB+D": "V:fx",
            "RB+START": "P", "RB+LB": "X",
            **UNDO, **HELPER,
        },
    },
    "fx": {
        "view": "fx",
        "enter": ["RB+D", "RB+D"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "X", "D": "C", "L": "X", "R": "C",
            "START": "P",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "B+A": "X",
            "RB+U": "V:mixer", "RB+R": "V:eq",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    "eq": {
        "view": "eq",
        "enter": ["RB+D", "RB+D", "RB+R"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "X", "D": "C", "L": "X", "R": "C",
            "START": "P",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "B+A": "X",
            "RB+L": "V:fx", "RB+R": "V:limit",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    "limit": {
        "view": "limit",
        "enter": ["RB+D", "RB+D", "RB+R", "RB+R"],
        "cursor": True,
        # The in/out dBFS readout is a live meter: exactly which buffer it
        # last measured before a key-press-timed Stop() lands is real
        # jitter, not a fixed value, so its digits after a stop aren't
        # worth pinning to the exact tenth of a dB
        "stop_noscreen": True,
        "keys": {
            **COPY,
            "U": "X", "D": "C", "L": "X", "R": "C",
            "START": "P",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "B+A": "X",
            "RB+L": "V:eq",
            "RB+START": "P",
            **UNDO, **HELPER,
        },
    },
    # Overlays and dialogs: `layer` is what must be on top once entered
    "helper": {
        "view": "song",
        "layer": "helper",
        "enter": ["R", "RB+SEL"],
        "cursor": False,
        "keys": {
            "U": "C", "D": "C",
            "A": "L:modal:GuideDialog",
            "B": "L:none",
            "RB+SEL": "L:none",
        },
        # The guide replaces the helper: B goes back to the screen
        # The guide replaces the helper: B back to its topics, B closes it,
        # back on the screen (not the helper)
        "exits": {"modal:GuideDialog": {"keys": ["B", "B"], "back_to": "none"}},
    },
    "guide": {
        "view": "song",
        "layer": "modal:GuideDialog",
        "enter": ["R", "RB+SEL", "A"],
        "cursor": False,
        "keys": {
            "U": "C", "D": "C", "L": "X", "R": "X",
            "A": "X",
            "B": "X",
            "START": "L:none",  # closes the guide
            "LB+U": "X", "LB+D": "X",
            "LB+L": "C", "LB+R": "C",  # previous / next page, wraps: always changes
            # Redo (View::ProcessButton) is blocked inside any modal
            # (IsModal()): LB alone does nothing on the guide, so nothing
            # to redo leaves the screen unchanged. B alone (held first, so
            # it fires on its own before Select joins it) already goes back
            # to Contents: **UNDO's "X" default covers B+SEL
            "LB+SEL": "N",
            "RB+SEL": "L:none",
        },
    },
    "power": {
        "view": "song",
        "layer": "power",
        "enter": ["R", "P"],
        "cursor": False,
        "plain_keys": True,
        "keys": {
            "U": "C", "D": "C",
            "L": "X", "R": "X",  # volume on the first item
            "A": "X",
            "B": "L:none",
            "RB+SEL": "L:power+help",
        },
        "exits": {"power+help": ["RB+SEL"]},
    },
    "instlist": {
        "view": "instrument",
        "layer": "modal:InstrumentListDialog",
        "enter": ["D", "RB+R", "RB+R", "RB+R", "RB+U"],
        "cursor": True,
        "keys": {
            "U": "N", "L": "N",  # first sound, first page
            "D": "C", "R": "C",
            "A": "L:none",
            "B": "L:none",
            "START": "X",
            "SEL": "X",
            "LB+A": "X",
            # My sounds, nested on top of the instrument list (not replacing it)
            "LB+START": "L:modal:InstrumentListDialog>SoundFilesDialog",
            # Redo is blocked inside any modal (IsModal()): LB alone does
            # nothing here, so nothing to redo leaves the screen unchanged.
            # B alone (held first) already closes the list: the default "X"
            # fallback (B maps to "L:none", not "N") covers B+SEL
            "LB+SEL": "N",
            "RB+SEL": "L:modal:InstrumentListDialog+helper",
        },
        "exits": {"modal:InstrumentListDialog+helper": ["RB+SEL"],
                   "modal:InstrumentListDialog>SoundFilesDialog": ["B"]},
    },
    "live": {
        "view": "song",
        "enter": ["R", "SEL"],
        "cursor": True,
        "keys": {
            **COPY,
            "U": "C", "D": "C", "L": "C", "R": "C",
            "SEL": "C",
            "START": "X", "LB+START": "X", "RB+START": "X", "B+START": "X",
            "A": "X",
            "A+U": "C", "A+D": "C", "A+L": "C", "A+R": "C",
            "A+RB": "C", "B+RB": "C",
            "A+SEL": "C",
            "B+U": "N",  # already at the top
            "B+D": "C", "B+A": "C", "B+LB": "X", "LB+B": "X",
            "LB+L": "X", "LB+R": "X", "LB+U": "X", "LB+D": "X",
            "RB+U": "V:project", "RB+D": "V:mixer", "RB+R": "V:chain",
            # Live shares the Song screen's RB grid (SongView::ProcessButton
            # is unconditional on RB+Left/Right/Up/Down): RB+Left also opens
            # the Rack from here
            "RB+L": "V:rack",
            "RB+LB": "X", "A+LB": "X", "RB+A": "X", "RB+B": "X",
            **UNDO, **HELPER,
        },
    },
    "rack": {
        "view": "rack",
        "enter": ["RB+L"],
        "cursor": True,
        "keys": {
            "U": "N",  # the first sound: nothing above
            "D": "C", "R": "C",  # next sound, next page
            "L": "N",  # first page
            "A": "X",  # plays while held
            "A+L": "X", "A+R": "X", "A+U": "X", "A+D": "X",  # note / octave, plays
            "LB+A": "C",  # copy to a free slot
            "START": "P",  # riff
            "SEL": "L:modal:SoundBrowserDialog",
            "LB+START": "L:modal:SoundFilesDialog",
            "RB+R": "V:instrument",
            "RB+D": "V:phrase",
            "RB+L": "C",  # says B goes back
            # RackView::ProcessButtonMask returns on any R-held combo it
            # doesn't name (R+Right/Down/Left), before ever reaching the
            # plain-START case that starts a riff: R+Start is a dead
            # combo here, not a second way to trigger it (that's "START"
            # above, matching the screen's own "START riff" hint).
            "RB+START": "N",
            "B": "V:song",
            "X": "C", "Y": "C", "LB+Y": "C",
            **UNDO, **HELPER,
        },
        "exits": {"modal:SoundBrowserDialog": ["B"], "modal:SoundFilesDialog": ["B"]},
    },
    "soundbrowser": {
        "view": "rack",
        "layer": "modal:SoundBrowserDialog",
        "enter": ["RB+L", "SEL"],
        "cursor": True,
        "keys": {
            "U": "X", "D": "C", "L": "X", "R": "X",
            # The cursor starts on the first category ("Synth presets"): A
            # opens it, which previews its first preset ("init") the same
            # way moving onto any sound does (Controls.md) -- it keeps
            # sounding for as long as that preset's own envelope takes
            "A": "C:preview",
            "B": "L:none",  # puts the slot back
            "RB+SEL": "L:modal:SoundBrowserDialog+helper",
        },
        "exits": {"modal:SoundBrowserDialog+helper": ["RB+SEL"]},
    },
    "soundfiles": {
        "view": "rack",
        "layer": "modal:SoundFilesDialog",
        "enter": ["RB+L", "LB+START"],
        "cursor": True,
        "keys": {
            "U": "X", "D": "C", "L": "X", "R": "X",
            "A": "X",
            "B": "L:none",
            "RB+SEL": "L:modal:SoundFilesDialog+helper",
        },
        "exits": {"modal:SoundFilesDialog+helper": ["RB+SEL"]},
    },
    "cmdpicker": {
        "view": "phrase",
        "layer": "modal:CommandSelectorModal",
        # Phrase 00, step 0: Right x2 is the first command column
        "enter": ["D", "RB+R", "RB+R", "R", "R", "SEL"],
        "cursor": True,
        "keys": {
            # The grid reads Up/Down/Left/Right alone (LB/RB held first
            # change nothing extra, so LB+dir / RB+dir land on the same
            # command as dir alone) and moving previews the newly selected
            # command's sound, same as the sound browser's category list
            "U": "C:preview", "D": "C:preview", "L": "C:preview", "R": "C:preview",
            "LB+U": "C:preview", "LB+D": "C:preview",
            "LB+L": "C:preview", "LB+R": "C:preview",
            "RB+U": "C:preview", "RB+D": "C:preview",
            "RB+L": "C:preview", "RB+R": "C:preview",
            "A": "L:none",  # takes the command
            "LB+A": "L:none", "RB+A": "L:none",  # A is read the same way
            "B": "L:none",  # puts back the command you had
            # But B (unlike A/the D-pad) is read normally: LB/RB+B isn't Back
            "LB+B": "N", "RB+B": "N",
            "RB+SEL": "L:modal:CommandSelectorModal+helper",
        },
        "exits": {"modal:CommandSelectorModal+helper": ["RB+SEL"]},
    },
    "render": {
        "view": "chain",
        "layer": "modal:RenderToSampleDialog",
        "enter": ["D", "RB+R", "LB+START"],
        "animated": True,  # a progress bar: never holds still
        "cursor": False,
        "keys": {
            "B": "L:none",
            "RB+SEL": "L:modal:RenderToSampleDialog+helper",
        },
        "exits": {"modal:RenderToSampleDialog+helper": ["RB+SEL"]},
    },
    "movetracks": {
        "view": "song",
        "enter": ["R", "U"],
        "cursor": True,
        # Same queued-draw HUD as "song" above: real, not app state.
        "stop_noscreen": True,
        "keys": {
            "L": "C", "R": "C",
            "D": "C", "B": "C",  # back to the grid
            "A+L": "C", "A+R": "C",
            "START": "P", "RB+START": "P",
            "RB+U": "V:project", "RB+D": "V:mixer",
            "RB+R": "X",
            # Pressing RB leaves track moving (design question: see report)
            "RB": "C", "A+RB": "X", "LB+RB": "X",
            "RB+L": "X", "RB+A": "X", "RB+B": "X", "RB+LB": "X",
            # X/Y copy and paste the chain under the song cursor while the
            # tracks are being moved (design question: see the test report)
            "X": "X", "Y": "X", "LB+Y": "X",
            **UNDO, **HELPER,
        },
    },
}

# Helpers need their own way out when a key opens something on top
DEFAULT_EXITS = {
    "helper": ["RB+SEL"],
    "power": ["P"],
    "power+help": ["RB+SEL", "P"],
    "confirm": ["B"],
    "modal": ["B"],
}

# Overlays that must stay still while a song plays: nothing may draw over
# them (play markers, meters). (screen entry, keys that open it)
PLAYBACK_OVERLAYS = [
    ("song", ["RB+SEL"]),
    ("chain", ["RB+SEL"]),
    ("phrase", ["RB+SEL"]),
    ("mixer", ["RB+SEL"]),
    ("instrument", ["RB+SEL"]),
    ("song", ["P"]),
    ("song", ["RB+SEL", "A"]),
    ("instrument", ["RB+U"]),
]
