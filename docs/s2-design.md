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

## The device build (Nov 15, 2026): ready to flash, not flashed

Built with M-VAVE's unchanged `sdk/Build.ps1 -FirmwareOnly`, on branch `ajbox` of the firmware fork. Nothing has been installed.

**Size report**

| | Bytes |
| --- | --- |
| App image | 439,092 of 453,921 allowed: 14,829 to spare |
| Stock FM1 Open V14, for comparison | 453,576 |
| Memory left for the library's heap | 192,076 (the script wants at least 49,152) |
| One-shot sample bank (in the user partition, not in the image) | 51,128 of the 446,464 the stock bank holds |

Every check in the script passed: sound-interrupt code in fast memory, Bluetooth and USB parts present, start-up entry, package fits 1 MiB.

**What is in `firmware/app` now**

- Kept from FM1 Open, unchanged: `board/` (start-up, USB, panel scan, screen driver), `midi_router.c`, `samples.c`, the headers.
- Changed by one call: `audio_output.c` runs AJ Box's mix effects ahead of the volume and the limiter.
- Removed: FM1 Open's editor (`screen.c`), projects (`project.c`), presets, workbench transfers, voice library and 128-voice factory bank.
- New: `screen.c` (AJ Box's front: the library's entry points, the controls, the screen), `transfer.c` (three empty entry points), `aj_fm.c` (the 11 factory DX7 voices AJ Box uses), and `ajbox/`, a copy of AJ Box's shared code made by `tools/sync_firmware.sh`. Edit the originals in `dsp/stock`, `dsp/src`, `dsp/include` and `assets`, never the copies.
- `version.h`: the identity the update page shows is `FM-1_8660001`, so an AJ Box install can be told from FM1 Open (`FM-1_8000014`).

**The desktop tests now run on those same files.** `dsp/stock/build.sh` builds FM1 Open's simulator with `firmware/app` as the FM-1 build has it. 485 checks pass, 64 of them new ones that press the simulator's keys, buttons and knobs (`dsp/stock/ajpanel.c`).

**Decided while building (say if any is wrong)**

- **Controls not in the PRD:** SELECT moves 4 BPM a click. KNOB 1 to 4 move 4 of 127 a click (32 clicks end to end). HOME and SAVE act when let go, so that holding both for parent mode does neither job. In parent mode SELECT picks the row, any of KNOB 1 to 4 changes it, HOME leaves, and everything else is ignored.
- **Lights:** the playing loop's key is lit while its page is shown; a Dice pick blinks twice; effect buttons, PLAY, REC and ARP are lit while on; OCT− or OCT+ is lit when the octave is down or up; Fill, Fast and Mute are lit while held.
- **Battery:** the readings that mean full and low are not known, so the battery picture and the sleepy screen are left out of this build. Parent mode shows the raw reading in its corner (`b 900`), to note at full charge and when nearly flat. Next to it, `l 0` counts how often the sound fell behind, which is the processor-load measurement S0 left open.
- **No workbench transfers:** AJ Box does not answer the workbench's project, voice or sample pages. Firmware updates are not affected; M-VAVE's library handles those.

**The one-shot sounds have to be put on the FM-1 separately, before the install.** They are 51 KiB, too big for the image, and they belong in the stock sample bank, which a firmware update leaves alone. `ajstock bank build/ajbox-oneshots.fm1s` writes them as a workbench sample bank file. While the FM-1 is still on FM1 Open V14: workbench, sample page, "Restore sample bank file", pick that file, write to device. This replaces all 8 sample slots. If it is skipped, the 5 drum loops that use the one-shots play the FM-1's own samples or nothing.

**Found on the way**

- Code the sound interrupt calls has to sit in the FM-1's fast memory, because it can run while flash is being written (`fm1.h`). AJ Box's mix effects and their tables are placed there (7.7 KiB), and the build was checked by listing every call they make: none leave fast memory. A 64-bit conversion that called a flash routine was changed to 32 bits. The interrupt also no longer asks the instrument for the playing step; `ajapp_tick` hands it over.
- Pressing ARP while music was playing stopped the clock and never restarted it. Fixed (`ajapp_replay`), with a check.
- AJ Box's saved records use sectors 4 to 11 of the user partition, where FM1 Open keeps project slots 5 to 12. The two test projects saved during S0 will be overwritten by the first heart or setting saved.

**Not known until it runs on the FM-1**

- Whether the processor keeps up with six engines, five mix effects and the screen (watch `l` in parent mode).
- How fast the screen repaints. It sends one 16-row strip per pass of the control loop; the play screen needs 11 strips every 16th note.
- Whether Bluetooth switches as asked, and whether the mix effects glitch when a knob changes them mid-block.
- The save gap against PRD rule 10, still unmeasured.

**The old engine's build** (`dsp/build.ps1`) compiles FM1 Open's `fm_factory.c`, which this firmware branch no longer has. Fixed later the same day: the build now takes that file from FM1 Open V14 (commit `4c7496d` of the submodule) into `dsp/build/fm1open`, and tells the shared code it is the old engine (`AJB_OWN_ENGINE`). Its 6,008 checks pass again. It is not what goes on the FM-1.

### The update page (Nov 15, 2026): built, not used

Built with M-VAVE's unchanged `sdk/Build.ps1 -Package`. Nothing has been installed.

- **Page:** `firmware/build/official-web/FM1-Official-Open-Source.html`, 2,453,074 bytes, SHA-256 `27b1d411…a938`. Open it in Chrome or Edge on the PC.
- **What it holds:** the AJ Box package (479,796 bytes, identity `FM-1_8660001`, Build ID `77d1d6db72053920`, SHA-256 `6de4a5b8…266f`) and M-VAVE's original V15 as the page's own recovery image. The app image is the same 439,092 bytes and every script check passed again.
- **What it calls itself:** the page is M-VAVE's template, so it says "FM1 Workbench" and "Official Open firmware V14". Tell it from the hosted FM1 Open page by the Build ID under "Firmware details".
- **Force update:** the page's own rule (`sdk/WebUpdate.js`) accepts any `FM-1_8…` identity as an Open build, so going from `FM-1_8000014` to AJ Box should not ask for force update. If it asks, stop.
- **Checked on this PC:** the page loads with no script errors, shows "Firmware verified. Waiting for device connection", and its browser panel draws AJ Box's play screen. Connect was not pressed.
- **Its other tabs don't apply:** "Play & edit" and "Projects & sounds" are FM1 Open's editor and do nothing useful with AJ Box (`firmware/web/engine/aj_stubs.c` gives them empty answers). Don't use "Save to device" or the sample page from this page; write the one-shot bank from the hosted FM1 Open workbench, before the install.
- **Tools:** WASI SDK 27 is at `C:\JL\wasi-sdk\wasi-sdk-27.0-x86_64-windows` (outside OneDrive; archive hash matched the one pinned in `tools/Build-Web.ps1`). To rebuild, in `firmware/`: `$env:FM1_WASI_SDK = 'C:\JL\wasi-sdk\wasi-sdk-27.0-x86_64-windows'; .\sdk\Build.ps1 -Package -Toolchain 'C:\JL\pi32' -SourceUrl 'https://github.com/productpartho/fm1_open/tree/ajbox'`. Don't redirect its output with `*>`: PowerShell 5.1 then stops at the first compiler message.
- **Source link:** the page's licence notice points at branch `ajbox` of the fork, which is not pushed. Push it before giving the page to anyone else (GPL).
- **Two changes the browser compile needed** (it is stricter than the FM-1 compile): `assets/loops.c` says its left-out fields are meant, and `ajbox_target.h` recognises the browser build like the FM-1 build.
- **One layout difference from FM1 Open V14:** AJ Box's image is smaller, so M-VAVE's packager starts the FM-1's own settings area (Bluetooth pairings and the like) 12 KiB earlier (458,752 instead of 471,040) and makes it 45,056 bytes instead of 32,768. The user partition, where the sample bank and AJ Box's saves live, starts at the same place (507,904). I expect the FM-1 to forget its Bluetooth pairings at the install; I can't confirm that from the desktop.

### First run on the FM-1 (Nov 15, 2026), and the second build

Partho installed build `77d1d6db72053920` through the update page. Identity before `FM-1_8000014`, after `FM-1_8660001`, no force update asked. M-Flasher's bootloader was not reinstalled (build plan, step 2).

**What worked:** hello screen and chime, play screen, drum and melody loops, key and PLAY lights, a heart (shown as number 1), a jam recorded and replayed, a parent setting ("Saved"). Parent-mode readings: `b 612`, `l 0`: the sound never fell behind. The power-off test has not been done yet.

**The update page's warning after the install** ("sample initialization is incomplete. Write the built-in bank from the sampler page") is expected and must be ignored: the page asks the FM-1 about its sample bank, AJ Box does not answer the workbench, and after ten tries the page says this. It writes nothing unless the FM-1 replies that the bank is empty.

**Bug found: the screen stayed half painted after the heart picture.** The screen is sent in 15 strips, one per control pass, and every pass started again from the top. With a pair playing, the tiles (strips 3 to 13) changed again before a pass reached the bottom, so the bottom strips kept the old picture until something else changed. The desktop tests missed it because they repaint the whole screen before looking. Fixed in `ajscreen_draw`: strips now take turns, carrying on from the last one sent, and a strip the screen driver reports as lost is sent again. New test `test_repaint` plays a pair at 200 BPM, saves a heart and compares what one-strip-a-pass painting left with a full repaint; it failed on the old code (33 rows left over) and passes now.

**Pages 1 and 4 swapped, at Partho's request:** Around the World / Animal Choir is now page 1 and Bouncy Pop / Twinkle page 4, on both sides (PRD "Loop library and storage" updated to match). The loops, their tile pictures, page kits and per-loop volumes moved together. Hearts, the jam and the remembered pair store loops by position, so anything saved from page 1 or 4 with the first build plays the other page's loop at the same key after this build.

**Second build:** app image 439,060 of 453,921 bytes, every script check passed, Build ID `8e19300bcf4417d1`, page SHA-256 `3fa17555…2839`, package SHA-256 `4b3c6026…b4d9`. 488 desktop checks pass. Not installed yet.

**Old engine:** 1 of its 6,008 checks fails after the page swap (Trap Cat alone peaks at -3.76 dBFS, wanted -6 +/- 2). The same loop passes its level check on the stock instrument, which is what the FM-1 runs. Not looked into.

**Brightness:** the screen driver already sets the panel to its maximum (command 0x51, 255) and there is no backlight dimmer; lights are on or off.

### Second run on the FM-1 (Nov 15, 2026), and the third build

Partho installed the second build (`8e19300bcf4417d1`) and reported five things.

**White noise, "really loud", after melody Dice or on Star Steps.** The chip (PSG), pluck and bass engines each have one set of settings per track, and an instrument only wrote the settings it names. Crunchy Chip sets the chip's MODE to MIX (tone and noise); Chip Star, Game Lead and Bloop never set MODE back, so they played with the noise on after it. Dice finds this quickly. Fixed in `apply_instrument`: every setting of the engine is written, the ones an instrument doesn't name at the engine's own default. New test `test_leaks` plays 240 melody loops in random order and compares each loop's engine settings and loudness with the same loop from a fresh start; it fails on the old code (67 checks) and passes now. The desktop engine's noise is not loud enough to show in the loudness check alone, which is why the settings are compared directly.

**Notes that stay on when another melody loop starts (Zelda's Lullaby).** A held note is the same note in each cell, tied to the next. The stock sequencer lets a tied note go only when another note cell arrives, so emptying or moving the pattern mid-note left it sounding for good. Fixed in `melody_put`: when the cell now playing is tied and the next is empty, that note is written once more there, untied with the shortest gate, which the sequencer keeps without striking again and lets go; `ajapp_tick` takes the cell away once it has played. New test `test_hanging` swaps and stops all 48 melody loops at three different moments each and listens for sound 3 s later; it failed on the old code and passes now.

**The tile picture didn't change with the page while a loop played.** It showed the playing loop's page; the page dots showed the page the keys play. The picture now follows the page shown, like the dots.

**Tearing while the pictures move.** The screen is sent a strip at a time, so a moving picture is briefly part old, part new. Two changes: only the strips that changed are sent (the picture's 5 strips instead of the tile's 11), and the picture now hops up on the beat and comes down on the next 16th (two repaints a beat) instead of moving on every 16th. Not checkable on the desktop; to be looked at on the FM-1. How long the FM-1 takes per strip is still unmeasured.

**Drum sound "could be a bit better and cleaner".** Not looked at yet: needs to know which loops and what is wrong.

**Third build:** app image 439,732 of 453,921 bytes (14,189 to spare), heap 192,044, every script check passed, Build ID `daaa1eb6f784f553`, page SHA-256 `98406133…ae9a`, package SHA-256 `ef4dbe9d…3fce`. 1,119 desktop checks pass. Not installed yet. The old engine's tests were not rerun.

### Knobs changed at Partho's request (Nov 15, 2026), and the fourth build

Partho didn't like holding OCT- or OCT+ and turning ALGORITHM for Mood. His choice, now in the PRD's control map:

- **KNOB 3:** drums and melody balance (was KNOB 4).
- **KNOB 4:** Mood. Each pass that the knob turns steps one Mood, either way, wrapping round, with the Mood picture shown.
- **How fast the effects go** (was KNOB 3) is a fourth row in parent mode, "Effect speed": Slow, Normal or Fast for all effects at once. Normal is each effect's own speed, as before (Stutter an 8th note, the rest the middle). It is stored in the parent record's spare byte, so settings saved by earlier builds still load, as Normal.
- **OCT- and OCT+** only move the octave now; both together still reset it.

The parent screen has four rows (36 pixels apart instead of 46), and three letters were added to the screen's letters for the new words (E, F, c). Speeds saved in hearts and jams by earlier builds are overridden by the parent setting.

**Fourth build:** app image 440,308 of 453,921 bytes (13,613 to spare), heap 192,044, every script check passed, Build ID `8db777a7c5fc3d43`, page SHA-256 `d430c956…b043`, package SHA-256 `12211cf9…c3b6`. 1,129 desktop checks pass. Not installed yet; it also carries the third build's fixes, which Partho has not installed either.

### Fourth build on the FM-1, and the tempo pop-up (Nov 15, 2026)

Partho installed the fourth build (`8db777a7c5fc3d43`): "worked great". That covers the white noise, the stuck notes, the tile picture following the page, and the new knobs, as he found them on the FM-1; he did not report on each item separately, and the tearing and the power-off test are still open.

**Added at his request: a pop-up when SELECT changes the tempo**, like the instrument's. It shows the speed arrows, the BPM in big figures and a gold arc that fills from 30 to 200 BPM, for 1.2 s. While a Mood or heart picture is up, it waits behind it. PRD "Screens" updated.

**Fifth build:** app image 440,692 of 453,921 bytes (13,229 to spare), heap 192,044, every script check passed, Build ID `254d09aeb93dfda6`, page SHA-256 `a7d019e8…84aa`, package SHA-256 `4843b02d…3a56`. 1,134 desktop checks pass. Not installed yet.

### Partho's report on the FM-1, Nov 15, 2026 (evening)

In his words: "drums are fine now, tearing is better, power off tests work too."

- **Drum sound:** fine now. Nothing was changed for it on purpose; the settings fix (an instrument or kit writing every setting of its engine) is the likely reason, not confirmed.
- **Tearing:** better.
- **Power-off test:** hearts, the jam and a parent setting came back after switching off. Not yet recorded: which build it was run on (the fourth, `8db777a7c5fc3d43`, or the fifth, `254d09aeb93dfda6`) and whether it was the three rounds the plan asks for.

**Still open before S2's "everything in the PRD works on the FM-1" can be said:**

- The battery picture and the sleepy screen are left out: only one battery reading is known (`b 612`); a reading at full charge and one near flat are needed.
- The sound gap while saving (PRD rule 10) has not been measured or asked about.
- The PRD's pictures for "the jam didn't save" and "the setting didn't save" are not drawn.
- The old desktop engine has 1 of 6,008 checks failing since the page swap.

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
