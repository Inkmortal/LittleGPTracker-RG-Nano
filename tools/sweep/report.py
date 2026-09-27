#!/usr/bin/env python3
"""Summarise key sweep failures.

    python tools/sweep/report.py                 # sim-artifacts-suite
    python tools/sweep/report.py sim-artifacts-sweep
    python tools/sweep/report.py -v              # with the screen at each failure
    python tools/sweep/report.py sweep-results.txt

Each sweep run writes sweep-results.txt (the sim log only keeps its last
1 MB); the runner moves it into the case's artifacts folder.
"""

import glob
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main(argv):
    verbose = "-v" in argv
    paths = [a for a in argv if a != "-v"]
    if not paths:
        paths = [os.path.join(ROOT, "sim-artifacts-suite")]
    # A folder: the suite's artifacts (one sweep-<screen> folder per script)
    found = []
    for p in paths:
        if os.path.isdir(p):
            found += sorted(glob.glob(os.path.join(p, "sweep-*", "sweep-results.txt")))
        else:
            found.append(p)
    paths = found
    if not paths:
        print("no sweep results")
        return 1
    total = 0
    for path in paths:
        print("== %s" % os.path.relpath(path, ROOT))
        for line in open(path, encoding="utf-8", errors="replace").read().splitlines():
            if line.startswith("FAIL "):
                total += 1
                print(" - " + line[5:])
            elif line.startswith("    "):
                if verbose:
                    print("   " + line)
            elif line.strip():
                print("   " + line)
    print("%d failures" % total)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
