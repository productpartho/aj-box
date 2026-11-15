# S2 Design: AJ Box on FM1 Open's stock instrument

Nov 14, 2026 · for Partho to review before any firmware is written

Decided so far: AJ Box is a new front for FM1 Open's stock instrument (PRD "How it fits together"), and the 8 recordings are one-shot sounds (PRD "The 8 sampled sounds"). Background and measurements: `docs/s0-findings.md`, "S2 design finding".

## How it works

FM1 Open's sealed library keeps doing everything it does today: start-up, USB, the update handler, the 8-track sequencer and all the sound engines. AJ Box replaces only the open app on top of it (`firmware/app/`): the screens, what the keys do, and what is in the tracks.

| Track | Engine | Used for |
| --- | --- | --- |
| 1 | DRUM 808 | Synthesized drum loops, the Fill and the Finish crash |
| 2 | Sampler (8 pads) | Drum loops made of the recorded one-shots |
| 3 | DX7 (FM) | Melody loops on the 11 FM instruments |
| 4 | PSG | Melody loops on the 4 chip instruments |
| 5 | Pluck | Melody loops on Harp and Toy Guitar |
| 6 | 303 | Melody loops on the 3 bass and lead instruments |
| 7, 8 | off | Spare |

All six engines stay loaded, so changing loop or instrument never has to load an engine while she plays. Only one drum track and one melody track hold notes at a time; the others are empty.

Every track runs 64 steps (4 bars). A 1-bar or 2-bar loop is written out two or four times. A swap writes the new loop's 64 steps into the track while the sequencer keeps running (`groove_sequence_replace`), so the new loop continues from the step the beat is on. That is the PRD's phase-locked swap, done by the stock clock.

## PRD controls and what they turn into

| PRD control | On the stock instrument |
| --- | --- |
| Loop key | Write that loop's steps into its track; clear the other track on that side |
| Same key again (stop on the next beat) | Clear the track's steps at the next beat |
| PLAY/STOP | Stop and start the stock transport |
| Back, Next, Dice | AJ Box only: which page the keys show, then a loop key as above |
| Fill (held) | Write the fill pattern into the 808 track; put the loop back on release |
| Fast (held) | Rewrite the melody at double speed; put it back on release |
| Mute drums, Mute melody (held) | Track mute |
| Finish | On the next bar write a crash and the last chord, then stop the transport |
| PRESETS (12 kits) | Set the 808 track's part settings (`groove_tone_edit`, `groove_timbre_apply`) |
| ALGORITHM (20 instruments) | Move the melody to that engine's track and load its sound (`groove_fm_import` for FM voices, tone settings for the others) |
| OCT− and OCT+ | Rewrite the melody an octave up or down |
| Mood (7 scales) | Rewrite the melody in the new scale, just before the next bar |
| SELECT (tempo) | `groove_tempo_edit` |
| KNOB 4 (balance) | Drum and melody track volumes |
| Echo, Sparkle | The stock delay and reverb: sends on every track |
| Wobble, Wah, Crunch, Stutter, KNOB 1 Swoosh | AJ Box's own effects on the finished stereo mix, in the app's output stage (`app/audio_output.c`) |
| Limiter always on | The stock master limiter inside the instrument, plus a safety limit after AJ Box's own effects |
| SAVE, SEQ (hearts), REC, ARP (jam), power-off memory | AJ Box only, saved under the build plan's "Saving and update-mode rules" |
| Hello chime | Four notes on the FM track at start-up |

## What changes from the desktop version you heard

- **Same:** the 96 loop patterns, the 12 kits, the 20 synthesized instruments, the 7 moods, tempo range and all the keys.
- **Different sound source for 12 loops:** the 4 sampled drum loops become patterns of one-shots, and the 8 melody loops that used Music Box, Kalimba, La and Cat move to synthesized instruments. I'll propose the new instruments and render them for you to hear.
- **Levels:** AJ Box's per-loop gains become track volumes. They need re-balancing against the stock mixer.
- **Effects on the whole mix only.** That matches PRD "Effects" ("for the whole mix").
- **Jam replay** is driven from the control side, so replayed presses land on steps as live presses do.

## Flash budget

| Part | Bytes (estimate unless marked) |
| --- | --- |
| Room with the stock app removed | about 84,600 (measured by trial link) |
| Loop, kit and instrument data | 13,300 as compiled today (measured); can be packed smaller |
| The 11 FM voices | 1,700 |
| Loop logic: pages, swaps, moods, arpeggios, hearts, jam, saving | 10,000 |
| Own effects (Wobble, Wah, Crunch, Stutter, Swoosh) | 3,400 to 7,800 (measured, depends on optimisation) |
| Board, audio and MIDI glue kept from FM1 Open | 6,000 |
| Screens, icons and drawing | what is left: about 40,000. So far 18,800 (measured with the FM-1's compiler, Nov 15, 2026), parent mode included |

The first device build reports the real size, and every build after it.

## First results on the desktop (Nov 14, 2026)

- FM1 Open's whole simulator (`firmware/web`, `firmware/app`, `firmware/third_party`) builds natively with the desktop compiler and no changes, and runs. `dsp/stock/probe.c` writes a drum pattern and a tune into two tracks with `groove_sequence_replace`, starts the transport and renders `renders/s2/probe-stock-instrument.wav`.
- A pattern swapped while playing takes effect and the position carries on (step 31 after 2 bars at 120 BPM, as expected).
- `dsp/stock/ajstock.c` turns AJ Box's loops, kits and instruments into stock cells and settings and renders pairs. Page 1 is rendered both ways for the drum loudness question in `renders/s2/page1-one-track` and `renders/s2/page1-three-tracks`. Since then: Partho chose one drum track; all 7 moods and all 6 pages render (`ajstock all`, `ajstock tour`); `ajstock levels` finds a track volume for every loop (`dsp/stock/volumes.inc`: drums alone near -12 dBFS, melodies near -15, about 6 dB under the old engine because the stock instrument is quieter). Three melody loops stay 2 to 4 dB short at full volume (Rocket, Boss Fight, Bird Call). The 8 one-shots are synthesized in `dsp/stock/oneshots.c` (dhol low and high, pot, spoon, mouth kick, "shh", cuica high and low), written into a stock sample bank and loaded on the sampler track; Dhol Party, Pots and Pans, Beatbox and Rain Stick play as pad patterns, and Goron Drums' cuica is on the two cuica pads. `dsp/stock/ajapp.c` now holds AJ Box's app logic on the stock calls: keys, swaps, stop on the beat, pages, Dice, PLAY/STOP, Fill, Fast, Mute, Finish, kits, instruments, octave, Mood, tempo, balance, Echo and Sparkle on the stock sends, and Wobble, Wah, Crunch, Stutter and Swoosh on the finished mix. `dsp/stock/test_app.c` runs 263 checks on it in the simulator, and `ajstock demos` renders ten control demos. Found on the way: an 808 part's level resets when its Sound changes, and kits have to start from the defaults. Hearts, the jam, power-off memory, parent settings and the hello chime are in too. `dsp/stock/ajstore.c` does the saving the build plan asks for: SDK flash calls only, a checksummed record in two sectors written in turn (sectors 4 to 11 of the user partition, below the sample bank), the header written last, then a read-back compare; a heart, jam or setting that fails any of that is not kept. Saved data is range-checked before use. 301 checks pass, including switching off and on in the simulator and damaged records. A real flash failure cannot be produced in the simulator, so the not-saved path is covered only by damaged data.
- **New limit found:** a drum step has one velocity for every drum in it. AJ Box's patterns give each part its own loudness on the same step (a hard kick with a soft hat). Either the loudest hit sets the step, or soft, normal and hard hits go on separate 808 tracks. To be decided by ear.

## Screens on the desktop (Nov 15, 2026)

- The designs are vector drawings, so the firmware keeps them as outlines, not bitmaps. `tools/icon_pack.pl` reads the Icons sheet and `assets/icons/extra.html` and writes `assets/icons/art.inc`: 81 pictures in 6,368 bytes.
- `dsp/stock/ajdraw.c` draws boxes, circles, arcs and those pictures with smooth edges, in whole numbers only. `dsp/stock/ajscreen.c` lays out the start-up, play, effects, recording, heart, Ta-da, Mood and sleepy battery screens from the sizes in `docs/design`, driven by `ajapp_status`. It repaints only the 16-row strips that changed, a few per call.
- `ajstock screens <dir>` saves every screen as a 240 × 240 PNG plus one sheet (`renders/s2/screens`). 93 screen checks were added; 394 checks pass in all.
- Size with the FM-1's compiler (objects, before linking): 9,700 bytes of code, 6,900 of tables, about 10.5 KiB of memory.
- Where the firmware differs from the designs:
  - The letters ("AJ Box", "Ta-da!", the heart's number) are thick round strokes, not the Fredoka font, to save space.
  - Speed is one, two or three arrows (under 90, under 140, faster), as the design drew it; the PRD says turtle or rabbit.
  - The picture in each half is the page theme of the playing loop, as the Icons sheet gives 12 themes, not 96 loop pictures.
  - Four moods had no drawing: the wand is the Magic kit picture; balloon, lantern and map are new. The Mood screen shows the chosen mood and its two neighbours.
  - The designs show lit bubbles only for Echo and Sparkle. Wobble is lilac, Wah purple, Crunch magenta, Stutter cream.
  - Not in the designs: a kit or instrument change shows its picture in the middle for about a second; the filter and balance knobs draw their arc left or right from the top, with a gold dot when the filter is clean; a heart that was not saved shows as a dark empty heart.
  - 12 kit pictures and 20 instrument pictures are matched to the library's kits and instruments by me (`ajscreen.c`, top).
- Parent mode is in: `ajapp_parent_open`, `_row`, `_change` and `_close` run a three-row menu (default tempo in steps of 5, swap timing, Bluetooth). Each change is saved at once and the screen shows "Saved" or "Not saved"; a value that was not saved is not shown. The screen has small stroke letters for just the words it uses. "Next bar" swaps now work: a new loop waits for the bar line, the first press and the stop key behave as before. Bluetooth on means music and MIDI, off means both off. 421 checks pass.
- Not done or not known: which buttons move through the parent menu on the FM-1 (HOME + SAVE held 2 s opens it; the rest is for the device step); how long a repaint takes on the FM-1 (the play screen repaints 11 strips on every 16th note); the battery levels that mean 1, 2, 3 bars and "low"; the key LEDs.

## What I don't know yet, and how I'll find out

1. **Does a pattern swap click or stall?** Edits briefly hold the audio gate. Test on the desktop first, then on the first device build.
2. **How late can a swap land?** The control side decides when a write happens. If it runs every few milliseconds, swaps are tight; if not, they need to be scheduled a step ahead.
3. **Processor load of the five mix effects** on top of six loaded engines. Measured on the device with `fm1_audio_stats_read`; if it is too much, effects are capped as the PRD already allows.
4. **Does the sampler track need the stock sample-transfer code** to load the 8 recordings, and how much flash does that cost?
5. **Parent mode's "swap sampled sounds over USB"** depended on the stock workbench code AJ Box removes. It may need to come back, or be done by reinstalling.

## Desktop first

CLAUDE.md says new sound code gets a WAV and a test before it touches `firmware/`. The device will no longer run `dsp/src/engine.c`, so:

1. Build FM1 Open's portable instrument (`firmware/web/engine`, the code behind its browser simulator) for the desktop.
2. Move AJ Box's loop logic onto it as a small layer that makes the same calls the device will make.
3. Re-render the pairs and demos and re-run the behaviour tests against that. These WAVs are then what the device should sound like.
4. Keep the old engine only until the new renders match what you approved.

## One device build (Partho, Nov 14, 2026)

Everything goes onto the FM-1 in a single install: keys, swaps and the start-up screen; the full library and one-shots; effects and knobs; helper keys; jams, favourites and Mood; final screens; parent mode. Claude Code advised three installs; Partho chose one. The order above is kept for building and testing on the desktop. Before the first one: reinstall M-Flasher's bootloader, and I ask before the 518 MiB download needed to build the local update page.

## Decisions (Partho, Nov 14, 2026)

1. The design is approved to build.
2. No recorded sounds: Claude Code synthesizes the 8 one-shots.
3. Parent mode sample swapping is dropped for this build.
5. A real cuica for Goron City: `tools/make_cuica.c` synthesizes a high and a low cuica squeak. They take 2 of the sampler's 8 pads when the one-shots are built, and Goron Drums' cuica part moves from cowbell and rim onto them. Partho compared six versions on Nov 14, 2026 and chose the first (version A), which is what the tool makes.
4. Asked for: Saria's Song and Song of Storms (from Zelda: Ocarina of Time) in place of two melody loops. Done for three (Saria's Song, Song of Storms, Zelda's Lullaby, in place of Cat Song, La La La and Kitten Chase on page 4); Zelda's Lullaby and Goron City were added to the request. Goron City waits on its notes. The PRD's no-copyrighted-songs line is removed at Partho's instruction.

## Decisions I needed from you (answered above)

1. Is this design right to build?
2. The 8 one-shot sounds: which 8, and who records them. Placeholders will do until then.
3. Parent mode sample swapping (point 5 above): keep, or drop for this build?
