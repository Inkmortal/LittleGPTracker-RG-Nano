# Sound Design

How each sound engine works inside, what its knobs really do, and recipes you can type in. Each section has a small picture of the insides: read it top to bottom, like the sound flows.

Every recipe starts from a preset (**A + Left/Right** on `preset`), then changes a few knobs. Values are hex, as on the screen. **A + Start** plays the instrument while you turn knobs, so you hear each change.

## How a synth note is made

Whatever the engine, a synth note goes through the same chain. The engine makes the raw tone; everything after it is shared, and lives on the ENV, FILTER, LFO, MOD, MIX and EQ pages.

```
 note (pitch, tune)
   |
 ENGINE: the raw tone
   |
 + noise   (noise knob)
   |
 drive     (FILTER page)
   |
 filter    cutoff, reso
   |       <- env, LFO
 amp env   attack decay
   |       sustn releas
 EQ        (EQ page)
   |
 volume, pan, sends
```

So a dull engine can be brightened by the filter, a short one lengthened by the amp envelope, and any engine can wobble with the LFO or a MOD slot. The `engine` knob on the SOUND page picks the engine; the rest of the page changes to that engine's knobs.

| You want | Engine | Start from |
| --- | --- | --- |
| basses, leads, pads, classic analog | `synth` | `bass`, `lead`, `pad` |
| e-pianos, bells, brass, organs | `fm4` | `epiano`, `fm bell`, `brass` |
| huge chords, trance leads, hoovers | `hyper` | `hyper pad`, `trance lead` |
| chiptune, lo-fi, buzzy 8-bit | `wav` | `chip lead`, `fold bass` |
| drum machine kicks, snares, hats | `drum` | `808 kick`, `909 snare`, `808 hat` |
| marimba, bells, strings, hand drums | `phys` | `wood bar`, `guzheng` |
| vowels, 47 more models in two knobs | macro (`type`) | see [Macro Synth](Macro-Synth) |

## synth: waves through a filter

The classic analog recipe: start with a bright wave, then carve it with the filter. The `fm` knob lets a second sine bend the wave for bells and keys.

```
 wave + sub + noise
   |    ^
   |    fm: a sine at
   |    'ratio' bends it
   v
 filter <- env, LFO
   v
 amp env -> out
```

- `wave`: saw and pulse are bright (lots of harmonics to filter), sine and triangle are soft. `shape` changes the wave (pulse width, fold, detune).
- `cutoff` closes the filter (darker), `reso` makes it ring at the cutoff. `env` opens the filter at every note and `envdec` closes it again: the "pluck" of a bass or a synth brass.
- `sub` adds a square an octave down: a fatter bass for free.

**Rubber bass** — from `init`: `wave saw`, `sub 60`, `tune -24`, `cutoff 48`, `reso 50`, `env A0`, `envdec 88`, `decay 90`, `sustn 40`.

**Soft lead** — from `lead`: `shape 40`, `attack 18`, `glide 30`, `cutoff B0`, then LFO page `lfo pitch`, `rate B0`, `depth 14`.

**Wind** — from `init`: `wave noise`, `filter bandpass`, `cutoff 90`, `reso C0`, `attack 80`, `releas B0`, then LFO page `lfo cutoff`, `rate 40`, `depth 50`.

## fm4: operators bending each other

Four sine waves, **A B C D**. A **carrier** is heard. A **modulator** is not heard: it bends another operator's pitch very fast, which adds new harmonics. More modulator `level` = brighter and more metallic. `ratio` sets each operator's pitch against the note: whole numbers sound warm, others ring like bells.

```
 algo 00:
 A -> B -> C -> D -> out
 each bends the next

 algo 07 (e-piano):
 A -> B -> out
 C -> D -> out
```

Each operator has its own envelope (`atk` `dec` `sus`): a modulator that decays fast gives the "tine" ping of an e-piano, one that attacks slowly gives a brass swell. The `algo` picture on the page draws who bends whom.

**Bell** — from `fm init`: C `ratio 3.50`, C `level A0`. **Pluck** — then C `dec 70`: the brightness dies, the note stays.

**Organ** — from `fm init`: `algo 0B` (all four heard, no FM), A `ratio 0.50` `level C0`, B `ratio 1.00` `level FF`, C `ratio 2.00` `level A0`, D `ratio 3.00` `level 60`: four drawbars.

**Tremolo e-piano** — preset `epiano`, then LFO page `lfo volume`, `rate A0`, `depth 20`: the classic stage-piano wobble.

## hyper: a detuned saw choir

Six notes of a chord, each played by **two saws** tuned a little apart. Two saws at almost the same pitch beat against each other: that shimmer is what makes pads and trance leads sound huge. `width` pushes one saw of each pair left and the other right.

```
 note 1: saw~saw
 note 2: saw~saw
 ...      (x6)
 note 6: saw~saw
   ~ = swarm (detune)
 width: L saws / R saws
 + square sub
```

- `chord` fills the six notes (`minor`, `maj7`, ...); `shift` fades from notes 1-3 to 4-6.
- `swarm` is the detune: `20` gentle, `60` thick, `C0`+ seasick. `scale on` keeps every chord in the song's key.

**Wide minor pad** — from `hyper init`: `chord minor`, `swarm 60`, `width FF`, then ENV `attack C0`, `releas C0`, MIX `reverb 80`.

**Trance lead** — preset `trance lead`, then `shift 00` (only notes 1-3) and MIX `delay 60`.

**Hoover** — preset `hoover`, then `glide 40` and slide between notes with `PTCH`.

## wav: an 8-bit oscillator you bend

A wave drawn in a few steps, like a game console, with nothing smoothed. Every knob reshapes the drawing.

```
 wave (pulse, saw, sine)
   |
 size: steps per cycle
   |
 mult: repeat it (sync)
   |
 warp, mirror: squeeze,
   |    move the middle
 drive + limit
```

**Chip lead** — preset `chip lead`, then in the phrase `ARPG 0047` on a held note: the classic chiptune chord.

**PWM pad** — preset `pwm pad`: the LFO moves `mirror`, so the pulse width sweeps.

**Fold bass** — from `wav init`: `wave sine`, `tune -24`, `drive 60`, `limit fold`: the louder, the more it folds into buzz.

## drum: circuit models of drum machines

The classic drum machines made their sounds from small circuits, not samples. This engine simulates those circuits, so a knob changes the drum the way turning a pot inside the machine would.

```
 kick 808:
 pulse -> ringing filter
   tone, decay
 + click, FM (snap)
 -> overdrive (snap)

 snare 808:
 2 ringing drum modes
 + noise (snap)

 hat 808:
 6 square oscillators
 -> band-pass (tone)
 -> fade (decay)
 + white noise (snap)
```

- `kick 909` and `snare 909` are sine oscillators with pitch and FM sweeps: punchier and cleaner than the 808's ringing filters. `hat ring` multiplies oscillators in pairs for a harder, clangy metal.
- `tune` sets the drum's pitch; play different notes for tom fills or to tune a kick to your key.
- The drum decays by itself (`decay`), so leave the ENV page's amp envelope open.

**Deep 808** — preset `808 kick`: `decay C0`, `tune -29`, `snap 20`. Long and round, for trap and hip hop.

**Tight house kick** — preset `909 kick`: `decay 50`, `snap C0`, `tone 90`.

**Crack snare** — preset `909 snare`: `snap FF`, `tone C0`, `decay 40`, MIX `reverb 50`.

**Trap hat roll** — preset `808 hat`; in the phrase put `RTRG 0002` on a note (a roll) and `VOLM` to shape it.

## phys: objects you hit or pluck

Instead of a wave, this engine simulates an object. Hit a bar and it rings with a few frequencies at once (its **partials**); how those are spaced is what makes wood sound like wood and a bell like a bell.

```
 modal (struck):
 click (bright, strike)
   |
 24 resonators, one per
 partial of the object
   matter: their spacing
   decay: how long

 string (plucked):
 noise burst (the pick)
   |
 delay line, 1 period
   | <- filter: highs
   |    fade first
   +-> back round
```

- `matter` is the key knob. On `modal`: `00` squeezed partials (a hand drum), `40` perfectly in tune (a string or pipe), `C0` a wooden bar, `E0`+ a bell. On `string`: below `40` a buzzing bridge like a sitar's, above it a stiffer, metallic string.
- `bright` is the mallet or pick: felt (`00`) or hard (`FF`). `decay` is how long it rings.

**Marimba** — preset `wood bar` as it is; play chords on three tracks.

**Glockenspiel** — from `phys init`: `matter E0`, `bright C0`, `decay 90`, `tune +24`.

**Guzheng bend** — preset `guzheng`; in the phrase a note then `PTCH 1002` on the next row: the string bends up a whole tone as it rings.

**Gong** — from `phys init`: `matter 10`, `bright 60`, `decay E0`, `tune -24`, MIX `reverb A0`.

## Macro synth: 47 models, two knobs

Set `type` to `macro` for the other family: 47 complete synthesis models from the Braids module (vowels, formants, FM, wavetables, physical models, noise), each played with just `timbre` and `color`. The page tells you what the two knobs do on the shape under the cursor. See [Macro Synth](Macro-Synth) for the list.

**Singing vowel** — preset `vowels`, then in the phrase `TIMB 2000` on one note and `TIMB 20FF` a few rows later: the voice slides from "a" to "u".

## What each engine costs

The RG Nano has one 1.2 GHz core. Measured in the device's own build (`tools/dsp-harness/plaits_engines_check.cpp`), eight voices of one sound at once take:

| Sound (8 voices) | CPU |
| --- | --- |
| `wav` pwm pad | 8% |
| `drum` hats | 8–13% |
| `fm4` epiano | 10% |
| `phys` string | 10% |
| `drum` kicks, snares | 10–11% |
| `synth` pad | 13% |
| `hyper` pad | 15% |
| `phys` modal | 20% |

A full song with heavy sounds on every track, the send effects, EQ and limiter plays well within the device's limits, so mix engines freely. The `modal` model is the heaviest: one or two tracks of it is plenty.
