#!/usr/bin/env python3
"""Write the key sweep scripts from table.py.

    python tools/sweep/make_sweep.py            # all screens, every combo
    python tools/sweep/make_sweep.py song chain # some
    python tools/sweep/make_sweep.py --quick    # the quick tier: single keys

The --quick scripts (sweep-quick-<screen>.rgsim) press every single key on
every screen: the `quick` tier of run-rgnano-sim-suite.ps1. The full ones
add every two-key combo.

One script per screen (projects/resources/RGNANO_SIM/sweep-<screen>.rgsim)
plus sweep-playback.rgsim. Each case reloads the Afterglow demo, walks to the
screen with real keys, presses one key or combo and checks the effect the
table promises, then the generic checks: no clipped text, a cursor where
there should be one, nothing left playing, a way back out. Run one with

    tools/run-rgnano-sim.ps1 -Script projects/resources/RGNANO_SIM/sweep-song.rgsim -Mute -OpenDemo Afterglow

and read the failures with tools/sweep/report.py.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import table  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "projects", "resources", "RGNANO_SIM")

SIM_KEY = {"A": "a", "B": "b", "L": "l", "R": "r", "U": "u", "D": "d",
           "LB": "m", "RB": "n", "START": "s", "SEL": "k", "P": "p",
           "X": "x", "Y": "y"}
STABLE = "wait_stable 2500"


def press(combo):
    keys = combo.split("+")
    for k in keys:
        if k not in SIM_KEY:
            raise ValueError("unknown key %s in %s" % (k, combo))
    held = keys[:-1]
    lines = ["down %s" % SIM_KEY[k] for k in held]
    lines.append("press %s 80" % SIM_KEY[keys[-1]])
    lines += ["up %s" % SIM_KEY[k] for k in reversed(held)]
    return lines


def effect_of(screen, combo):
    keys = screen["keys"]
    if combo in keys:
        return keys[combo]
    parts = combo.split("+")
    if len(parts) == 2 and screen.get("plain_keys"):
        # Menus drawn by the event manager read each key on its own: a
        # held LB/RB is ignored, the second key acts as if pressed alone
        first, second = parts
        if keys.get(first, "N") != "N":
            return "X"
        return keys.get(second, "N")
    if len(parts) == 2:
        first, second = parts
        # Held first, the key does its own thing before the combo exists
        if keys.get(first, "N") != "N":
            return "X"
        reverse = second + "+" + first
        if reverse in keys:
            return keys[reverse]
    return "N"


def exits_for(screen, layer):
    """(keys that close it, the layer you are back on or None = the screen's own)"""
    own = screen.get("exits", {})
    if layer in own:
        way = own[layer]
        if isinstance(way, dict):
            return way["keys"], way.get("back_to")
        return way, None
    if layer in table.DEFAULT_EXITS:
        return table.DEFAULT_EXITS[layer], None
    if layer.startswith("modal"):
        return table.DEFAULT_EXITS["modal"], None
    raise ValueError("no way out of %s" % layer)


# Screens whose grid scrolls sideways: text past the edge is by design
SCROLLS_SIDEWAYS = {"table"}


def clip_checked(screen, view):
    return view not in SCROLLS_SIDEWAYS


def settle(screen):
    # An animated dialog (a progress bar) never holds still: give it time
    return "wait 800" if screen.get("animated") else STABLE


def record(screen):
    # The exact state (tools/sweep/states.py compares it with the goldens);
    # an animated screen's text depends on timing, so only the rest counts
    return "state_record noscreen" if screen.get("animated") else "state_record"


def enter(screen):
    lines = ["sim_reload_project", "wait 300"]
    for combo in screen["enter"]:
        lines += press(combo)
        lines.append("wait 150")
    lines += [settle(screen),
              "expect_view %s" % screen["view"],
              "expect_layer %s" % screen.get("layer", "none"),
              # the song as the case begins: the record lists what changed
              "model_snap"]
    return lines


def case_lines(name, screen, combo):
    view = screen["view"]
    base = screen.get("layer", "none")
    effect = effect_of(screen, combo)
    out = ["", "# %s: %s" % (combo, effect), "case %s__%s" % (name, combo)]
    out += enter(screen)
    out.append("snap before")
    out += press(combo)
    wait = settle(screen)
    if effect == "N":
        out += [wait]
        if not screen.get("animated"):
            out.append("expect_unchanged before")
        out += ["expect_view %s" % view, "expect_layer %s" % base, record(screen)]
    elif effect == "C" or effect == "C:tail" or effect == "C:preview":
        # C:tail: the key auditions a sound and releases it in the same
        # press (the sound browser's take()), so the release envelope may
        # still be ringing when the screen has long since settled.
        # C:preview: the key opens a list whose first entry previews (the
        # sound browser's openCategory): by design it keeps sounding for as
        # long as that preset's own envelope takes (Controls.md: "every
        # sound plays as you move onto it"), so there is no fixed wait
        # after which silence is the right assertion; record what plays
        # instead of demanding it stop
        out += [wait, "expect_changed before",
                "expect_view %s" % view, "expect_layer %s" % base]
        if effect == "C:tail":
            out += ["wait 3500", "expect_player_running no"]
        elif effect == "C:preview":
            out.append("sound_snap")
        else:
            out.append("expect_player_running no")
        out.append(record(screen))
    elif effect.startswith("V:"):
        out += [STABLE, "expect_view %s" % effect[2:], "expect_layer none",
                "expect_player_running no", "state_record"]
    elif effect.startswith("L:") or effect.startswith("A:"):
        # A: an animated dialog (a progress bar): it never holds still.
        # A helper or other overlay opened on top of it (layer startswith
        # base rather than == base) doesn't stop it animating underneath,
        # so it inherits the same treatment - checked wait_stable there
        # timed out for exactly this (render__RB+SEL: the helper over the
        # render progress dialog).
        layer = effect[2:]
        animated = effect.startswith("A:") or (screen.get("animated") and layer.startswith(base))
        out += ["wait 800" if animated else STABLE, "expect_layer %s" % layer,
                "state_record noscreen" if animated else "state_record"]
        if clip_checked(screen, view):
            out.append("expect_no_clipping")
        final_animated = animated
        if layer != base and layer != "none":
            keys, back_to = exits_for(screen, layer)
            for k in keys:
                out += press(k) + ["wait 150"]
            back = back_to or base
            final_animated = screen.get("animated") and back.startswith(base)
            out += ["wait 800" if final_animated else STABLE, "expect_layer %s" % back]
        out += ["expect_view %s" % view]
        if not final_animated:
            # An animated dialog (rendering) legitimately still has the
            # player running for as long as it's on screen - real state,
            # not settled, so there's nothing fixed to assert here (the
            # generic "closing a modal stops the player" check only holds
            # once the dialog is actually gone).
            out.append("expect_player_running no")
    elif effect == "P":
        # sound_snap: what plays (a preview: which note of which sound).
        # Poll isRunning_ (wait_player, up to 10s) instead of a fixed wait:
        # some starts (a Rack riff) don't set it as fast as a normal Start
        # does, and a fixed 600ms was occasionally too short there.
        out += ["wait_player running 0 1", "expect_player_running yes", "sound_snap"]
        out += press(combo)
        out += ["wait 400", "expect_player_running no",
                # Echo and reverb tails die away, then nothing may sound
                "wait 3500", "reset_audio_stats", "wait 400",
                "expect_audio_silence 0",
                # The stop can leave a play-position HUD (percent/timecode)
                # or a meter's peak-hold mid fade-out: its removal is a
                # queued player update the UI thread draws when it gets
                # scheduled, which under several sims running at once can
                # take longer than the usual STABLE window (screen-text
                # flake between this case and its neighbor at -Jobs 4).
                # Record only once the screen has genuinely settled, with
                # more headroom than the default wait_stable. Even then,
                # some screens' post-stop readout (a live meter, a HUD a
                # queued player update draws) is real timing, not settled
                # app state: don't pin its exact text (table.py's
                # stop_noscreen), same as an animated dialog never holds
                # still for a full compare.
                "wait_stable 6000",
                "state_record noscreen" if screen.get("stop_noscreen") else record(screen)]
    elif effect == "X":
        out += ["wait 600", "state_record noscreen"]
    else:
        raise ValueError("unknown effect %s for %s %s" % (effect, name, combo))
    if clip_checked(screen, effect[2:] if effect.startswith("V:") else view):
        out.append("expect_no_clipping")
    if screen.get("cursor") and effect in ("N", "C") and base == "none":
        out.append("expect_cursor")
    return out


def script_for(name, quick=False):
    screen = table.SCREENS[name]
    for combo in screen["keys"]:
        if combo not in table.KEY_UNIVERSE and len(combo.split("+")) < 3:
            raise ValueError("%s: %s is not in the key universe" % (name, combo))
    keys = table.SINGLE if quick else table.KEY_UNIVERSE
    lines = ["# Generated by tools/sweep/make_sweep.py from tools/sweep/table.py:",
             "# every %s on the %s screen. Don't edit by hand." % (
                 "single key" if quick else "key and combo", name),
             "wait 1500", "soft_fail on"]
    for combo in keys:
        lines += case_lines(name, screen, combo)
    lines += ["", "case end", "expect_no_soft_failures", "quit"]
    return lines


def playback_script():
    lines = ["# Generated by tools/sweep/make_sweep.py: overlays and dialogs",
             "# opened while the song plays must stay still (nothing draws over them).",
             "wait 1500", "soft_fail on"]
    for screen_name, keys in table.PLAYBACK_OVERLAYS:
        screen = table.SCREENS[screen_name]
        name = "playback__%s__%s" % (screen_name, "_".join(keys))
        lines += ["", "case %s" % name]
        lines += enter(screen)
        lines += press("RB+START") + ["wait 800", "expect_player_running yes"]
        for k in keys:
            lines += press(k) + ["wait 300"]
        lines += ["wait 900", "expect_layer modal" if keys[-1] in ("A", "RB+U")
                  else "expect_layer %s" % ("power" if keys == ["P"] else "helper"),
                  "snap still", "wait 1200", "expect_unchanged still",
                  "expect_player_running yes"]
    lines += ["", "case end", "expect_no_soft_failures", "quit"]
    return lines


def main(argv):
    quick = "--quick" in argv
    argv = [a for a in argv if a != "--quick"]
    names = argv or list(table.SCREENS)
    for name in names:
        path = os.path.join(OUT, "sweep-%s%s.rgsim" % ("quick-" if quick else "", name))
        with open(path, "w", newline="\n") as f:
            f.write("\n".join(script_for(name, quick)) + "\n")
        print("wrote %s (%d cases)" % (os.path.relpath(path, ROOT),
              len(table.SINGLE if quick else table.KEY_UNIVERSE)))
    if not argv and not quick:
        path = os.path.join(OUT, "sweep-playback.rgsim")
        with open(path, "w", newline="\n") as f:
            f.write("\n".join(playback_script()) + "\n")
        print("wrote %s (%d cases)" % (os.path.relpath(path, ROOT), len(table.PLAYBACK_OVERLAYS)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
