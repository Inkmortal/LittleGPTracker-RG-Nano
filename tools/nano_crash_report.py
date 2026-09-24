#!/usr/bin/env python3
"""Read a crash report from the RG Nano and name the code it died in.

The app writes Applications/lgpt-rgnano-crash.txt when it crashes (signal,
pc/lr, return addresses found on the stack, the last actions) and keeps the
previous run's log as lgpt-rgnano.log.prev. The installer archives each
build's ELF as build/elf/<commit>.elf, so any report can be symbolized.

Usage:
    python tools/nano_crash_report.py            # card mounted as D:
    python tools/nano_crash_report.py --card E:
    python tools/nano_crash_report.py --file some-crash.txt
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ADDR2LINE = "sdk/FunKey-sdk-DrUm78/bin/arm-funkey-linux-musleabihf-addr2line"


def wsl_path(p: Path) -> str:
    p = p.resolve()
    return f"/mnt/{p.drive[0].lower()}/" + "/".join(p.parts[1:])


def symbolize(elf: Path, addrs: list[str]) -> list[str]:
    cmd = f"'{wsl_path(ROOT / ADDR2LINE)}' -e '{wsl_path(elf)}' -f -C -p " + " ".join(addrs)
    out = subprocess.run(["wsl", "-e", "bash", "-lc", cmd], capture_output=True, text=True)
    return out.stdout.splitlines()


def report(text: str) -> None:
    for block in text.split("=== CRASH ===")[1:]:
        build = re.search(r"^build (\S+)", block, re.M)
        commit = build.group(1) if build else "unknown"
        print("=== CRASH ===")
        for line in block.splitlines():
            if line.startswith(("signal", "uptime", "fault", "build")):
                print(line)
        elf = ROOT / "build" / "elf" / f"{commit}.elf"
        if not elf.exists():
            elf = ROOT / "projects" / "lgpt-rgnano.elf.debug"
            print(f"(no archived ELF for {commit}; using the current build, lines may be off)")
        named = []
        for label in ("pc", "lr"):
            m = re.search(rf"^{label} (0x[0-9a-f]+)", block, re.M)
            if m:
                named.append((label, m.group(1)))
        stack = re.findall(r"^  (0x[0-9a-f]+)$", block, re.M)
        named += [("stack", a) for a in stack]
        if named:
            lines = symbolize(elf, [a for _, a in named])
            for (label, addr), line in zip(named, lines):
                print(f"{label:5} {addr}  {line}")
        trail = block[block.find("last actions"):] if "last actions" in block else ""
        print(trail.strip())
        print()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--card", default="D:")
    ap.add_argument("--file", type=Path)
    args = ap.parse_args()
    crash = args.file or Path(args.card + "/Applications/lgpt-rgnano-crash.txt")
    if not crash.exists():
        print(f"no crash report at {crash}")
    else:
        report(crash.read_text(errors="replace"))
    if not args.file:
        for name in ("lgpt-rgnano.log.prev", "lgpt-rgnano.log"):
            log = Path(args.card + "/Applications/" + name)
            if log.exists():
                lines = log.read_text(errors="replace").splitlines()
                beats = [l for l in lines if "[HEARTBEAT]" in l]
                print(f"--- {name}: {len(lines)} lines, last heartbeats:")
                for l in beats[-6:]:
                    print("  " + l)
                print("  last lines:")
                for l in lines[-12:]:
                    print("  " + l)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
