<p align="center">
  <img src="images/demo-song-playing.png" width="240" alt="Song screen playing Neon Drive">
  <img src="images/synth-1-sound.png" width="240" alt="Synth sound page">
  <img src="images/demo-mixer.png" width="240" alt="Mixer">
</p>

<h1 align="center">LGPT for RG Nano</h1>

<p align="center"><b>A pocket tracker workstation for the Anbernic RG Nano.</b><br>
Eight tracks, built-in synths, sample packs, chorus, echo and reverb, live mode, WAV export — on a 1.54" screen.</p>

---

This is a fork of [LittleGPTracker / Little Piggy Tracker](https://github.com/djdiskmachine/LittleGPTracker) rebuilt for the RG Nano's 240×240 screen and tiny buttons, with the spirit of the Dirtywave M8: everything you need to write a full song lives in your pocket, and the screen stays quiet so the music does the talking.

**Never used a tracker?** Go straight to **[Your First Song](Your-First-Song)**. In about half an hour you build **Dusk**, a full song with jazzy chords, a bouncing bass and an echoing hook, from the synths every new song starts with.

## Start here

| | Page | What you get |
| --- | --- | --- |
| 1 | **[Install](Install)** | Put the app and demo songs on your RG Nano |
| 2 | **[Controls](Controls)** | The few key rules every screen follows, and every combo |
| 3 | **[How Trackers Work](How-Trackers-Work)** | The four ideas behind every tracker |
| 4 | **[Your First Song](Your-First-Song)** | Build a whole song, step by step |
| 5 | **[Demo Songs](Demo-Songs)** | Finished songs to open, play and pull apart |

## Reference

| Page | |
| --- | --- |
| **[Screens](Screens)** | Song (and Live mode), Chain, Phrase, Instrument, Table, Groove, Project, Mixer, FX, EQ, Limit, Helper |
| **[Synth](Synth)** | The built-in synth: every knob, every preset, sound recipes |
| **[Commands](Commands)** | All phrase/table commands with examples |
| **[Music Theory Cheat Sheet](Music-Theory-Cheat-Sheet)** | Scales, chords, progressions and drum patterns that just work |
| **[Samples](Samples)** | The built-in sample packs, your own WAVs, render to sample |
| **[Export](Export)** | Bounce your song to WAV (stereo or stems) |
| **[FAQ](FAQ)** | Common "why did that happen?" answers |
| **[Developer Guide](Developer-Guide)** | Simulator, tests, song composer, build and install scripts |

## What makes this fork different

- **Makes sound instantly.** New projects come with a 16-instrument synth kit: kick, snare, hats, clap, bass, lead, pad, pluck, keys, bell, acid, sub, chip, tom, perc.
- **M8-style synth engine.** Seven waveforms, sub oscillator, noise, 2-operator FM, one-note chords, envelopes, resonant filter, drive, LFO, glide — plus shared reverb and tempo-synced echo.
- **Designed for 240×240.** Each instrument page draws what its knobs do. Explanations only appear when you ask for them with **RB + Select**.
- **Real instruments.** Sample packs of piano, strings, brass, drums, guitar, Chinese instruments and more, ready to import.
- **Play it live.** Live mode turns the Song screen into a clip launcher; the Mixer and FX screens mix it.
- **Forgiving.** One key grammar on every screen, and **B + Select** undoes any change.
- **Happy accidents.** Random notes, fills, shuffles and the `RAND`/`CHNC` commands, plus a master EQ and limiter to finish the mix, and an EQ on every instrument.
- **Tested like a product.** A desktop simulator drives the real app with scripted button presses, checks the screens and measures the audio before anything goes on the device.

> **Tip:** On any screen, **RB + Select** opens the helper: a map of where you are, the buttons for this screen, and a short how-to. **A** in the helper opens this whole guide inside the app. Press **RB + Select** again to close.
