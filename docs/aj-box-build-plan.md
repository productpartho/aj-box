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

1. **S0 Spike.** Claude Code reads `firmware/` and reports build steps, free flash, CPU headroom, screen format and sample slots; you flash its build of stock FM1 Open, then go back to V15. *Verify: both flashes work.*
2. **S1 Sound on the Mac.** Clock, the DRUM 808 engine, FM voice, sampler, limiter and a WAV renderer, with 4 drum and 4 melody loops. *Verify: she likes what she hears.*
3. **S2 AJ Box on the device.** In this order, flashing after each: keys, swaps and the start-up screen; the full library and samples; effects and knobs; helper keys; jams, favourites and Mood; final screens; parent mode. *Verify: everything in the PRD works on the FM-1, in time and under budget.*
4. **S3 Play-test.** 3 sessions with her, fix where she gets stuck, tag v1.0. *Verify: the PRD's success measures are met.*

## Flashing rules

- Charged battery, a data cable straight into the computer, nothing else holding the MIDI port, and never unplug mid-install.
- Keep the way back to stock V15 tested.
