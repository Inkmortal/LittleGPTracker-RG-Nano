# Music Theory Cheat Sheet

Just enough theory to make things sound right. Everything here is in **A minor / C major** (same notes, no sharps or flats) so it's easy to type.

## Set the key and stop worrying

Project screen → `Key: A`, `Scale: Aeolian mode (minor)`. Now **A + Left/Right** on a note only steps through notes that fit. **LB + D-pad** steps outside the scale when you want a "wrong" note on purpose.

To see the scale on a keyboard, or make your own, press **RB + Right** on Project ([Scale screen](Screens#scale)): **A** on a key puts that note in or out. Five notes you like, say A C D E G, and every edit and every `RAND` stays on them.

## Scales

| Scale | Notes from A | Feels | Genres |
| --- | --- | --- | --- |
| Minor (Aeolian) | A B C D E F G | emotional, serious | synthwave, techno, trap, most electronic |
| Minor pentatonic | A C D E G | impossible to play a wrong note | wuxia / Chinese, blues, rock solos |
| Major (C Ionian) | C D E F G A B | happy, bright | pop, chiptune |
| Dorian (from D) | D E F G A B C | cool, jazzy minor | lo-fi, funk, house |
| Phrygian (from E) | E F G A B C D | dark, exotic | metal, flamenco, dark techno |

## Chords that fit A minor

Type the **root note** in the note column, then add the **CHRD** value for the kind of chord (see [Synth](Synth#chords-with-chrd) for why).

| Chord | Root note | CHRD | Role |
| --- | --- | --- | --- |
| Am | A | `0037` | home |
| C | C | `0047` | bright relative |
| Dm | D | `0037` | soft, moving away |
| Em | E | `0037` | tension (E major `0047` = even more) |
| F | F | `0047` | lift, hopeful |
| G | G | `0047` | pushes back home |
| Am7 / Dm7 / Em7 | | `037A` | smoother, jazzier |
| Cmaj7 / Fmaj7 | | `047B` | dreamy, lo-fi |

## Progressions that always work

One chord per bar, four bars, loop.

| Progression | Sound | Used in |
| --- | --- | --- |
| `Am F C G` | epic, singalong | synthwave, pop, trance |
| `F G Em Am` | lift into a chorus | anime, pop, J-rock |
| `Am G F G` | driving, heroic | rock, soundtrack |
| `Am Em F C` | melancholic | ballads |
| `Dm7 G7 Cmaj7 Am7` | smooth, jazzy | lo-fi, neo-soul |
| `Am Dm Am E` | classical, dramatic | wuxia, tango, soundtracks |

**Bass:** play the root of each chord. On the last beat of a bar, a passing note that walks toward the next root makes it move.

## Melody in five rules

1. **Land on chord notes** on beats 1 and 3 (for Am: A, C or E).
2. **Move in steps** most of the time; after a big jump, step back the other way.
3. **Leave space.** Rests are what make a melody memorable.
4. **Repeat, then change.** Play a 2-bar idea twice, changing only the ending the second time.
5. **Call and answer.** Bar 1 asks (ends up high or unresolved), bar 2 answers (ends on the root).

## Drum patterns

`x` = hit, `.` = rest, 16 steps = one bar. Rows `00 04 08 0C` are the beats.

```text
FOUR ON THE FLOOR (house, disco, synthwave)
kick   x...x...x...x...
clap   ....x.......x...
hat    ..x...x...x...x.     (open hats on the off-beat)

BOOM BAP (hip-hop, lo-fi) — use groove 7 5 for swing
kick   x.....x...x.....
snare  ....x.......x...
hat    x.x.x.x.x.x.x.x.

HALF TIME (epic, trap, dubstep)
kick   x.......x.x.....
snare  ........x.......
hat    x.x.x.x.x.x.xxxx     (RTRG 0002 on the last hats for a roll)

BREAKBEAT (drum and bass at 170+ BPM)
kick   x.........x.....
snare  ....x.......x..x
hat    x.x.x.x.x.x.x.x.

TAIKO / WUXIA
drum   x.......x...x...
       x.....x.x...x.o.     (o = VOLM 0070 ghost)
```

**Make it human:** vary hat volumes with `VOLM` (loud on the beat, quieter between), and push a note late with `DLAY 0001`.

## Genre starting points

| Genre | BPM | Groove | Key sounds (starter kit) | Trick |
| --- | --- | --- | --- | --- |
| Synthwave | 85–110 | `6 6` | kick, snare+reverb, bass octaves, pad, pluck arp, lead | octave-jumping 8th bass, FCUT sweep on the arp |
| Lo-fi hip-hop | 70–90 | `7 5` | soft kick, snare, keys with `037A`, sub, bell | chords re-hit off the beat at lower `VOLM` |
| House | 120–128 | `6 6` | kick 4/4, clap, openhat off-beat, keys stabs, bass | open hat on every `2 6 A E` step |
| Techno | 125–135 | `6 6` | kick with drive, hat, acid | automate `FCUT`/`FRES` on the acid line |
| Chiptune | 120–160 | `6 6` | chip lead, `ARPG` chords, chip bass, noise drums | fast `ARPG 0047` on held notes |
| Wuxia | 80–100 | `6 6` | tom as taiko, keys as guzheng, sine+noise flute, bell/metal gong | pentatonic only, `RTRG 0002` tremolo, glide on the lead |

See them all put together in the **[Demo Songs](Demo-Songs)**.
