#!/usr/bin/env python3
"""Exact key sweep: every case's end state against tests/golden/sweep.

Each sweep case (make_sweep.py) ends with state_record: the screen text,
cursor, view, layer, notification, what plays and every line of the song
that changed. The sim writes them to sweep-states.jsonl, which the runner
keeps in the case's artifacts folder.

    python tools/sweep/states.py sim-artifacts-suite            # compare
    python tools/sweep/states.py sim-artifacts-suite --update   # accept

A difference is printed as what changed, e.g.

    sweep-song  song__B+A
      model: -song 00 00 01 ...   +song 00 -- 01 ...
      screen line 5: '00 00 01 ...' -> '00 -- 01 ...'

--update writes the goldens (one file per sweep script) and prints the same
report, so the commit that changes a key's behaviour shows exactly how.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
GOLDEN = os.path.join(ROOT, "tests", "golden", "sweep")
FIELDS = ["view", "layer", "selected", "notification", "sound", "model", "screen", "pixel"]


def load_jsonl(path):
    records = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                r = json.loads(line)
                records[r["case"]] = r
    return records


def describe(old, new):
    out = []
    for field in FIELDS:
        a, b = old.get(field), new.get(field)
        if field in ("screen", "pixel"):
            # A case recorded without its screen (timing decides it) on
            # either side: nothing to compare
            if a is None or b is None or a == b:
                continue
            for i in range(max(len(a), len(b))):
                la = a[i] if i < len(a) else ""
                lb = b[i] if i < len(b) else ""
                if la != lb:
                    out.append("%s line %d: %r -> %r" % (field, i, la, lb))
        elif field == "model":
            if a != b:
                gone = [x for x in a if x not in b]
                came = [x for x in b if x not in a]
                out.append("model: expected %d changed line(s), got %d" % (len(a), len(b)))
                out += ["  not now: " + x for x in gone[:12]]
                out += ["  new:     " + x for x in came[:12]]
        elif a != b:
            out.append("%s: %r -> %r" % (field, a, b))
    return out


def collect(artifacts_root):
    """{script name: {case: record}} from every case folder"""
    found = {}
    for name in sorted(os.listdir(artifacts_root)):
        path = os.path.join(artifacts_root, name, "sweep-states.jsonl")
        if os.path.isfile(path):
            found[name] = load_jsonl(path)
    return found


def main(argv):
    update = "--update" in argv
    args = [a for a in argv if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    runs = collect(args[0])
    if not runs:
        print("no sweep-states.jsonl under %s" % args[0])
        return 2
    changed = 0
    missing = 0
    total = 0
    report = []
    for script, cases in runs.items():
        golden_path = os.path.join(GOLDEN, script + ".jsonl")
        golden = load_jsonl(golden_path) if os.path.exists(golden_path) else {}
        total += len(cases)
        for case, rec in cases.items():
            if case not in golden:
                report.append("%s  %s\n  new case (no golden)" % (script, case))
                changed += 1
                continue
            diff = describe(golden[case], rec)
            if diff:
                changed += 1
                report.append("%s  %s\n  %s" % (script, case, "\n  ".join(diff)))
        for case in golden:
            if case not in cases:
                # A case that failed its own checks is skipped by the sim:
                # its failure is in sweep-results.txt
                missing += 1
                report.append("%s  %s\n  no record (the case failed or was removed)" % (script, case))
        if update:
            os.makedirs(GOLDEN, exist_ok=True)
            with open(golden_path, "w", encoding="utf-8", newline="\n") as f:
                for case in cases:
                    f.write(json.dumps(cases[case], sort_keys=True) + "\n")
    print("\n".join(report))
    print("%d cases in %d scripts: %d differ from the goldens, %d without a record" % (
        total, len(runs), changed, missing))
    if update:
        print("goldens updated in %s" % os.path.relpath(GOLDEN, ROOT))
        return 0
    if changed or missing:
        print("EXACT SWEEP FAILED. If the new behaviour is right: "
              "python tools/sweep/states.py %s --update" % args[0])
        return 1
    print("EXACT SWEEP OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
