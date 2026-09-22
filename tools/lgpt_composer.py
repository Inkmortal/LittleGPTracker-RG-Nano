#!/usr/bin/env python3
"""Compose LittleGPTracker projects from Python and write real lgptsav.dat files.

Why this exists: demo songs and test fixtures should be written with the
same musical care a person would use in the tracker, then loaded by the app
exactly like a saved project. Everything here maps 1:1 onto tracker data:

    Song row  -> 8 chain slots (one per track)
    Chain     -> up to 16 (phrase, transpose) steps, one phrase = one bar
    Phrase    -> 16 steps of (note, instrument, cmd1, param1, cmd2, param2)
    Step      -> one 16th note at the default groove (6 ticks)

Note names follow the LGPT screen: C3 == MIDI 60 (the value the app shows as
"C 3"). A3 is 440 Hz.

Step notation for phrases (16 tokens, whitespace separated, '|' ignored):

    C3        note C3 with the phrase's default instrument
    C3@05     note C3 with instrument 05 (hex)
    C3:60     note C3 with VOLM 0060 (volume 0x60) on command 1
    C3~       note C3 with CHRD from the chord map passed to the builder
    .         nothing on this step (the previous note keeps sounding)
    -         note off (KILL 0000)
    x / X / o drum hit / accent / ghost (see Phrase.drums)

Run `python tools/lgpt_composer.py --help` for the demo build commands.
"""

from __future__ import annotations

import argparse
import wave
import importlib.util
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

ROOT = Path(__file__).resolve().parents[1]

NOTE_INDEX = {
    "C": 0, "C#": 1, "DB": 1, "D": 2, "D#": 3, "EB": 3, "E": 4, "F": 5,
    "F#": 6, "GB": 6, "G": 7, "G#": 8, "AB": 8, "A": 9, "A#": 10, "BB": 10, "B": 11,
}
NAMES_SHARP = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

CHORD_SHAPES = {
    "maj": (0, 4, 7),
    "min": (0, 3, 7),
    "dim": (0, 3, 6),
    "sus2": (0, 2, 7),
    "sus4": (0, 5, 7),
    "maj7": (0, 4, 7, 11),
    "min7": (0, 3, 7, 10),
    "7": (0, 4, 7, 10),
    "min9": (0, 3, 7, 10, 14),
    "maj9": (0, 4, 7, 11, 14),
    "add9": (0, 4, 7, 14),
    "5": (0, 7),
}

NO_NOTE = 0xFF
NO_INSTR = 0xFF
CMD_NONE = "----"


def _pitch_class(letter: str, accidental: str) -> int:
    pc = NOTE_INDEX[letter.upper()]
    if accidental == "#":
        pc += 1
    elif accidental == "b":
        pc -= 1
    return pc % 12


def note(name: str) -> int:
    """'C3' -> 60, 'A#2' -> 46, 'Eb4' -> 75 (LGPT octave numbering)."""
    m = re.fullmatch(r"([A-Ga-g])([#b]?)(-?\d)", name.strip())
    if not m:
        raise ValueError(f"bad note name {name!r}")
    octave = int(m.group(3))
    # B#/Cb cross an octave boundary; keep them relative to the written letter
    base = NOTE_INDEX[m.group(1).upper()] + {"#": 1, "b": -1, "": 0}[m.group(2)]
    return (octave + 2) * 12 + base


def note_name(value: int) -> str:
    return f"{NAMES_SHARP[value % 12]}{value // 12 - 2}"


def parse_chord(symbol: str) -> tuple[int, tuple[int, ...]]:
    """'Am' / 'Fmaj7' / 'G' / 'C#min7' -> (pitch class, intervals)."""
    m = re.fullmatch(r"([A-G])([#b]?)(.*)", symbol)
    if not m:
        raise ValueError(f"bad chord {symbol!r}")
    root = _pitch_class(m.group(1), m.group(2))
    quality = m.group(3) or "maj"
    aliases = {"m": "min", "m7": "min7", "M7": "maj7", "m9": "min9", "M9": "maj9"}
    quality = aliases.get(quality, quality)
    if quality not in CHORD_SHAPES:
        raise ValueError(f"unknown chord quality {quality!r} in {symbol!r}")
    return root, CHORD_SHAPES[quality]


def voice_chord(symbol: str, center: int, previous: Sequence[int] | None = None,
                max_span: int = 15) -> list[int]:
    """Pick the inversion of a chord closest to `previous` (smooth voice leading).

    Returns absolute MIDI notes, ascending, spanning at most `max_span`
    semitones so the whole voicing fits one CHRD command (nibbles 1..F).
    """
    root, shape = parse_chord(symbol)
    pcs = [(root + i) % 12 for i in shape]
    best: list[int] | None = None
    best_cost = None
    for bass in range(center - 12, center + 12):
        if bass % 12 not in pcs:
            continue
        voicing = [bass]
        for pc in pcs:
            if pc == bass % 12:
                continue
            n = bass + ((pc - bass) % 12)
            voicing.append(n)
        voicing.sort()
        if voicing[-1] - voicing[0] > max_span:
            continue
        if previous:
            cost = sum(min(abs(v - p) for p in previous) for v in voicing)
            cost += abs(sum(voicing) / len(voicing) - center) * 0.25
        else:
            cost = abs(sum(voicing) / len(voicing) - center)
        if best_cost is None or cost < best_cost:
            best, best_cost = voicing, cost
    if best is None:
        raise ValueError(f"cannot voice {symbol} near {note_name(center)}")
    return best


def chrd_param(voicing: Sequence[int]) -> int:
    """CHRD parameter for notes above the lowest voicing note."""
    offsets = [n - voicing[0] for n in voicing[1:]]
    if len(offsets) > 4 or any(o < 1 or o > 15 for o in offsets):
        raise ValueError(f"voicing {voicing} does not fit CHRD")
    value = 0
    for o in offsets:
        value = (value << 4) | o
    return value


def fourcc_hex(name: str) -> str:
    if len(name) != 4:
        raise ValueError(f"command must be 4 chars: {name!r}")
    return "".join(f"{ord(c):02X}" for c in name)


@dataclass(frozen=True)
class Step:
    note: int = NO_NOTE
    instr: int = NO_INSTR
    cmd1: str = CMD_NONE
    param1: int = 0
    cmd2: str = CMD_NONE
    param2: int = 0


class Phrase:
    """16 steps of tracker data."""

    def __init__(self, steps: Iterable[Step] | None = None):
        self.steps: list[Step] = list(steps) if steps else [Step() for _ in range(16)]
        if len(self.steps) != 16:
            raise ValueError("a phrase has exactly 16 steps")

    def key(self) -> tuple:
        return tuple(self.steps)

    def set(self, index: int, **kwargs) -> "Phrase":
        current = self.steps[index]
        data = dict(note=current.note, instr=current.instr, cmd1=current.cmd1,
                    param1=current.param1, cmd2=current.cmd2, param2=current.param2)
        data.update(kwargs)
        self.steps[index] = Step(**data)
        return self

    def command(self, index: int, cmd: str, param: int) -> "Phrase":
        """Put a command in the first free command slot of a step."""
        current = self.steps[index]
        if current.cmd1 == CMD_NONE:
            return self.set(index, cmd1=cmd, param1=param)
        if current.cmd2 == CMD_NONE:
            return self.set(index, cmd2=cmd, param2=param)
        raise ValueError(f"step {index} already has two commands")

    def copy(self) -> "Phrase":
        return Phrase(self.steps)

    @staticmethod
    def parse(text: str, instr: int | None, chords: dict[int, int] | None = None) -> "Phrase":
        """instr=None leaves the instrument column empty: the note keeps the
        channel's running instrument without a clean restart, so filter/volume
        ramps started earlier keep moving."""
        tokens = [t for t in text.replace("|", " ").split() if t]
        if len(tokens) != 16:
            raise ValueError(f"expected 16 steps, got {len(tokens)}: {text!r}")
        phrase = Phrase()
        for i, tok in enumerate(tokens):
            if tok == ".":
                continue
            if tok == "-":
                phrase.set(i, cmd1="KILL", param1=0)
                continue
            m = re.fullmatch(r"([A-Ga-g][#b]?-?\d)(@[0-9A-Fa-f]{2})?(:[0-9A-Fa-f]{2})?(~)?", tok)
            if not m:
                raise ValueError(f"bad step token {tok!r}")
            if m.group(2):
                step_instr = int(m.group(2)[1:], 16)
            else:
                step_instr = NO_INSTR if instr is None else instr
            phrase.set(i, note=note(m.group(1)), instr=step_instr)
            if m.group(3):
                phrase.command(i, "VOLM", int(m.group(3)[1:], 16))
            if m.group(4):
                if not chords or i not in chords:
                    raise ValueError(f"step {i} asks for a chord but none was given")
                phrase.command(i, "CHRD", chords[i])
        return phrase

    @staticmethod
    def drums(pattern: str, instr: int, pitch: int = 60, accent: int | None = None,
              normal: int | None = None, ghost: int = 0x40) -> "Phrase":
        """'x...x...x...x...' -> hits. X=accent volume, o=ghost volume."""
        pattern = pattern.replace("|", "").replace(" ", "")
        if len(pattern) != 16:
            raise ValueError(f"drum pattern needs 16 steps: {pattern!r}")
        phrase = Phrase()
        for i, ch in enumerate(pattern):
            if ch == ".":
                continue
            phrase.set(i, note=pitch, instr=instr)
            if ch == "X" and accent is not None:
                phrase.command(i, "VOLM", accent)
            elif ch == "x" and normal is not None:
                phrase.command(i, "VOLM", normal)
            elif ch == "o":
                phrase.command(i, "VOLM", ghost)
        return phrase

    @staticmethod
    def merge(*phrases: "Phrase") -> "Phrase":
        """Overlay phrases; later phrases win on steps that have a note."""
        out = Phrase()
        for p in phrases:
            for i, s in enumerate(p.steps):
                if s != Step():
                    out.steps[i] = s
        return out


@dataclass
class Instrument:
    type: str
    params: dict[str, object] = field(default_factory=dict)


class Project:
    def __init__(self, name: str, tempo: int = 120):
        self.name = name
        self.params: dict[str, str] = {
            "tempo": str(tempo),
            "master": "100",
            "pregain": "100",
            "softclip": "Bypass",
            "softclipGain": "[unity]",
            "wrap": "false",
            "transpose": "0",
            "scaleKey": "-1",
            "scale": "None (Chromatic)",
            "noteNames": "Sharps",
            "renderMode": "Off",
            "midi": "(null)",
        }
        self.instruments: dict[int, Instrument] = {}
        self._phrases: list[Phrase] = []
        self._phrase_index: dict[tuple, int] = {}
        self._chains: list[tuple[tuple[int, ...], tuple[int, ...]]] = []
        self._chain_index: dict[tuple, int] = {}
        self.song = [[0xFF] * 8 for _ in range(256)]
        self.tables: dict[int, list[tuple[str, int, str, int, str, int]]] = {}
        self.grooves: dict[int, list[int]] = {0: [6, 6]}
        self.sample_files: dict[str, Path] = {}

    # --- project settings -------------------------------------------------
    def set(self, name: str, value: object) -> None:
        if name not in self.params:
            raise KeyError(name)
        self.params[name] = str(value)

    def key(self, root: str, scale: str) -> None:
        self.params["scaleKey"] = str(_pitch_class(root[0], root[1:]))
        self.params["scale"] = scale

    def groove(self, index: int, ticks: Sequence[int]) -> None:
        self.grooves[index] = list(ticks)

    # --- instruments ------------------------------------------------------
    def synth(self, slot: int, preset: str, **params: object) -> int:
        """Synth instrument from a preset plus overrides (param names use '_' for spaces)."""
        values: dict[str, object] = {"preset": preset}
        for k, v in params.items():
            values[k.replace("_", " ")] = v
        self.instruments[slot] = Instrument("Synth", values)
        return slot

    def sample(self, slot: int, wav: Path, root: int | str, volume: int = 0x80, **params: object) -> int:
        """Sample instrument playing a WAV that is copied into the project's
        samples folder. root is the note the recording plays at (C3 = 60, or
        a name like "A3"); other params use the save names with '_' for spaces
        (e.g. reverb=0x60, delay=0x20, loopmode="none")."""
        wav = Path(wav)
        with wave.open(str(wav), "rb") as w:
            frames = w.getnframes()
        root_note = note(root) if isinstance(root, str) else int(root)
        values: dict[str, object] = {
            "sample": wav.name,
            "volume": volume,
            "root note": root_note,
            "loopmode": "none",
            "start": 0,
            "end": frames,
        }
        for k, v in params.items():
            values[k.replace("_", " ")] = v
        self.instruments[slot] = Instrument("Sample", values)
        self.sample_files[wav.name] = wav
        return slot

    def table(self, index: int, rows: Sequence[tuple]) -> int:
        """Rows of (cmd1, p1[, cmd2, p2[, cmd3, p3]])."""
        full = []
        for r in rows:
            r = list(r) + [CMD_NONE, 0] * 3
            full.append(tuple(r[:6]))
        while len(full) < 16:
            full.append((CMD_NONE, 0, CMD_NONE, 0, CMD_NONE, 0))
        self.tables[index] = full[:16]
        return index

    # --- sequencing -------------------------------------------------------
    def phrase(self, phrase: Phrase) -> int:
        k = phrase.key()
        if k not in self._phrase_index:
            if len(self._phrases) >= 0xFE:
                raise RuntimeError("out of phrases")
            self._phrase_index[k] = len(self._phrases)
            self._phrases.append(phrase.copy())
        return self._phrase_index[k]

    def chain(self, phrases: Sequence[Phrase | int], transpose: Sequence[int] | None = None) -> int:
        ids = [p if isinstance(p, int) else self.phrase(p) for p in phrases]
        if not 1 <= len(ids) <= 16:
            raise ValueError("a chain holds 1..16 phrases")
        tr = list(transpose) if transpose else [0] * len(ids)
        k = (tuple(ids), tuple(t & 0xFF for t in tr))
        if k not in self._chain_index:
            if len(self._chains) >= 0x80:
                raise RuntimeError("out of chains")
            self._chain_index[k] = len(self._chains)
            self._chains.append(k)
        return self._chain_index[k]

    def row(self, index: int, chains: Sequence[int | None]) -> None:
        if len(chains) != 8:
            raise ValueError("a song row has 8 tracks")
        self.song[index] = [0xFF if c is None else c for c in chains]

    # --- output -----------------------------------------------------------
    def _hex_rows(self, data: bytes, tag: str, indent: str) -> str:
        out = [f"{indent}<{tag}>"]
        for i in range(0, len(data), 64):
            chunk = data[i:i + 64]
            if len(set(chunk)) == 1:
                out.append(f'{indent}    <DATA VALUE="{chunk[0]}" LENGTH="{len(chunk)}" />')
            else:
                out.append(f"{indent}    <DATA>{chunk.hex().upper()}</DATA>")
        out.append(f"{indent}</{tag}>")
        return "\n".join(out)

    def to_xml(self) -> str:
        song = bytes(v for row in self.song for v in row)
        chains = bytearray([0xFF] * (0xFF * 16))
        transposes = bytearray(0xFF * 16)
        for ci, (ids, tr) in enumerate(self._chains):
            for j, (pid, t) in enumerate(zip(ids, tr)):
                chains[ci * 16 + j] = pid
                transposes[ci * 16 + j] = t
        notes = bytearray([0xFF] * (0xFF * 16))
        instrs = bytearray([0xFF] * (0xFF * 16))
        cmd1 = bytearray(bytes.fromhex(fourcc_hex(CMD_NONE)) * (0xFF * 16))
        cmd2 = bytearray(cmd1)
        par1 = bytearray(0xFF * 16 * 2)
        par2 = bytearray(0xFF * 16 * 2)
        for pi, phrase in enumerate(self._phrases):
            for j, s in enumerate(phrase.steps):
                k = pi * 16 + j
                notes[k] = s.note
                instrs[k] = s.instr
                cmd1[k * 4:k * 4 + 4] = bytes.fromhex(fourcc_hex(s.cmd1))
                cmd2[k * 4:k * 4 + 4] = bytes.fromhex(fourcc_hex(s.cmd2))
                par1[k * 2:k * 2 + 2] = s.param1.to_bytes(2, "little")
                par2[k * 2:k * 2 + 2] = s.param2.to_bytes(2, "little")

        lines = ["<LITTLEGPTRACKER>", '    <PROJECT VERSION="1">']
        for k, v in self.params.items():
            lines.append(f'        <PARAMETER NAME="{k}" VALUE="{v}" />')
        lines.append("    </PROJECT>")
        lines.append("    <SONG>")
        ind = "        "
        lines.append(self._hex_rows(song, "SONG", ind))
        lines.append(self._hex_rows(bytes(chains), "CHAINS", ind))
        lines.append(self._hex_rows(bytes(transposes), "TRANSPOSES", ind))
        lines.append(self._hex_rows(bytes(notes), "NOTES", ind))
        lines.append(self._hex_rows(bytes(instrs), "INSTRUMENTS", ind))
        lines.append(self._hex_rows(bytes(cmd1), "COMMAND1", ind))
        lines.append(self._hex_rows(bytes(par1), "PARAM1", ind))
        lines.append(self._hex_rows(bytes(cmd2), "COMMAND2", ind))
        lines.append(self._hex_rows(bytes(par2), "PARAM2", ind))
        lines.append("    </SONG>")
        lines.append("    <INSTRUMENTBANK>")
        for slot in sorted(self.instruments):
            inst = self.instruments[slot]
            lines.append(f'        <INSTRUMENT ID="{slot:02X}" TYPE="{inst.type}">')
            for k, v in inst.params.items():
                if isinstance(v, bool):
                    v = "true" if v else "false"
                lines.append(f'            <PARAM NAME="{k}" VALUE="{v}" />')
            lines.append("        </INSTRUMENT>")
        lines.append("    </INSTRUMENTBANK>")
        lines.append("    <TABLES>")
        for ti in sorted(self.tables):
            rows = self.tables[ti]
            lines.append(f'        <TABLE ID="{ti:02X}">')
            for ci, (cname, pname) in enumerate((("CMD1", "PARAM1"), ("CMD2", "PARAM2"), ("CMD3", "PARAM3"))):
                cbytes = b"".join(bytes.fromhex(fourcc_hex(r[ci * 2])) for r in rows)
                pbytes = b"".join(int(r[ci * 2 + 1]).to_bytes(2, "little") for r in rows)
                lines.append(self._hex_rows(cbytes, cname, ind + "    "))
                lines.append(self._hex_rows(pbytes, pname, ind + "    "))
            lines.append("        </TABLE>")
        lines.append("    </TABLES>")
        groove = bytearray([0xFF] * (16 * 0x20))
        for gi in range(0x20):
            ticks = self.grooves.get(gi, [6, 6])
            for j, t in enumerate(ticks[:16]):
                groove[gi * 16 + j] = t
        lines.append("    <GROOVES>")
        lines.append(self._hex_rows(bytes(groove), "DATA", ind))
        lines.append("    </GROOVES>")
        lines.append("    <MIXER />")
        lines.append("</LITTLEGPTRACKER>")
        return "\n".join(lines) + "\n"

    def save(self, tracks_dir: Path) -> Path:
        folder = tracks_dir / f"lgpt_{self.name}"
        folder.mkdir(parents=True, exist_ok=True)
        samples = folder / "samples"
        samples.mkdir(exist_ok=True)
        for name, src in self.sample_files.items():
            shutil.copyfile(src, samples / name)
        (folder / "lgptsav.dat").write_text(self.to_xml(), encoding="ascii")
        return folder

    def length_steps(self) -> int:
        """Steps until the song loops, following track 0's chains row by row."""
        steps = 0
        for row in self.song:
            chain = row[0]
            if chain == 0xFF:
                break
            steps += 16 * len(self._chains[chain][0])
        return steps

    def length_seconds(self) -> float:
        return self.length_steps() * 15.0 / int(self.params["tempo"])

    def stats(self) -> str:
        used_rows = sum(1 for r in self.song if any(v != 0xFF for v in r))
        return (f"{self.name}: tempo {self.params['tempo']}, {used_rows} song rows, "
                f"{len(self._chains)} chains, {len(self._phrases)} phrases, "
                f"{len(self.instruments)} instruments, {len(self.tables)} tables, "
                f"{self.length_seconds():.1f}s")


# --- demo discovery / CLI ---------------------------------------------------

DEMO_DIR = Path(__file__).resolve().parent / "demos"


def load_demos() -> dict[str, object]:
    demos = {}
    for path in (str(Path(__file__).resolve().parent), str(DEMO_DIR)):
        if path not in sys.path:
            sys.path.insert(0, path)
    for path in sorted(DEMO_DIR.glob("*.py")):
        if path.name.startswith("_"):
            continue
        spec = importlib.util.spec_from_file_location(f"demo_{path.stem}", path)
        module = importlib.util.module_from_spec(spec)
        assert spec.loader
        spec.loader.exec_module(module)
        demos[path.stem] = module
    return demos


def main() -> int:
    parser = argparse.ArgumentParser(description="Build LGPT demo projects from tools/demos/*.py")
    parser.add_argument("--out", type=Path, default=ROOT / "projects" / "resources" / "demos",
                        help="tracks folder to write lgpt_<Name> projects into")
    parser.add_argument("--only", help="comma separated demo module names")
    parser.add_argument("--sim", action="store_true", help="also copy into rgnano-sim-data/tracks")
    args = parser.parse_args()

    demos = load_demos()
    wanted = set(args.only.split(",")) if args.only else set(demos)
    for name, module in demos.items():
        if name not in wanted:
            continue
        project = module.build()  # type: ignore[attr-defined]
        folder = project.save(args.out)
        print(project.stats(), "->", folder)
        if args.sim:
            sim_tracks = ROOT / "rgnano-sim-data" / "tracks"
            dest = sim_tracks / folder.name
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(folder, dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
