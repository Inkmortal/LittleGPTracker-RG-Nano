# Synth engines: what fits the RG Nano, and what's next

Measured 2026-09-26 with `tools/dsp-harness/plaits_engines_check.cpp` (device build under qemu, a7cost plugin: weighted Cortex-A7 instructions, the model `engine_room_check.cpp` is calibrated with). Figures are 8 voices of one preset as a share of the 1.2 GHz core; drums are re-hit 8 times a second, struck models 4 times a second.

## What the app has

| Engine | Where | 8 voices |
| --- | --- | --- |
| `synth` (subtractive + 2-op FM, TPT state-variable filter) | `SynthInstrument.cpp` | pad 12.6%, kick 11.2% |
| `fm4` (M8-style 4-op FM, MSFA-style operators) | `SynthEngines.cpp` | epiano 9.7% |
| `hyper` (6 notes x 2 detuned saws + sub) | `SynthEngines.cpp` | hyper pad 14.7% |
| `wav` (raw 8-bit bendable oscillator) | `SynthEngines.cpp` | pwm pad 8.1% |
| `drum` (Plaits 808/909 kick, snare, 2 hats) | `SynthPlaits.cpp` | 7.7-13.0% |
| `phys` string (Plaits Karplus-Strong + dispersion) | `SynthPlaits.cpp` | 9.5-10.5% |
| `phys` modal (Plaits 24-mode resonator) | `SynthPlaits.cpp` | 20.1% |
| macro (Braids, 47 shapes at 96 kHz, resampled) | `MacroInstrument.cpp` | 0.7-1.0x the synth pad (`macro_check.cpp`) |

For scale: the Engine Room demo (heavy engines on all 8 tracks, sends, master EQ and limiter) costs ~19.5% by the same model and runs ~20% average / 27% peak on the device without dropouts.

## Ranking of candidates (musical value per cycle, and the gap it fills)

1. **Plaits drum circuits** — added as `drum`. The biggest gap: drums were subtractive synth presets and Braids' simple KICK/SNARE/CYMBAL. Circuit models behave like the machines (tone, decay, snap all change the drum itself). Cheap: ~10% for 8.
2. **Plaits string** — added as `phys` / `string`. Plucked instruments (guzheng, nylon, sitar via the curved-bridge nonlinearity, steel) for ~10%. Braids' PLUCK is a cruder physical model.
3. **Plaits modal resonator** — added as `phys` / `modal`. Mallets, bells, hand drums, gongs from one `matter` knob. The heaviest engine (20%), fine for a track or two.
4. **Virtual analog with a 4-pole ZDF ladder filter** — next candidate. The synth's 2-pole SVF can't do the classic ladder bass squelch; a ladder costs ~2x the SVF per voice. Would slot in as a `filter` type rather than an engine.
5. **Plaits 6-op FM (DX7 patches)** — valuable for DX7 bank compatibility, but `fm4` covers most of that ground; needs patch storage and ~1.5x fm4's cost.
6. **Plaits chords / string machine, wave terrain, speech** — fun, niche; speech overlaps Braids' vowels.
7. **Wavetable morphing, granular noise** — already covered by Braids' WTBL/WMAP/WLINE/WTx4 and CLOUD/PARTICLE. True granular on samples belongs to the sampler (memory-heavy).

## Porting notes (for the next engine)

- `tools/import_plaits.py` copies Plaits sources: stmlib renamed `pstmlib` (Braids brings an older stmlib), 44.1 kHz `kSampleRate`, ARM asm only on ARM, per-thread random generator.
- Keep Plaits headers out of `SynthInstrument.h` (the macro synth's Braids code includes it).
- The device libm quirks still apply (`floor` replaced in `RGNANOLibm.cpp`, `sin` of large arguments wrong): Plaits uses lookup tables and wrapped phases, so it's safe.
- Self-enveloped models: the presets leave the amp envelope open and the voice ends after 100 ms below -80 dB.
