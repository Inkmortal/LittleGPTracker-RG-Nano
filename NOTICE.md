# Notices

This repository is an RG Nano-focused fork of Little Piggy Tracker / LittleGPTracker.

Upstream lineage:

- djdiskmachine/LittleGPTracker
- Mdashdotdashn/LittleGPTracker by Marc Nostromo

The original project lineage includes BSD 3-Clause license terms, and this fork carries GPLv3 terms as documented in [LICENSE](LICENSE).

This fork is not endorsed by Dirtywave, Little Sound Dj, Anbernic, FunKey, or the upstream LittleGPTracker maintainers unless explicitly stated by those parties.

Product names are used only to describe compatibility and development targets.

## Third-party code and designs

- `sources/Application/Instruments/ModSources.cpp` (MOD slot envelopes): the segment design, where each segment runs from the level it starts at to its target through a shaped phase curve, is adapted from the envelope of Mutable Instruments Braids (`braids/envelope.h`, Copyright 2012 Emilie Gillet, MIT License). No code was copied verbatim.
