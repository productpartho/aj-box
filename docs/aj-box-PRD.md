# AJ Box: FM-1 Loop Toy PRD

Nov 8, 2026 · @Partho

## Summary

AJ Box turns the M-VAVE FM-1 into a two-handed loop toy for a 6-year-old: the left 8 white keys start drum loops, the right 8 white keys start melodic loops, and the top-row buttons plus 4 knobs add effects. Everything shares one tempo and one key, so any drum plus any melody she picks sounds good.

Synthesized drums use FM1 Open's built-in DRUM 808 engine, so AJ Box doesn't wait on the Volca Drum-style FM-1 project; the two are separate firmwares.

**Decisions locked on Nov 8, 2026**

| Decision | Choice | Why |
| --- | --- | --- |
| Sound source | Hybrid: 8 sampled sounds, the rest synthesized | Recorded audio for \~100 loops will not fit in the FM-1's 1 MiB (mebibyte) of flash |
| Key layout | White keys split 8 and 8 | The 11 black keys become simple helper actions |
| Layering | 1 drum loop + 1 melody loop at a time | A new press on the same side swaps; always sounds musical |
| Effects controls | Buttons switch an effect on, knobs set how much | Top-row function buttons plus KNOB 1 to 4 |
| Drum engine | FM1 Open's built-in DRUM 808 | No waiting on the Volca Drum-style project |
| Firmware | AJ Box is its own firmware | Separate from the drum synth; no mode switch to build |
| Parent mode | Included | Default tempo, swap timing, Bluetooth and sample swaps, behind HOME + SAVE |

**Also decided**

- "About 50" became **48 per side**: 6 pages of 8 keys, so every page is full.
- The **8 sampled sounds split 4 drum loops and 4 melody sounds, matching FM1 Open's 8 sample slots**.
- Name: AJ Box, shown on the start-up screen every time it switches on.
- Effects apply to the whole mix, drums and melody together.
- Default tempo at switch-on is 110 BPM; check it in the first listening session.
- The 8 sampled sounds stay as listed: dhol, pots and pans, beatbox, rain stick, her "la", cat meow, kalimba, music box. Her "la" and the cat get recorded at home.

## Who it's for and what it must do

The primary user is Partho's daughter, age 6, an early reader who learns by pressing things. The secondary user is Partho, who builds it, loads sounds and sets limits.

**Problem:** the stock FM-1 is a DX7-style FM (frequency modulation) synth with 128 presets, 32 algorithms and multi-page parameter menus. Getting to a groove takes reading and menu diving she can't do yet.

**Goals**

1. Sound within 5 seconds of switching on, with no reading required.
2. Any drum plus any melody sounds musical: one shared tempo, one shared key, swaps that land in time.
3. She can change the feel herself: pages of loops, sound swaps, and effects she hears instantly.
4. She can't get stuck: PLAY/STOP always means silence, HOME always means a clean slate.

**Non-goals**

- Step editing, song arrangement, or any menu outside parent mode.
- More than 2 loops playing at once.
- MIDI (Musical Instrument Digital Interface) features beyond what the base firmware already keeps.

## FM-1 hardware this design works within

AJ Box uses every physical control on the FM-1; the tight limit is storage, not controls. Facts below are from the [FM-1 user manual](https://m.media-amazon.com/images/I/A1WOydif9HL.pdf) unless linked otherwise.

| Part | What the FM-1 has | AJ Box use |
| --- | --- | --- |
| Keys | 27 silicone keys, each with an LED (light-emitting diode); default range F3 to G5, which works out to 16 white and 11 black keys | 8 drum loops, 8 melody loops, 11 helpers |
| Function buttons, top row | FX, SEL, ENV, LFO, EDIT, GLO | 6 effect switches |
| Function buttons, bottom row | HOME, SAVE, ARP, SEQ, PLAY/STOP, REC | Clean slate, favourites, jam record and replay, stop and resume |
| Octave buttons | OCT−, OCT+ | Melody octave down and up |
| Encoders | KNOB 1 to 4 on the right; SELECT, PRESETS, ALGORITHM on the left; 7 endless encoders in total | Effects and balance; tempo, drum kit, melody instrument |
| Volume | MASTER potentiometer (300° turn) | Volume, full range |
| Screen | 1.54-inch TFT (thin-film transistor) colour display | Pink and purple play screen |
| Audio out | Built-in speaker; plugging into the 3.5 mm jack mutes it | Mix tuned for the small speaker first |
| Power | 2000 mAh (milliamp-hour) battery, rated 12 hours | Low battery shown as a sleepy face |
| Storage | 1 MiB of flash in total ([FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter/wiki)); one sampler firmware gets 500 KiB (kibibytes) usable for audio ([Pigeon 0.5 beta](https://github.com/TheShrimpBlimp/pigeon/releases)) | Sets the 8-sample limit |

- **Stock limit that goes away:** the stock firmware won't run the arpeggiator and sequencer together; it shows "Disable SEQ to enter". AJ Box runs its own clock, so both styles of melody can exist side by side.
- **Base to build on:** M-VAVE's official [FM1 Open V14](https://github.com/m-vave/fm1_open/releases/tag/V14) (Oct 19, 2026) already has a DX7 engine, sample playback, 8 sample slots, reverb and delay sends, tap tempo with tempo-synced delay, and a browser workbench for managing samples ([Synthtopia](https://synthtopia.com/content/2026/10/20/m-vave-officially-opens-fm-1-firmware-to-developers-with-new-workbench-sdk)). It is licensed GPLv3 (GNU General Public License v3) with an SDK (Software Development Kit) linking exception.

## Control map

On day one she only needs the 16 white keys and PLAY/STOP; the knobs and effect buttons come next, and the black keys and sound swaps come when she's ready. Everything works from the start, but you introduce it in those three layers.

&#91;embedded content: FM-1 key map · 16 loop keys, 11 helper keys\]

The white keys hold the loops; the 5 black keys on each side mirror each other, with Finish alone in the middle.

**Keys**

| Action | Drum side | Melody side | What happens |
| --- | --- | --- | --- |
| Loop keys | 8 white keys, F3 to F4 | 8 white keys, G4 to G5 | Tap: start or swap that loop. Tap the playing one again: that side stops |
| Back | F#3 | G#4 | Previous page of 8 loops |
| Next | G#3 | A#4 | Next page of 8 loops |
| Hold to play | A#3, Fill | C#5, Fast | Drum fill, or melody notes twice as fast, only while held |
| Mute | C#4 | D#5 | That side drops out while held and comes back on the beat |
| Dice | D#4 | F#5 | A surprise loop from any page on that side |
| Finish | F#4 (middle) | F#4 (middle) | Big ending on the next bar, then everything stops |

The black keys carry printed labels (OP1 to OP6, PIT, GLO, MONO, POLY) for the stock editor. A pink and purple sticker strip with pictures covers them.

**Buttons and knobs**

| Control | AJ Box job |
| --- | --- |
| FX, SEL, ENV, LFO, EDIT, GLO | The 6 effect switches (see Effects) |
| KNOB 1 (pink) | Swoosh: a filter on the whole mix. Left sounds underwater, right sounds like a tiny radio, the middle is clean |
| KNOB 2 (purple) | How much: strength of the effect switched on last |
| KNOB 3 (lilac) | How fast: speed of the effect switched on last, locked to the beat |
| KNOB 4 (magenta) | Drums ↔ melody balance |
| SELECT | Speed: turtle to rabbit, 30 to 200 BPM (beats per minute) |
| PRESETS | Drum kit swap: same pattern, new sounds (Robot, Kitchen, Space and more). Its stock job is scrolling sounds, so the habit carries over |
| ALGORITHM | Melody instrument swap: same tune, new instrument |
| OCT− and OCT+ | Melody one octave down or up; both together reset |
| PLAY/STOP | Stop everything; press again to bring back the last drum and melody |
| HOME | Clean slate: every effect off, filter back to the middle, home screen |
| REC | Record a jam: everything she presses and turns, up to 2 minutes; tap again to stop |
| ARP | Replay: plays her last jam back |
| SAVE | Save a favourite: the current drum, melody, sounds and effects become a heart (8 hearts; a new one replaces the oldest) |
| SEQ | Favourites: each tap jumps to the next heart |
| Hold OCT− or OCT+ and turn ALGORITHM | Mood: Happy (C major pentatonic), Dreamy (F major pentatonic) or Spooky (C minor pentatonic), changing on the next bar |
| HOME + SAVE held 2 s | Parent mode, hard for a 6-year-old to hit by accident |

## Loop library and storage

48 drum loops and 48 melody loops sit in 6 themed pages of 8 per side. 8 of the 96 use real recorded audio; the other 88 are synthesized from tiny pattern data, which is why the whole library fits.

| Page | Drum theme | Melody theme |
| --- | --- | --- |
| 1 | Bouncy Pop | Twinkle (music box, bells) |
| 2 | Disco Dance | Bubblegum (bright synth chords) |
| 3 | Head-Nod (hip-hop, lo-fi) | Space (lasers, bloops, pads) |
| 4 | Around the World (dhol, tabla, samba, reggaeton) | Animal Choir (cat, bird, voices) |
| 5 | Silly Kitchen (pots, spoons, beatbox) | Game On (chiptune) |
| 6 | Sleepy Time (brushes, rain stick) | Dreamy (kalimba, harp, toy piano) |

**The 8 sampled sounds**

- **Drums (4),** each stored as a 1-bar loop cut into 16 slices: dhol, pots and pans, beatbox, rain stick. Slices are re-triggered by the clock, so a sampled loop follows any tempo without changing pitch.
- **Melody (4),** each stored as a single note that the pattern plays at different pitches: her own "la" (recorded at home), cat meow, kalimba, music box. One recording can play any tune in any key.

**How a melody loop is built:** instrument plus pattern. About half are arpeggios (a held chord played Up, Down, Bounce or Random, the same shapes as the stock arpeggiator) and half are 16-step riffs. Melodies use only the 5 notes of one pentatonic scale (default C major pentatonic: C, D, E, G, A), so nothing clashes. All patterns are original or public domain; no copyrighted songs.

**Sound swaps:** PRESETS steps through 12 drum kits built on the DRUM 808 engine and ALGORITHM through 24 melody instruments (20 FM, 4 sampled). A sampled drum loop keeps its own sound; turning PRESETS on one just shows a sparkle.

**Storage math** (IMA ADPCM, Interactive Multimedia Association adaptive differential pulse-code modulation: 4 bits per sample, mono)

1. At 16 kHz: 16,000 × 4 ÷ 8 = **8,000 bytes per second**.
2. 1 bar at 100 BPM = 4 beats × 0.6 s = 2.4 s, so 2.4 × 8,000 = **19,200 bytes** per drum loop.
3. 4 drum loops: 4 × 19,200 = **76,800 bytes (75 KiB)**.
4. 4 melody notes at 0.8 s each: 4 × 0.8 × 8,000 = **25,600 bytes (25 KiB)**.
5. Total audio: 102,400 bytes = **100 KiB, or 20% of a 500 KiB budget**.
6. That leaves room for 2-bar drum loops: 153,600 + 25,600 = 179,200 bytes = 175 KiB (35%), if the spike confirms the space.
7. The synthesized side is small: 24 DX7 voices at 128 bytes each = 3,072 bytes (3 KiB); 96 patterns × 64 steps × 4 bytes = 24,576 bytes (24 KiB).

Her saved jam and favourites are tiny: one jam of up to 2 minutes stored as key and knob events (5 events per second × 120 s × 4 bytes = 2,400 bytes) plus 8 favourites at 32 bytes each (256 bytes) come to 2,656 bytes, under 3 KiB.

16 kHz caps the audio at 8 kHz of treble: fine on the built-in speaker, a little dull on headphones. 22.05 kHz would cost 37.8% more space (22,050 ÷ 16,000 = 1.378).

## Playback rules

Every press answers instantly and still lands in time: a new loop starts from whatever step the beat clock is on (phase-locked), instead of waiting for the next bar. At the 110 BPM default, waiting for the next bar could mean up to 2.18 seconds of nothing (4 × 60 ÷ 110 = 2.18), which a 6-year-old reads as "broken".

1. **One clock** for both sides: default 110 BPM, range 30 to 200 in 2 BPM steps on SELECT, with bigger steps when the knob turns fast.
2. **From silence,** the first press starts the clock at step 1 of that loop.
3. **New key on the same side:** swap immediately, phase-locked.
4. **Same key again:** that side stops on the next beat, with a 10 ms fade so there's no click.
5. **Two at most:** 1 drum + 1 melody. There is no third layer to manage.
6. **Loop lengths** are 1, 2 or 4 bars, and all line up on a 4-bar cycle.
7. **Finish** (F#4): on the next bar, a crash cymbal plus the melody's final chord, a confetti animation, then stop.
8. **PLAY/STOP** stops both sides; pressing it again resumes the last pair at the last tempo.
9. **Sound swaps** (PRESETS, ALGORITHM) change on the next note, never in the middle of one.
10. **Memory:** last pages, last pair, sounds, tempo and effects survive power-off. Saving happens after 30 s with no changes, to spare the flash.

Parent mode can switch swaps to "next bar" for a classic DJ feel once she's older.

## Effects

The 6 top-row buttons each switch one effect on or off for the whole mix; KNOB 2 (how much) and KNOB 3 (how fast) then shape whichever effect was switched on last.

| Button | Name on screen | What she hears | Built from | Kid-safe limit |
| --- | --- | --- | --- | --- |
| FX | Echo | Repeats in time with the beat | Delay, tempo-synced | Feedback max 60%, so it never runs away |
| SEL | Sparkle | A big shiny room | Reverb, Hall type | Mix max 50% |
| ENV | Wobble | A swirly, wobbly sound | Phaser | Depth capped |
| LFO | Wah | A filter sweeping up and down with the beat | Filter moved by an LFO (low-frequency oscillator) | Resonance capped so it never whistles |
| EDIT | Crunch | Crunchy robot grit | Distortion plus bit reduction | Level compensated so it never gets louder |
| GLO | Stutter | Hold it and the last 1/8 note repeats; let go and the loop carries on | New: beat repeat | Works only while held |

- **Any combination** can be on. Each shows on screen as a coloured bubble and its button LED lights. If the spike shows 6 at once overloads the CPU (central processing unit), cap it at 3 and drop the oldest.
- **HOME** switches everything off at once.
- **Stock effects reused:** the FM-1 already has Filter, Reverb, Delay, Distortion, Chorus and Phaser. AJ Box skips Chorus (too subtle for her to notice) and adds Stutter.
- **KNOB 1's filter is always available,** with no button. The encoders have no centre stop, so the middle 3 clicks snap to "clean" and the screen shows a clean zone.

## Screen and look

The screen splits down the middle to match her hands: pink drums on the left, purple melody on the right, with pictures instead of words everywhere.

**Layout** (assumes 240 × 240 pixels, common for 1.54-inch panels; confirm in the spike)

- **Top strip:** 4 beat dots pulsing with the tempo, plus a turtle or rabbit for speed.
- **Left half, pink:** a big picture of the playing drum loop, bouncing on every beat, with 6 page dots below.
- **Right half, purple:** a picture of the playing melody with notes floating up, and 6 page dots below.
- **Bottom strip:** a bubble for each effect that's on. Turning a knob draws an arc in that knob's colour.
- **States by glow, not words:** playing glows, muted dims, a Dice pick spins in.

While REC is on, a dot pulses in the top strip. SAVE and SEQ show a heart with its number (1 to 8). Mood shows a sun, a cloud or a moon.

| Role | Colour | Hex |
| --- | --- | --- |
| Background | Deep plum | #2A0A3D |
| Drums | Bubblegum pink | #FF5FA2 |
| Drum highlight | Hot magenta | #E0218A |
| Melody | Grape purple | #7B2CBF |
| Melody highlight | Lilac | #C9A0FF |
| Sparkle accent | Gold | #FFD166 |
| Text and outlines | Cream | #FFF4FB |

**Feel**

- Every loop, kit and instrument gets a 48 × 48 picture: chunky rounded shapes, thick cream outlines.
- Everything moves on the beat; confetti on Finish; a sleepy face for low battery.
- Start-up screen: "AJ Box" in big rounded letters, pink and purple with a gold sparkle, for about 1.5 s with a 1-second hello chime at low volume. Then the home screen with the last jam ready, under 3 s from switch-on.
- Key LEDs: playing key solid, Dice pick blinks twice. Colour depends on what the LEDs can do (open question).

**Physical touches**

- A sticker strip over the keys: pink above the drum keys, purple above the melody keys, a small picture on each black key (arrows, drum, sparkle, dice, star for Finish).
- Coloured knob caps for KNOB 1 to 4 (pink, purple, lilac, magenta), if aftermarket caps fit the FM-1's shafts.

**Prior art:** [Gnomad](https://www.matrixsynth.com/2026/10/gnomad-synth-v1-for-m-vave-fm-1.html) (Oct 2026), a free kid-focused FM-1 firmware, already shows sounds as pictures and colour-codes its 4 knobs. Worth playing with before designing the icons.

## Safety and build approach

Nothing she does can distort the sound, delete anything, or leave her stuck; the recommended build is a fork of M-VAVE's official FM1 Open firmware.

**Safety**

- **Limiter:** a limiter on the output stops stacked effects and loops from clipping or jumping in level. MASTER keeps its full volume range.
- **Effects are range-limited** (see Effects), so Echo can't feed back and Crunch can't jump in level.
- **Nothing to break:** the factory library is read-only; a new jam or heart only replaces her oldest one.
- **Always a way back:** PLAY/STOP means silence, HOME means clean slate.
- **Bluetooth off** by default; parent mode turns it on, and long-pressing HOME doesn't toggle it.

**Parent mode** (hold HOME + SAVE for 2 s)

- Default tempo (110 BPM to start)
- Swap timing: phase-locked (default) or next bar
- Bluetooth on or off
- Swap any of the 8 sampled sounds over USB (Universal Serial Bus) from the browser workbench, for example new recordings of her voice

The tempo range (30 to 200 BPM) is fixed in the build.

**Where to build it**

| Option | Upside | Downside |
| --- | --- | --- |
| Fork FM1 Open (recommended) | Official and maintained; DX7 engine, DRUM 808 engine, 8 sample slots and workbench already exist | A new codebase to learn |
| [fm1-dock](https://github.com/joakimeriksson/fm1-dock) module | Installs beside other synths; easy to switch between | Module size limits and sample support unclear |
| Fork [Gnomad](https://github.com/kpalaniuk/gnomad-firmware) | Kid-friendly screens and loops already built | Licence to check; inherits its design choices |

**How it fits together**

1. **Engines:** FM1 Open's FM voices for melodies; FM1 Open's DRUM 808 engine for synthesized drums; one ADPCM sample player with 2 voices (1 sliced drum, 1 pitched melody note).
2. **Clock:** one master clock in 16th-note steps drives both sides; swaps are phase-locked to it.
3. **Library:** one manifest file listing all 96 loops, 12 kits, 24 instruments and their pictures, compiled into the firmware.
4. **Workflow with Claude Code:** render loops to WAV files on the PC and tune by ear, then run them in the browser preview, then flash the FM-1.
5. **Licence:** GPLv3 is fine for home use; if you share the firmware publicly, publish the source with it.

## Success, milestones and risks

AJ Box is done when she starts a drum and a melody without help on her first try and plays for 10 minutes without asking for help.

**Success measures**

- Switch-on to first sound: under 5 s.
- First session: drum plus melody started unaided within 2 minutes.
- She picks it up unprompted at least 3 times in the first 2 weeks.
- Zero "it's broken" moments (stuck state, silence she can't fix, crash) across her first 5 sessions.

**Milestones**

1. **Spike:** build FM1 Open; measure free flash, worst-case CPU (8 drum voices + 4 FM voices + 2 sample voices + 6 effects), screen resolution, key LED colours and the supported sample format.
2. **Core toy:** 8 + 8 keys on one page, phase-locked swaps, PLAY/STOP, synthesized loops only.
3. **Library:** 48 + 48 loops, paging, sound swaps, the 8 samples.
4. **Magic:** 6 effects, 4 knobs, Fill, Fast, Mute, Dice, Finish, jam recording, favourites and Mood.
5. **Pink and purple:** screen art, animation, sticker strip.
6. **Play-test with her:** 3 sessions, watch without helping, fix wherever she gets stuck, then add parent mode.

**Risks**

| Risk | Fallback |
| --- | --- |
| Less than 100 KiB free for audio once code and engines are in | Drop to 12 kHz, or fewer sampled sounds |
| 6 effects at once overload the CPU | Cap at 3 at a time, oldest drops off |
| Small speaker loses bass | Mix every loop on the built-in speaker first |
| Too many controls for a 6-year-old | Introduce the three layers over days, not all at once |
| Some newer FM-1s ship a different flash chip (UC25HQ80) that some firmwares don't support, losing saves at switch-off ([Drey Andersson](https://dreyandersson.com/blog/m-vave-fm-1-custom-firmware/)) | Check the unit's chip before flashing; confirm support with the base firmware's maintainers |

## Sources

- [M-VAVE FM-1 user manual (PDF)](https://m.media-amazon.com/images/I/A1WOydif9HL.pdf)
- [FM1 Open V14 release, GitHub](https://github.com/m-vave/fm1_open/releases/tag/V14)
- [Synthtopia: M-VAVE opens FM-1 firmware to developers](https://synthtopia.com/content/2026/10/20/m-vave-officially-opens-fm-1-firmware-to-developers-with-new-workbench-sdk)
- [Pigeon 0.5 beta release notes, GitHub](https://github.com/TheShrimpBlimp/pigeon/releases)
- [FM-1 Transporter, GitHub](https://github.com/kurogedelic/FM-1-transporter/wiki)
- [Gnomad Synth V1, MATRIXSYNTH](https://www.matrixsynth.com/2026/10/gnomad-synth-v1-for-m-vave-fm-1.html)
- [Drey Andersson: FM-1 custom firmware](https://dreyandersson.com/blog/m-vave-fm-1-custom-firmware/)
- [fm1-dock, GitHub](https://github.com/joakimeriksson/fm1-dock)
