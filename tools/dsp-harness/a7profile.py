"""Cost per function (or per source line) of a harness run under the a7cost
plugin.

    python3 tools/dsp-harness/a7profile.py [--top N] [--lines FUNCTION]
                                           [--blocks FILE] [--binary FILE]

--blocks: the per-block costs the plugin wrote (default
projects/buildRGNANO/harness/engine_room.blocks); --binary: the harness
program (default projects/buildRGNANO/harness/engine_room_check);
--lines: break one function (a substring of its name) down by source line
(the device objects are built with -g). Code in shared libraries (libc,
libm, SDL) is named from <blocks>.maps (the harness saves its memory map)
when present. Run in WSL, where the SDK's binutils are.
"""

import argparse
import bisect
import os
import subprocess


def symbols(nm, path, dynamic=False):
    cmd = [nm, "-n", "-C"] + (["-D"] if dynamic else []) + [path]
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[0] and len(parts[1]) == 1 and parts[1] in "tTwWiI":
            syms.append((int(parts[0], 16), parts[2]))
    syms.sort()
    return syms


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    proj = os.path.join(here, "..", "..", "projects")
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--lines", default=None)
    ap.add_argument("--blocks", default=os.path.join(proj, "buildRGNANO/harness/engine_room.blocks"))
    ap.add_argument("--binary", default=os.path.join(proj, "buildRGNANO/harness/engine_room_check"))
    args = ap.parse_args()
    bindir = os.path.join(proj, "../sdk/FunKey-sdk-DrUm78/bin/arm-funkey-linux-musleabihf-")

    # the program, then each shared object from the saved memory map
    regions = []   # (start, end, base, syms, label)
    syms = symbols(bindir + "nm", args.binary)
    if syms:
        regions.append((0, syms[-1][0] + 0x100000, 0, syms, None))
    maps = args.blocks + ".maps"
    if os.path.exists(maps):
        seen = {}
        for line in open(maps):
            f = line.split()
            if len(f) < 6 or "x" not in f[1] or not f[5].startswith("/"):
                continue
            lo, hi = (int(x, 16) for x in f[0].split("-"))
            path = f[5]
            if os.path.abspath(path) == os.path.abspath(args.binary):
                continue
            base = lo - int(f[2], 16)
            if path not in seen:
                seen[path] = symbols(bindir + "nm", path, dynamic=True)
            regions.append((lo, hi, base, seen[path], os.path.basename(path)))

    def name_of(addr):
        for lo, hi, base, s, label in regions:
            if lo <= addr < hi:
                off = addr - base
                keys = [a for a, _ in s]
                i = bisect.bisect_right(keys, off) - 1
                if i >= 0:
                    return s[i][1] + (f" [{label}]" if label else "")
                return f"<{label or 'program'}>"
        return "<unknown>"

    blocks = []
    total = 0
    for line in open(args.blocks):
        a, c, _ = line.split()
        blocks.append((int(a, 16), int(c)))
        total += int(c)
    print(f"total cost {total}")

    if args.lines:
        mine = [(a, c) for a, c in blocks if args.lines in name_of(a)]
        sub = sum(c for _, c in mine)
        # one line per address (the innermost, for inlined code)
        inp = "\n".join(hex(a) for a, _ in mine) + "\n"
        out = subprocess.run([bindir + "addr2line", "-e", args.binary], input=inp,
                             capture_output=True, text=True).stdout.splitlines()
        lines = {}
        for (a, c), r in zip(mine, out):
            key = r.split("/sources/")[-1] if "/sources/" in r else r
            lines[key] = lines.get(key, 0) + c
        print(f"{args.lines}: {100.0 * sub / total:.2f}% of the total")
        for k, c in sorted(lines.items(), key=lambda kv: -kv[1])[:args.top]:
            print(f"{100.0 * c / sub:6.2f}%  {c:14d}  {k}")
        return

    cost = {}
    for a, c in blocks:
        n = name_of(a)
        cost[n] = cost.get(n, 0) + c
    for n, c in sorted(cost.items(), key=lambda kv: -kv[1])[:args.top]:
        print(f"{100.0 * c / total:6.2f}%  {c:14d}  {n}")


if __name__ == "__main__":
    main()
