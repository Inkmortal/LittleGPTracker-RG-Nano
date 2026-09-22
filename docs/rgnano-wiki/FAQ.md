# FAQ

**I pressed A on a new track and it plays my kick.**
One **A** on an empty cell reuses the last chain or phrase (the screen says `Reused 00`). Press **A** again for a new, empty one (`New chain 01`). Reusing is on purpose: it's how you repeat parts without copying.

**RB + Right does nothing.**
The cell is empty — the screen tells you to press **A** first. On Phrase, RB + Right needs a note with an instrument.

**My note keeps ringing.**
Notes sound until the next note on the same track. Put `KILL 0000` where it should stop, or lower the instrument's `sustn` (ENV page) so it fades by itself.

**Two parts on one track cut each other off.**
Each track plays one note at a time. Put the second part on another track, or use `chord`/`CHRD` for chords.

**The tracks drift out of sync after a while.**
Chains on the same Song row must be the same length (same number of phrases). A shorter chain moves on to the next row earlier.

**A track keeps looping one section.**
An empty `--` Song cell makes that track jump back to the top of its block. Give silent sections a chain of empty phrases instead.

**Notes jump by more than one semitone when I edit them.**
A Project `Key`/`Scale` is set, so editing follows the scale. Use **LB + D-pad** for chromatic steps, or set `Key: --`.

**Where is the reverb setting?**
Two parts: each instrument's **MIX** page sets how much it sends (`reverb`, `delay`); the **Project** screen sets the room (`Reverb`, `Damp`) and echo (`Echo`, `Fdbk`).

**It's too loud / distorted.**
Lower instrument volumes (kick and bass eat the most headroom), or `Drive` on the Project screen. Watch the Mixer meters.

**How do I get back to the project list?**
Project screen → `Song List`, or press the Menu/Power button for the app menu.

**I changed a demo song by accident.**
Songs only change on disk when you save. Reload it, or copy the demo folder from the build zip again.

**Something went wrong — can I send a log?**
The app writes `Applications/lgpt-rgnano.log` on the SD card, including the button presses, so a session can be replayed in the simulator.
