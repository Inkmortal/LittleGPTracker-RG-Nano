# Export

Bounce your song to WAV on the device.

<img src="images/project.png" width="300" align="right" alt="Project screen">

1. Song screen → **RB + Up** to open **Project**.
2. Move to `Render:` and set it with **A + Left/Right**:
   - `Stereo` → one file, `mixdown.wav`
   - `Stems` → one file per track, `channel0.wav` … `channel7.wav`
3. Press **Start**. The song plays and records in real time.
4. Press **Start** again to stop and close the file.
5. Set `Render:` back to `Off` for normal playing.

The files land in the song's folder: `Applications/Tracks/lgpt_<name>/`.

<br clear="right">

## Tips

- Stems are recorded **before** the shared reverb and echo; the stereo mix includes them.
- To get a clean loop, stop exactly at the end of the song, or trim the WAV on a computer.
- If the export sounds squashed or distorted, lower `Drive` on the Project screen or the instrument volumes, and watch the [Mixer](Screens#mixer). `Clip: Subtle` softly catches the occasional peak.
