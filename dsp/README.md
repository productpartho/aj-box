| `tests/test_main.c` | Timing, swap step, stop, clipping, codec and sampler tests |
| `tests/test_s2.c` | Pages, Dice, held keys, Finish, effects, sound swaps, hearts, jams, memory |
| `tests/test_library.c` | Factory library: counts, pattern shapes, sample bank budget, levels, pairs on every page |# dsp/

The AJ Box sound engine as portable C, plus a Windows renderer that writes WAV files (build plan S1 and the sound side of S2).

The DRUM 808 circuit, the DX7, Pluck, PSG and 303 melody engines, the master limiter and reverb/delay are compiled unchanged from `firmware/` (FM1 Open V14), so loops use the same DSP code as the FM1 Open browser simulator. AJ Box adds the master clock, phase-locked swaps, the controls in PRD "Control map", a SEA5 sampler (FM1 Open's stock codec) and the loop library: 6 pages of 8 drum and 8 melody loops, 12 kits and 24 instruments (`assets/loops.c`, one file per page in `assets/loops/`). The 8 sampled sounds are synthesized placeholders until the real recordings exist.

```powershell
./dsp/build.ps1            # build ajbox_render.exe and ajbox_tests.exe, print the size report
./dsp/build.ps1 -Test      # ... and run the tests
./dsp/build.ps1 -Render    # ... and render to renders/ (about 480 MB): page1..page6 same-key pairs,
                           #     moods/ mood tours, demos/ the controls, extras/ sampler demos
./dsp/build.ps1 -Assets    # regenerate assets/samples/*.c from tools/ (placeholder sounds)
```

The build needs clang for Windows (`winget install MartinStorsjo.LLVM-MinGW.UCRT`), or set `AJB_CC`.

```powershell
dsp/build/ajbox_render.exe one 2 1 out.wav 8 120   # page 1: drum 2 + melody 1, 8 bars at 120 BPM (0 = none)
dsp/build/ajbox_render.exe one 3.5 6.2 out.wav 8 110 spooky   # page 3 drum 5 + page 6 melody 2, Spooky mood
dsp/build/ajbox_render.exe page 4 renders/page4    # all 64 pairs of page 4
dsp/build/ajbox_render.exe levels                  # each loop's peak where it meets the limiter
```

| File | What it is |
| --- | --- |
| `include/ajbox.h` | Engine API: press, tempo, render 128 frames of Q23 stereo |
| `include/ajbox_library.h` | Loop, kit, instrument and sample types |
| `src/clock.h` | Exact integer 16th-step clock |
| `src/engine.c` | Sides, swaps, stop fades, mixing and limiter |
| `src/sampler.c`, `src/sea5.c` | 32 kHz SEA5 voice: loop slices and pitched notes |
| `src/fm1_glue.c` | DX7 VMEM unpacker taken from `firmware/app/fm_library.c` |
| `render/main.c` | Command-line renderer |
| `tests/test_main.c` | Timing, swap step, stop, clipping, codec and sampler tests |

## Levels

Each loop alone peaks near -6 dBFS (drums) or -9 dBFS (melody) where it meets the limiter, and the 12 kits sit within about 2.5 dB of Pop on four page-1 loops (Sleepy is the soft one). After changing a pattern, voice or kit, run `ajbox_render levels` and copy the suggested gains into `assets/loops/`; `tests/test_library.c` fails if a loop drifts more than 2 dB.

Three engine rules keep surprises down: the Fill plays at its own gain whichever loop is running, the Finish chord plays 6 dB down on the engines that sound all its notes, and chip (PSG) notes treat velocity 100 and up as full volume because the chip's volume steps are much coarser than the other engines'. An instrument turned onto another loop with ALGORITHM can still be a few dB louder or softer than on its own loops; the limiter covers the loud end.
