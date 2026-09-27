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
"""

SINGLE = ["U", "D", "L", "R", "A", "B", "LB", "RB", "START", "SEL"]
MODIFIERS = ["A", "B", "LB", "RB"]
PARTNERS = ["U", "D", "L", "R", "A", "B", "LB", "RB", "START", "SEL"]


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

SCREENS = {
    "song": {
        "view": "song",
        "enter": ["R"],
        "cursor": True,
        "keys": {
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
        "keys": {
            "L": "X", "R": "C",
            "START": "P",
            "A+U": "N",  # track 1 is at the top (FF)
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
        "keys": {
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
            "B": "X",
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
            "RB+SEL": "X",
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
            "RB+SEL": "L:modal:InstrumentListDialog+helper",
        },
        "exits": {"modal:InstrumentListDialog+helper": ["RB+SEL"]},
    },
    "live": {
        "view": "song",
        "enter": ["R", "SEL"],
        "cursor": True,
        "keys": {
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
            "RB+LB": "X", "A+LB": "X", "RB+A": "X", "RB+B": "X",
            **UNDO, **HELPER,
        },
    },
    "movetracks": {
        "view": "song",
        "enter": ["R", "U"],
        "cursor": True,
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
