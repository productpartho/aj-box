# AJ Box: Build Plan

Nov 8, 2026 · @Partho

AJ Box is small: one PRD, one design pass, four build slices. The only real risk is the hardware (free flash, CPU headroom, sample slots), so a short spike still comes first.

## 1. Tighten the PRD

- [x] Close the 4 remaining open questions
- [ ] Add a numbered list of about 12 requirements (R1 to R12) at the top, so prompts and commits can cite them; no epics
- [ ] Export it to Markdown as `docs/aj-box-PRD.md`

## 2. One Claude Design pass

One prompt on the [AJ Box Screens canvas](https://claude.ai/artifact/Jy4F2F12j5jjQffjqJ5bsJ), in your usual two-block shape: the PRD's "Screen and look" section verbatim, then the instruction.

- [ ] 1× versions of the 4 screens at exactly 240 × 240
- [ ] The icon set at 48 × 48
- [ ] The few missing states: low battery, muted side, effect bubbles on and off
- [ ] Export the boards as PNGs into `docs/design/`

## 3. Repo

- [ ] `productpartho/aj-box`, with your fork of [m-vave/fm1\_open](https://github.com/m-vave/fm1_open) as a submodule at `firmware/`
- [ ] `docs/` holds the PRD, this plan and the design PNGs
- [ ] CLAUDE.md at the root:

```markdown
# AJ Box
Loop-toy firmware for the M-VAVE FM-1, for a 6-year-old.
Spec: docs/aj-box-PRD.md (cite R numbers). Screens: docs/design/.

- dsp/ portable C engine + Mac WAV renderer; firmware/ fork of m-vave/fm1_open (GPLv3); assets/ loops, samples, icons
- The PRD wins. If unsure, ask.
- New sound code: render a WAV before touching firmware/.
- Never flash the FM-1 or call a slice done from Mac tests: say "ready to flash" and stop.
- Size report on every build; audio stays under 500 KiB.
- Output limiter always on.
```

## 4. Build in four slices

1. [x] **S0 Spike.** Closed Nov 14, 2026, the short way by Partho's decision: FM1 Open V14 is installed and a project save survives a power-off; two more save rounds, device measurements and the trip back to stock V15 are deferred (`docs/s0-findings.md`, "S0 on the device"). Original step: Claude Code reads `firmware/` and reports build steps, free flash, CPU headroom, screen format and sample slots. On the device: read the firmware identity on the workbench update page (Connect only, install nothing); if it isn't `FM-1_015`, stop and ask. (Done on stock V15 on Nov 10, 2026. The unit has run Baud Girl since, and Partho chose on Nov 14 to carry on from there: read the identity again, and the FM1 Open install is a migration from that identity.) A full flash backup taken with M-Flasher is kept outside the repo (`docs/s0-findings.md`, "M-Flasher"). Save a preset on stock and check it survives a power-off. Flash Claude Code's build of stock FM1 Open; this first install asks for force-flash, which is expected only then (see "Flashing rules"). After each install, hold PLAY at power-on and note whether M-Flasher's recovery menu still appears. Save a project and a DX7 voice, switch off, unplug for a minute, switch on, three times; measure the S0 numbers; then go back to stock V15 through M-VAVE's own update route. *Verify: identity recorded, saves survive every power-off, both flashes work.*
2. **S1 Sound on the Mac.** Clock, the DRUM 808 engine, FM voice, sampler, limiter and a WAV renderer, with 4 drum and 4 melody loops. *Verify: she likes what she hears.*
3. **S2 AJ Box on the device.** Built as a new front for FM1 Open's stock instrument, not its own engine (decided Nov 14, 2026; `docs/s0-findings.md`, "S2 design finding"). First: settle the sampled sounds and the effects, rebuild the desktop renderer on FM1 Open's portable instrument (`firmware/web/engine`) so WAVs and tests match the device, and agree the design. Then, in this order, flashing after each: keys, swaps and the start-up screen; the full library and samples; effects and knobs; helper keys; jams, favourites and Mood; final screens; parent mode. Every build follows "Saving and update-mode rules"; the first build that saves anything saves one heart and passes the power-off test before more is added. *Verify: everything in the PRD works on the FM-1, in time and under budget.*
4. **S3 Play-test.** 3 sessions with her, fix where she gets stuck, tag v1.0. Play-test builds follow the same saving and update-mode rules; the v1.0 build passes the power-off test and a trip back to stock V15 before the tag. Before AJ Box goes to her, decide whether to put the stock bootloader back: with M-Flasher's bootloader, holding PLAY at power-on lands her in a recovery menu. Default: restore stock and keep the backup. *Verify: the PRD's success measures are met.*

## Flashing rules

- Charged battery, a data cable straight into the computer, nothing else holding the MIDI port, and never unplug mid-install.
- Keep the way back to stock tested. Stock V15 through M-VAVE's own update route is the way back.
- Install through M-VAVE's update protocol (the FM1 Open workbench or update page): that is the normal route for every build. Partho's unit has the original Puya chip (0x856014), which M-Flasher and the UBOOT recovery tool both support, so M-Flasher is the rescue route if an install leaves the FM-1 unable to reach the update page. Use it only after stopping and asking. On a UC25HQ80 unit recovery is weaker (`docs/s0-findings.md`).
- After any flash that touches saving, do the power-off test before calling it done.
- The first install of FM1 Open on a stock unit asks for force-flash: that is the update page's own route from V15 (`docs/s0-findings.md`, "Force-flash is the normal route"). Accept it only when the identity read just before was exactly `FM-1_015`, or, for this unit's S0 install only, exactly the Baud Girl identity recorded in the S0 log (approved once by Partho, Nov 14, 2026). Any other force-flash request, including any on a unit already running FM1 Open or AJ Box, means stop and ask.
- After every install, before anything else: reconnect on the update page and read the identity (Connect only). If the page can't see the FM-1, stop: that build has broken the way back.
- The unit had M-Flasher's open bootloader (`docs/s0-findings.md`, "M-Flasher"); the FM1 Open install on Nov 14, 2026 most likely put the stock one back (no recovery menu with PLAY held). Partho reinstalls M-Flasher's bootloader before the first build that is not an official M-VAVE or FM1 Open release, which is the first S2 build. After every install, hold PLAY at power-on and note whether its recovery menu still appears; an install may have put the stock bootloader back. Ask Partho before reinstalling it.
- If a build opens the recovery menu by itself, its checksum failed: fix the build. "Boot anyway" is never the routine, and "Wipe user data" is never used.
- "Restore original V15" always writes (it replaces the four voice banks even on a unit already on V15). Press it only when going back to stock.

## Saving and update-mode rules (S2, S3 and every later build)

These turn the CLAUDE.md flash rule into steps. Partho's unit has the Puya chip (0x856014); the rules hold for both chips, so AJ Box stays portable to UC25HQ80 units. They apply to every build that goes on the FM-1, including play-test builds in S3.

Saving:

- Storage calls are `fm1_flash_size`, `fm1_flash_read`, `fm1_flash_write`, `fm1_flash_erase` and `fm1_flash_map`, and nothing else. No SPI driver, no chip-ID read or compare anywhere in AJ Box, no table of known chips.
- Flash calls run only in the control task, never in audio, timer, MIDI or SysEx callbacks (`fm1.h`). A save asked for elsewhere is queued.
- Each thing saved (hearts, the jam, the power-off memory, parent settings) is a record with a version and a checksum, kept in two sectors written in turn, with a commit mark written last. A save that is cut off by switch-off leaves the older copy readable. A record must fit one 4 KiB sector with its header: the jam is 1,024 events today (4,136 bytes), so cap it at 1,000 events when saving is built.
- Every return code is checked. After the last write the record is read back with `fm1_flash_read` and compared byte for byte with what was meant to be saved, even though the SDK also verifies.
- The screen shows a heart, a jam or a parent setting as saved only after that compare passes. On any failure the heart stays empty (PRD "Risks"), and nothing is retried silently.
- If `fm1_flash_size()` is 0 or too small, AJ Box still plays, but every save shows as failed. It never keeps saves in RAM and shows them as saved. This is decided by the SDK's answer, never by the chip.
- AJ Box's records live outside the 466,944-byte sample bank. Their place is fixed from the S0 flash numbers before the first saving build.
- First saving build: one heart only, then the power-off test (save, off, unplugged 1 minute, on, three times) before jams, memory and parent settings are added. Repeat the test whenever the save code changes, and once more on the v1.0 build.

Keeping update mode reachable:

- The update handler is inside M-VAVE's sealed SDK and runs before AJ Box's SysEx hook. AJ Box never handles, filters or replaces update messages (commands 17, 48, 49, 50).
- The SDK start-up and USB-MIDI bring-up in `app/board/` stay exactly as FM1 Open has them, and run before any AJ Box code.
- Nothing AJ Box does at start-up can hang or crash: no flash writes at start-up, no unbounded loops, and every saved record is checked (version, checksum, every index in range) before it is used. A record that fails is ignored and the factory defaults are used.
- The control task always returns to the SDK; interrupts are never left off; audio code keeps to the SDK's RAM-section rules so it is safe while flash is being written.
- `FM1_UPDATE_PRODUCT` stays in the `FM-1_8<6 digits>` form, so the page accepts AJ Box builds and the way back to V15 without force-flash.
- Each S2 step is a small change on top of a build that is already proven to reach the update page. If an install fails or the page stops seeing the FM-1, stop and ask before trying anything else. M-Flasher is the rescue route, not a way to install builds.

Open items for Partho:

- The PRD has no picture for "the jam didn't save" or "the parent setting didn't save" (only the empty heart). Needed before those saves are built.
- PRD "Playback rules" 10 saves 30 s after the last change, but a flash write briefly silences the sound. S0 did not measure the gap (deferred); measure it on the first S2 build that saves, then decide whether to save only while nothing is playing.
