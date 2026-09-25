# Notices

This repository is an RG Nano-focused fork of Little Piggy Tracker / LittleGPTracker.

Upstream lineage:

- djdiskmachine/LittleGPTracker
- Mdashdotdashn/LittleGPTracker by Marc Nostromo

The original project lineage includes BSD 3-Clause license terms, and this fork carries GPLv3 terms as documented in [LICENSE](LICENSE).

This fork is not endorsed by Dirtywave, Little Sound Dj, Anbernic, FunKey, or the upstream LittleGPTracker maintainers unless explicitly stated by those parties.

Product names are used only to describe compatibility and development targets.

Design references (no code copied):

- The master limiter (`sources/Application/Mixer/MasterLimiter.cpp`) follows the structure described in Geraint Luff's "Designing a straightforward limiter" (Signalsmith Audio, 2022; their DSP library is MIT-licensed): peak hold over the look-ahead window, instant-attack/exponential release, stacked moving averages, audio delayed by the window.
- The three-band EQ filters (`sources/Application/Mixer/ThreeBandEQ.cpp`) use Robert Bristow-Johnson's public "Audio EQ Cookbook" formulas.
- The MOD slot envelopes (`sources/Application/Instruments/ModSources.cpp`): the segment design, where each segment runs from the level it starts at to its target through a shaped phase curve, is adapted from the envelope of Mutable Instruments Braids (`braids/envelope.h`, Copyright 2012 Emilie Gillet, MIT License). No code was copied verbatim.

## Third-party code

- `sources/Externals/Braids`: the macro oscillator (analog and digital oscillators, macro oscillator, lookup tables) from Braids by Emilie Gillet, https://github.com/pichenettes/eurorack (braids/), MIT License, copyright notices kept in each file. Used by the Macro Synth instrument; changes: include paths, the settings header trimmed to the shape list, a per-thread random generator.
- `sources/Externals/Braids/stmlib`: `stmlib.h`, `utils/dsp.h`, `utils/random.h/.cc` from stmlib by Emilie Gillet, https://github.com/pichenettes/stmlib, MIT License.
