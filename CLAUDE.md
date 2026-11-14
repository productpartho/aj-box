# AJ Box

Loop-toy firmware for the M-VAVE FM-1, for a 6-year-old.
Spec: docs/aj-box-PRD.md (cite section names). Plan: docs/aj-box-build-plan.md.
Screens: docs/design/*.dc.html, true size 240 x 240 (Icons and Stickers are reference sheets). Read the inline styles for exact sizes and colours.

## Layout
- dsp/       portable C engine + a desktop (Windows) renderer that writes WAV files
- firmware/  submodule: fork of m-vave/fm1_open V14 (GPLv3 with SDK linking exception)
- assets/    loop manifest, 8 samples, icon sheets
- tools/     sample encoder, icon packer, size report

## Rules
- The PRD wins. If code and PRD disagree, stop and ask; don't guess.
- One slice at a time (S0 to S3 in the plan); each ends with its verification step.
- New sound code: render a WAV and add a test before touching firmware/.
- Never flash the FM-1 and never call a slice done from desktop tests alone: say "ready to flash" and stop.
- Size report on every firmware build; sampled audio (SEA5, 32 kHz) stays inside the stock 456 KiB sample bank (PRD "Storage math": about 285 KiB).
- Output limiter always on.
- Flash chip (every slice, every build; steps in the build plan's "Saving and update-mode rules"): Partho's unit has the original Puya chip (ID 0x856014, read by M-Flasher). FM-1s made since autumn 2026 ship a UC25HQ80 (ID 0xB36014), and AJ Box must work on both as a portability rule. Both are 1 MiB with 4 KiB sectors. Save only through the SDK's fm1_flash_read/write/erase/map calls; never add an SPI flash driver, a chip-ID check, or a RAM-only fallback. Check every return code, read back after each write, and never show a heart, jam or parent setting as saved unless the read-back matches. Firmwares that broke on the new chip (Melodee before 1.6.0) did so through a hard-coded `== 0x856014` check; don't write one. Install through M-VAVE's update protocol; M-Flasher is the rescue route on Partho's unit, not a way to install builds. A bad build must still never be able to stop the FM-1 reaching M-VAVE's update mode.
- Palette is fixed: plum #2A0A3D, pink #FF5FA2, magenta #E0218A, grape #7B2CBF, lilac #C9A0FF, gold #FFD166, cream #FFF4FB, dusk #3D1656.
- Keep upstream licence headers; check a licence before borrowing code from another firmware.
- Tick boxes in docs/aj-box-build-plan.md as slices finish.
