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
| Screens, icons and drawing | what is left: about 40,000 |

The screens are the unknown. Icons at 48 × 48 in the fixed 8-colour palette have to be stored packed. The first build reports the real size, and every build after it.

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

## Build order on the device (unchanged from the build plan)

Keys, swaps and the start-up screen; the full library and one-shots; effects and knobs; helper keys; jams, favourites and Mood; final screens; parent mode. Each is a small change on a build that already reaches the update page. Before the first one: reinstall M-Flasher's bootloader, and I ask before the 518 MiB download needed to build the local update page.

## Decisions I need from you

1. Is this design right to build?
2. The 8 one-shot sounds: which 8, and who records them. Placeholders will do until then.
3. Parent mode sample swapping (point 5 above): keep, or drop for this build?
