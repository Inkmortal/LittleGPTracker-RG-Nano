#!/usr/bin/env python3
"""Write a long random-play simulator script (a soak test for crashes and leaks).

Presses buttons the way a curious player does for a long time: every screen,
the helper, dialogs, playback, live mode, faders. Seeded, so a crash found
with one seed can be replayed exactly.

    python tools/make_soak_script.py --minutes 15 --seed 1
    powershell -File tools/run-rgnano-sim.ps1 -Script projects/resources/RGNANO_SIM/soak.rgsim -Mute -OpenDemo Dusk

Memory is logged every 30 s as [HEARTBEAT] lines in projects/rgnano-sim.log.
"""

from __future__ import annotations

import argparse
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "projects" / "resources" / "RGNANO_SIM" / "soak.rgsim"
DIRS = "udlr"


def combo(rng: random.Random, hold: str, key: str) -> list[str]:
    return [f"down {hold}", f"press {key} 60", f"up {hold}", f"wait {rng.randint(60, 250)}"]


def action(rng: random.Random) -> list[str]:
    kind = rng.choices(
        ["move", "screen", "edit", "play", "helper", "select", "back", "bnav", "lb", "wait"],
        weights=[30, 14, 14, 6, 4, 4, 6, 6, 4, 12])[0]
    d = rng.choice(DIRS)
    if kind == "move":
        return [f"press {d} 60", f"wait {rng.randint(40, 200)}"]
    if kind == "screen":
        # Not RB+Up: from Song that is the Project screen, whose buttons
        # quit or load songs and would end the run
        return combo(rng, "n", rng.choice("dlr"))
    if kind == "edit":
        return combo(rng, "a", d)
    if kind == "play":
        return ["press s 60", f"wait {rng.randint(300, 3000)}"]
    if kind == "helper":
        out = combo(rng, "n", "k")
        for _ in range(rng.randint(0, 3)):
            out += [f"press {rng.choice('ud')} 60", "wait 150"]
        return out + combo(rng, "n", "k")
    if kind == "select":
        # Song: live mode; Phrase/Table: command picker; sample: import
        return ["press k 60", f"wait {rng.randint(150, 600)}", f"press {d} 60", "wait 100"]
    if kind == "back":
        return ["press b 60", "wait 120"]
    if kind == "bnav":
        return combo(rng, "b", d)
    if kind == "lb":
        return combo(rng, "m", rng.choice(DIRS + "s"))
    return [f"wait {rng.randint(500, 4000)}"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=15)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    lines = [f"# Soak test, seed {args.seed}, about {args.minutes} minutes (tools/make_soak_script.py)",
             "wait 1500"]
    budget = args.minutes * 60000
    spent = 0
    while spent < budget:
        step = action(rng)
        lines += step
        spent += sum(int(x.split()[1]) for x in step if x.startswith("wait")) + 60 * len(step)
    lines += ["press b 60", "wait 200", "press b 60", "wait 200", "quit"]
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT} ({len(lines)} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
