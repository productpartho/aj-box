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
- Size report on every firmware build; audio stays under the budget measured in S0 (target 100 KiB).
- Output limiter always on.
- Palette is fixed: plum #2A0A3D, pink #FF5FA2, magenta #E0218A, grape #7B2CBF, lilac #C9A0FF, gold #FFD166, cream #FFF4FB, dusk #3D1656.
- Keep upstream licence headers; check a licence before borrowing code from another firmware.
- Tick boxes in docs/aj-box-build-plan.md as slices finish.
