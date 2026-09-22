# Synth

Every instrument slot `00`–`7F` can be a **synth** or a **sample**. New projects fill `00`–`0F` with a ready synth kit. Open an instrument with **RB + Right** from a phrase; the cursor lands on `preset`, so **A + Left/Right** immediately flips through sounds and **Start** plays one.

Each track plays one synth voice at a time. A note stopped by `KILL` or replaced by a different instrument fades out with its release instead of cutting off.

## The starter kit

| Slot | Preset | Slot | Preset |
| --- | --- | --- | --- |
| `00` | **kick** — sine with a fast pitch drop and drive | `08` | **pluck** — bright saw that closes fast |
| `01` | **snare** — triangle body + noise | `09` | **keys** — FM electric piano with tremolo |
| `02` | **hat** — six metallic squares + noise, highpassed | `0A` | **bell** — FM bell, long ring |
| `03` | **openhat** — longer hat | `0B` | **acid** — resonant saw with glide |
| `04` | **clap** — bandpassed noise | `0C` | **subbass** — clean sine sub |
| `05` | **bass** — saw + sub, filter bite | `0D` | **chip** — 8-bit pulse |
| `06` | **lead** — pulse with vibrato and glide | `0E` | **tom** — pitched drum |
| `07` | **pad** — supersaw, slow attack, drifting filter | `0F` | **perc** — woodblock/rim |

`init` is a plain starting point for designing from scratch. Basses are tuned two octaves down, drums are tuned so `C 3` sounds right.

## Pages and knobs

Switch pages with **LB + Left/Right**. Values are hex `00`–`FF` unless noted. The top of the page draws the result; press **RB + Select** for a plain-English explanation of the focused knob.

### SOUND — the raw tone

<img src="images/synth-1-sound.png" width="280" align="right">

| Knob | Does |
| --- | --- |
| `type` | synth / sample |
| `preset` | load a ready sound (overwrites the knobs) |
| `wave` | sine, triangle, saw, pulse, supersaw, noise, metal |
| `shape` | depends on the wave: sine → feedback (buzzier), triangle → wavefold, saw → octave brightness, pulse → width, supersaw → detune, noise → crunch, metal → spread |
| `sub` | square one octave below — fatter bass |
| `noise` | mixes white noise in — snares, breath |
| `fm` | FM depth: bells, keys, metallic tones |
| `ratio` | FM partner pitch: `1`, `2` warm; `3.5`, `7`, `11` bell-like |
| `chord` | play a chord from every note: 5th, octave, major, minor, sus2, sus4, maj7, min7, dom7 |
| `tune` | semitones, `-12` = an octave down |

<br clear="right">

### ENV — volume and pitch over time

<img src="images/synth-2-env.png" width="280" align="right">

| Knob | Does |
| --- | --- |
| `attack` | fade-in time |
| `decay` | time to fall to the sustain level |
| `sustn` | level while the note holds; `00` = pluck or drum |
| `releas` | fade after the note stops |
| `p.env` | pitch drop at note start, up to +48 semitones — kicks, toms, zaps |
| `p.dec` | how fast that pitch drop happens |
| `glide` | slide between notes (mono-synth legato); `00` = off |

Times are exponential: `00` = 1 ms, `40` = 10 ms, `80` = 100 ms, `C0` = 1 s, `FF` = 10 s.

<br clear="right">

### FILTER — bright or dark

<img src="images/synth-3-filter.png" width="280" align="right">

| Knob | Does |
| --- | --- |
| `filter` | lowpass, highpass, bandpass, off |
| `cutoff` | 20 Hz – 20 kHz; lower = darker (lowpass) |
| `reso` | peak at the cutoff; high = squelch |
| `env` | brightness burst at each note (also drives FM brightness) |
| `envdec` | how fast the burst closes |
| `drive` | saturation before the filter — warmth, grit |

<br clear="right">

### MOD — movement

| Knob | Does |
| --- | --- |
| `lfo` | what wobbles: pitch (vibrato), cutoff (wah), volume (tremolo), shape (PWM) |
| `rate` | `00` = 0.05 Hz … `80` = 1.6 Hz … `C0` = 9 Hz … `FF` = 50 Hz |
| `depth` | how much; `00` = off |
| `fine` | fine tune in cents |
| `auto` / `table` | an instrument table that runs on every note |

### MIX — level and space

<img src="images/synth-5-mix.png" width="280" align="right">

| Knob | Does |
| --- | --- |
| `volume` | loudness (`VOLM` changes it per note) |
| `pan` | `00` left, `7F` centre, `FE` right |
| `reverb` | send to the shared reverb |
| `delay` | send to the shared echo |

The reverb room and echo time are shared by every instrument and set on the **Project** screen (`Reverb`, `Damp`, `Echo`, `Fdbk`).

<br clear="right">

## Chords with CHRD

**How it works — two parts:**

- **The note you type picks WHICH chord.** Type `A 2` and you get an A chord.
- **The CHRD value picks WHAT KIND.** `0047` = major (happy), `0037` = minor (sad).

So `A 2` + `CHRD 0037` = **A minor**, and `F 2` + `CHRD 0047` = **F major**. The CHRD value on its own is never "A" or "F" — it's only major or minor.

**What the digits actually do:** each digit says "also play the note this many steps up from mine". `0037` on `A 2`:

```text
A                               <- your note
A  A# B  C                      <- 3 steps up:  C
A  A# B  C  C# D  D# E          <- 7 steps up:  E
                                   plays A + C + E = A minor
```

Count every key, black ones included (A → A# → B → C is 3 steps). Past 9 the digits are hex letters: `A` = 10, `B` = 11.

| Type this note | + CHRD `0037` plays | + CHRD `0047` plays |
| --- | --- | --- |
| `A 2` | A C E = **A minor** | A C# E = A major |
| `C 3` | C D# G = C minor | C E G = **C major** |
| `D 2` | D F A = **D minor** | D F# A = D major |
| `E 2` | E G B = **E minor** | E G# B = E major |
| `F 2` | F G# C = F minor | F A C = **F major** |
| `G 2` | G A# D = G minor | G B D = **G major** |

The **bold** ones are the chords that belong to A minor / C major — use those and it always sounds right.

More kinds, same idea (the note still picks which chord):

| CHRD | Kind | Sounds |
| --- | --- | --- |
| `0047` | major | happy, bright |
| `0037` | minor | sad, serious |
| `047B` | major 7 | dreamy, lo-fi |
| `037A` | minor 7 | smooth, jazzy |
| `047A` | dominant 7 | bluesy, wants to move on |
| `0057` | sus4 | open, unresolved |
| `0027` | sus2 | airy |
| `0007` | power chord | rock, neutral |

`CHRD` overrides the `chord` knob for that note. The `chord` knob does the same thing for every note of the instrument (pick `minor` and every note you type becomes a minor chord).

**Smoother changes (inversions):** a chord doesn't have to start on its own name. `C 3` + `0059` plays C F A — still an F major chord, just stacked differently — so moving from C major to F major barely moves your hand. Advanced; skip it until the basics feel easy.

## Recipes

Start from `init` (or the preset named) and set:

| Sound | Settings |
| --- | --- |
| **Deep 808 kick** | kick; `decay C0`, `p.env 80`, `drive 30` |
| **Punchy techno kick** | kick; `decay 98`, `p.dec 60`, `drive 90` |
| **Trap hat roll** | hat; in the phrase `RTRG 0002` on a note |
| **Reese bass** | wave supersaw, `shape 40`, `tune -24`, `sub 60`, lowpass `cutoff 60`, `drive 40` |
| **Acid line** | acid; write 16ths, raise `reso`, automate with `FCUT` |
| **Plucky arp** | pluck; `delay 60` on MIX, Project `Echo 3` |
| **Warm pad** | pad; `chord minor`, `attack C0`, `releas C8`, `reverb A0` |
| **Glassy bell** | bell; `ratio 7`, `env A0`, `reverb 90` |
| **Flute** | wave sine, `noise 28`, `attack 38`, `lfo pitch`, `rate B0`, `depth 18` |
| **Bowed string (erhu)** | wave saw, lowpass `cutoff 90`, `attack 58`, `glide 50`, vibrato `depth 20` |
| **Chip arpeggio** | chip; in the phrase `ARPG 0047` on a held note |
| **Gong** | wave metal, `tune -24`, lowpass `cutoff 90`, `reso 40`, `decay E0`, `reverb 90` |

## Commands synths understand

`VOLM` `PAN_` `FCUT` `FRES` `FLTR` `PTCH` `LEGA` `PFIN` `ARPG` `RTRG` `CRSH` (drive) `KILL` `TABL` `DLAY` `CHRD` — see [Commands](Commands). `RTRG` restrikes the envelopes, great for rolls and tremolo.
