# S0 Findings: FM1 Open V14 (read-only)

Nov 9, 2026 · read from `firmware/` at `4c7496d` ("Fix Quick Edit display and support JieLi 2.5.3"). Nothing in `firmware/` was changed, built or flashed. The FM-1 hasn't arrived, so every on-device number below is **measure on device**.

## Summary

| Question | Answer |
| --- | --- |
| Toolchain | JieLi Windows toolchain **2.5.3** (PI32v2 clang 4.0.1, `r3` CPU) for the device; WASI SDK 27 for the browser preview. Neither is installed on this PC yet. |
| Desktop build of the engines | **Yes.** The DRUM 808 and DX7 sources compile cleanly for Windows with clang 23 (LLVM-MinGW), zero warnings, using the browser build's own flags. `dsp/` now links them directly. |
| Audio format | 44,100 Hz nominal (hardware clock gives 44,118 Hz), **128-frame blocks**, stereo interleaved, **signed Q23 in int32** (24-bit I2S). Engines mix on mono **Q24 int32** buses; the 808 is float inside. |
| Sample slots | **8 pads**, one shared bank of **466,944 bytes**, stored as **SEA5 at 32 kHz** (228 bytes per 320 frames, 5.7 bits per sample), **not IMA ADPCM**. About 19.5 to 20.4 s of audio in total. |
| Screen | 240 × 240 ST7789 over SPI, **RGB565**, native-endian in the API (the driver byte-swaps), DMA bands of at most 240 × 16 pixels. |
| Free flash after a build | **Measure on device** (the build prints it; see "Numbers to read in the spike"). |
| CPU headroom | **Measure on device** (counters exist; see "Numbers to read in the spike"). |

**Decided Nov 10, 2026** (details under "Decisions for Partho"): samples use the stock **SEA5 at 32 kHz** format, and AJ Box runs **its own engine** in place of the sealed stock sequencer. The PRD is updated to match.

## How to build it on Windows

From `README.md` and `sdk/Build.ps1`:

1. Install **JieLi Windows toolchain 2.5.3** (`https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe`, linked from the README). It installs to `C:\JL\pi32` by default.
2. The build hashes all 22 toolchain files against `sdk/manifest.json` and refuses anything else (2.5.2 is rejected). The expected SHA-256 of `pi32/bin/clang.exe` is `d4bc97cf421c1ccc373dd8924ce88720ff7457b3f1478d15df4aeda6ae923bce`.
3. Local device build, no distribution URL needed:

   ```powershell
   ./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'
   ```

   Outputs go to `firmware/build/dx7/` (gitignored): `sdk.elf`, `sdk.map`, `firmware-report.json`, `package-report.json`, and `generated/FM1_OFFICIAL_OPEN.fwsc`, the file the workbench installs.
4. Browser preview: `./tools/Build-Web.ps1 -LocalPreview`. The first run downloads WASI SDK 27 (or set `FM1_WASI_SDK`). Output: `build/preview/index.html`.
5. `build.bat --package` makes a distribution workbench and needs `FM1_SOURCE_URL` set to a matching source archive (GPL corresponding source). Not needed for home use.

The build also checks `sdk/synth.json` against the binary engine (`libfm1_engine.a`), fixes the sample rate at 44,100 and the block at 128 frames, and rejects coefficient edits to `app/synth/*.inc` unless matching native kernels come with them.

**Going back to stock:** the SDK ships the original M-VAVE V15 recovery image, `sdk/firmware/FM-1_015.fwsc` (SHA-256 `db1642b2…edb8a`), and the workbench offers "return to original V15". This matches the build plan's flashing rule "Keep the way back to stock V15 tested".

## How the engines are structured

Device firmware = **APP** (`app/`, GPLv3 with the SDK linking exception, compiled from source) + **sealed engine** (`sdk/lib/libfm1_engine.a`, binary, FM-1-only licence). The portable C in `third_party/synth/` and `web/synth/` is what the browser simulator compiles. The device links native builds of the same kernels from the archive (several hand-written in assembly, for example `dx7_compute_*_asm` and `drum808_q24_add32_asm`), held to the same coefficient tables by `synth.json`.

### DRUM 808

- **Source:** `third_party/synth/drum808.c` and `drum808_state.h`, **MIT** (Electrosmith/DaisySP and Emilie Gillet/Plaits, plus M-VAVE), 277 lines. Maths helpers and the trapezoidal SVF are in `third_party/synth/dsp_math.h` (MIT).
- **Parts:** 11 (`D8_BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH`). Four have an alternate sound: toms ↔ congas, rim ↔ clave, clap ↔ maracas. Each part has Level, Tune, Decay, Drive, Dist type (7 kinds), Reverb and Delay sends and Pan; snare adds Tone and Snappy; kick adds Tone and Attack. Kit parameters are Level, Accent and hi-hat Choke.
- **DSP:** float, **44,100 Hz hard-coded** in its coefficients. `drum808_render(d, dry, rev, dly, count)` adds float samples to the buses for any count.
- **Kit wrapper:** `web/synth/voices.c` (GPL), `drum808_kit_*`. It renders 32-frame chunks into a 128-frame block, converts float to Q24 (`circuit_sample`) and applies a voice limit. On the device, `drum808_kit_render_st` is a native kernel that lives in SRAM.
- **Kits:** a kit is a 209-byte preset (`D8_PRESET_BYTES`): 11 sound selectors plus two sets of 99 parameter bytes. PRD "Sound swaps" asks for 12 kits on PRESETS; that is 12 × 209 = 2,508 bytes.

### DX7

- **Source:** `web/synth/dx7*.c`. The FM core, envelopes, LFO, pitch envelope and note scaling are an **Apache-2.0** C port of Google's MSFA/Dexed; `dx7synth.c` and `dx7math.c` are GPL glue. All fixed-point.
- **API** (`synth_dx7.h`): `synth_dx7_create(max_voices, sample_rate, block)` allocates once with `calloc` at creation and never in the audio path. Notes go in as MIDI messages, with the 156-byte expanded patch passed on each call; `synth_dx7_render` adds a mono Q24 block. The internal quantum is 64 samples, and 64- and 128-frame calls are both valid.
- **Voices:** 128 factory voices (original M-VAVE V15 bank, Apache-2.0) in `app/fm_factory.inc`, LZ-packed; `groove_fm_factory_raw()` unpacks them. Good PRD "Twinkle" candidates: 120 TUB BELLS, 112 CLANG BELL, 100 FROGBELL, 117 KALIMBA.
- **Size:** a stored voice is 128 bytes raw (112 packed), which matches PRD "Storage math" step 7 (24 voices × 128 bytes = 3 KiB).

### Other engines in V14 (not in the PRD yet)

FM1 Open ships three more melodic engines, each with portable C for the browser build and a native kernel in the sealed archive (listed in `sdk/synth.json`), so all three already run on the device:

- **Pluck** (`third_party/synth/pluck.c`, **MIT**, DaisySP String model): plucked string, 8 voices, knobs Decay, Bright, Tune, Release. Good for harp, kalimba-like and toy-guitar sounds.
- **PSG** (`web/synth/psg.c` on `third_party/synth/emu2149.c`, **MIT**): AY-3-8910 square-wave and noise chip, 8 voices on up to 3 emulated chips. Chiptune.
- **303** (`third_party/synth/bass303.c`, **MIT**, Open303): acid bass with filter, resonance, accent, slide and drive; 4 voices or mono.

Partho approved adding them, and PRD "Sound swaps" now lists all four synth engines for the 20 synthesized instruments.

### Do they compile for desktop?

**Yes.** With `-std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off -DFM1_HOST_TEST -DFM1_OPEN_WEB` (the browser build's flags), these compile with zero warnings under clang 23 on Windows: `third_party/synth/drum808.c`, `web/synth/{voices,dx7core,dx7env,dx7lfo,dx7math,dx7note,dx7pitchenv,dx7synth,fixed,adsr,mixer}.c`, `web/engine/effects.c` and `app/fm_factory.c`. `dsp/build.ps1` compiles them straight from `firmware/`, so desktop renders run the same C the browser simulator runs. The one exception is the 25-line DX7 VMEM unpacker, copied with its header into `dsp/src/fm1_glue.c` because the rest of `app/fm_library.c` needs the device Flash API.

What desktop can't show: whether the native device kernels match the portable C bit-for-bit. The build's contract checks say they should; confirm by ear on the device.

### Other reusable pieces

- **Master limiter:** `fm1_master_process128()` in `web/synth/mixer.c` is a linked stereo peak limiter with a −0.5 dBFS ceiling, one block of lookahead and about 186 ms release. `dsp/` uses it as the always-on output limiter (CLAUDE.md "Output limiter always on").
- **Reverb and delay:** `fm1_fx_*` (`web/engine/effects.c`), tempo-syncable delay up to 2.04 s. Relevant to S2 Effects (Echo, Sparkle).

## Audio sample rate and buffer format

- `sdk/include/fm1_block.h`: `FM1_SAMPLE_RATE 44100`, `FM1_BLOCK_FRAMES 128`, `FM1_CHANNELS 2`.
- `app/board/fm1_audio.h`: the I2S peripheral (ALNK0) runs 24-bit stereo from an 11.2896 MHz clock at 256 fs. `FM1_AUDIO_HARDWARE_RATE` is **44,118 Hz**, about 0.04% fast, which doesn't matter for a toy. Double-buffered DMA raises one interrupt per 128-frame block (2.90 ms).
- `fm1_engine_render(int32_t *output, 128)` fills **interleaved L/R signed Q23** (24-bit in int32). `app/audio_output.c` applies MASTER gain, mixes USB and Bluetooth audio, and hard-clips to 24 bits.
- Internal buses are **mono Q24 int32** (`fixed.h`: 128× headroom over full scale), mixed to Q23 stereo by `fm1_bus_output128` and then the master limiter.

## Sample slots and format

- `app/synth/sampledrum.h`: `SAMPLE_DRUM_PADS = 8` (matches PRD "Also decided": 8 sampled sounds in 8 slots), `SAMPLE_DRUM_VOICES = 24`.
- **One shared bank**, `SAMPLE_DRUM_BANK_BYTES = 466,944` (456 KiB) including a 512-byte header. Pads have no fixed per-pad length; one sample may use the whole bank.
- **Codec: SEA5** (`sdk/include/fm1_sea.h`, MIT SEA codec by Dani Biro): mono, 320-frame chunks of **228 bytes** (5.7 bits per sample), 20-frame groups with 4-bit scales and an LMS predictor. Decoding is native (`fm1_sea_decode_groups`), and `web/synth/sea_encode.h` is the encoder.
- **Rate: 32,000 Hz** (`app/samples.c` rejects banks whose header isn't 32000). Maximum 654,400 frames = 20.45 s; the README says "about 19.55 seconds of new sample storage".
- Playback (`web/synth/sampledrum.c`): per-pad Level, Tune (±24 semitones), Length, Choke, Variation and Velocity LPF; one-shot drum pads with a 32-frame end fade. There's no slicing and no looping.
- The user Flash partition is **536,576 bytes** (`user_bytes` in `Build.ps1`). `fm1.h` warns that the stock instrument already uses this same partition for projects, the DX7 library and the sample bank: "it is not extra free space".

## Screen colour format

- `fm1.h`: `FM1_SCREEN_WIDTH/HEIGHT 240`. This confirms the PRD "Screen and look" assumption of 240 × 240.
- Controller **ST7789** (`app/board/lcd.inc`), `COLMOD 0x55` = **RGB565, 16 bits per pixel**, display inversion on. `fm1_screen_blit` and `fm1_screen_fill` take native-endian `uint16_t`; `fm1_screen_submit` is non-blocking DMA of at most `240 × 16` pixels and byte-swaps the buffer in place.
- The CLAUDE.md palette in RGB565:

  | Name | Hex | RGB565 |
  | --- | --- | --- |
  | plum | #2A0A3D | `0x2847` |
  | pink | #FF5FA2 | `0xFAF4` |
  | magenta | #E0218A | `0xE111` |
  | grape | #7B2CBF | `0x7977` |
  | lilac | #C9A0FF | `0xCD1F` |
  | gold | #FFD166 | `0xFE8C` |
  | cream | #FFF4FB | `0xFFBF` |
  | dusk | #3D1656 | `0x38AA` |

## Related answers found on the way

- **Key LEDs** (PRD open question in "Feel"): `fm1_led_set(key, on)` is on/off only, driven from the key scan matrix (`fm1_led[11]` bytes). The colour is fixed by the hardware, so plan on single-colour LEDs. Check the colour on the device.
- **Controls:** `fm1_controls`: 27 key bits (F3 to G5), 14 buttons, 7 encoder deltas, the MASTER pot and battery as 10-bit ADC values. This matches PRD "FM-1 hardware this design works within".
- **Tempo:** stock internal tempo is 30 to 300 BPM; PRD "Playback rules" 1 narrows it to 30 to 200.
- **Flash chip (PRD "Risks"):** Partho's unit has the original Puya chip, JEDEC ID 0x856014, read by M-Flasher on Nov 13, 2026 ("M-Flasher" below). An earlier version of this note said it was the newer UC25HQ80 (0xB36014); that was wrong. Support for both chips stays as a portability rule. In the sealed SDK (`sdk/lib/libfm1_engine.a`), `read_flash_id` sends the standard ID command and stores the result in `flash_info`; `norflash_ioctl` sizes the chip from the ID's capacity byte (it calls `_pow`); `fm1_flash_init` uses the size and `fm1_layout`, with no ID compare. Neither 0x856014 (Puya) nor 0xB36014 (UC25HQ80) appears anywhere in the library. Both chips report capacity byte 0x14 (1 MiB). So no whitelist was found, but the pi32 code can't be disassembled here: the S0 power-off test on the device is the proof.
  - Outside evidence ([Melodee issue #110](https://github.com/keremimo/melodee/issues/110)): on a 0xB36014 unit, stock V15 and Baud Girl FM-1+VA save and keep settings; Melodee went RAM-only because `persist_boot()` set `flash_ok = (fl_jedec_ram() == 0x856014u)`, fixed in 1.6.0. No report yet for FM1 Open on the UC25HQ80. This is background now: Partho's unit has the Puya chip FM1 Open was written for.
  - Firmware identity: the update page (`sdk/WebUpdate.js`) asks the device for a 27-byte identity (MIDI command 17) on Connect, which writes nothing. Stock V15 answers `FM-1_015`; FM1 Open answers the `FM1_UPDATE_PRODUCT` in `app/version.h` (`FM-1_8000014` for V14).
  - **Force-flash is the normal route from stock to FM1 Open** (checked in `Updater.connect` and `install`, Nov 10, 2026). With an FM1 Open package loaded, any identity that is neither the package's own product nor an Open pattern (`FM1_OFFICIAL_OPEN`, `FM1_COMMUNITY_<n>`, `FM-1_8<6 digits>`) is a "migration", and that includes stock `FM-1_015`. Connect then only reports the identity; Install refuses until "Enable force flash and reconnect" is on, which puts a `FM-1_015` unit on the page's own V15 route (`mode = 'legacy'`). So the first install of FM1 Open on a stock unit always asks for force-flash. An unexpected force-flash request is one where the identity is not exactly `FM-1_015`, or one on a unit already running FM1 Open or AJ Box.
  - Going back: "Restore original V15" loads the stock package (SHA-256 pinned in the page). A unit on FM1 Open answers with an Open identity and the capability command (49), so no force-flash is needed. The restore also rewrites all four voice banks with the 128 factory voices, and on a unit already on V15 it does only that, so it is never a read-only action.
  - AJ Box builds must keep an identity the page accepts: `sdk/Build.ps1` requires `FM1_UPDATE_PRODUCT` to match `FM-1_8<6 digits>`. Keeping it means every later AJ Box install and the way back to V15 run without force-flash.
  - Update mode is in the sealed SDK, not the app: `fm1.h` says "Firmware update messages are handled before" the app's SysEx hook, and nothing in `app/` handles commands 17, 49 or 50 or calls `fm1_device_recovery()`. So the app can't remove the update route, but it can starve it: a crash or endless loop at start-up, interrupts left off, or a control task that never returns would stop the SDK answering. No button-hold that forces update mode was found in the open code; treat it as not existing.
  - Flash calls and audio: `fm1.h` allows `fm1_flash_read/write/erase` only from the control task ("Never from audio, timer, MIDI or SysEx callbacks") and says "Writes/erases briefly silence audio". `fm1_flash_write` already verifies by read-back and returns `FM1_FLASH_IO` on a mismatch; `fm1_flash_size()` returns 0 if the reservation can't be validated. The raw API is not transactional; FM1 Open itself uses commit markers and a staging sector (`app/project.c`, `app/fm_library.c`).
  - Recovery: on the Puya chip (this unit) M-Flasher and the UBOOT recovery tool both support the chip (Partho, Nov 14, 2026; M-Flasher read a full backup from this unit). On the UC25HQ80 the UBOOT recovery tool still refuses the chip (fix on an untested branch). Installing through M-VAVE's update protocol stays the normal route; M-Flasher is the rescue route.

## M-Flasher (Nov 13, 2026)

Partho ran M-Flasher on the unit. From the backup's own metadata file:

| Item | Value |
| --- | --- |
| Tool | m-flasher 0.12.0, loader "M-Flasher 1.3, CPU at 96 MHz" |
| Model | FM-1 |
| Flash chip | 0x856014 (Puya) |
| Bootloader at backup time | stock (M-VAVE) |
| Firmware at backup time | "FM-1 102 (Baud Girl)", app OK |
| Full backup | `Documents\M-Flasher\backups\fm1-20261113-164913.bin`, 1,048,576 bytes (the whole 1 MiB), SHA-256 `c639430c…115342f`, matching the metadata; kept outside the repo |

- The backup is of the unit as it was on Nov 13, running Baud Girl firmware. It is not a backup of stock V15.
- Confirmed by Partho, Nov 14, 2026: the unit is still running Baud Girl. The FM1 Open install in step c was never done, so nothing from FM1 Open has been on this unit yet.
- Confirmed by Partho, Nov 14, 2026: after the backup, M-Flasher installed its open bootloader (m-bootloader, github.com/jvitkauskas/m-bootloader) on the unit. Per its README (not yet tried on this unit): PLAY held at power-on opens a recovery menu (Boot app, Download mode, Boot anyway, Wipe user data); the feature list says REC held at power-on enters download mode; every firmware's checksum is checked before it starts, and a build that fails opens the menu by itself. Official M-VAVE updates put the stock bootloader back; the README doesn't say whether FM1 Open's update page does. To reinstall it: `m-flasher bootloader --file fm1-uboot.boot` (writes the bootloader only).
- Bootloader rules for every device step: after each install, Partho holds PLAY at power-on and the result (menu appears or not) goes in the log; then ask him whether to reinstall the bootloader. If a build opens the menu by itself, that is a failing checksum: fix the build; "Boot anyway" is never the routine. "Wipe user data" is never used in any step.
- Steps a and b below were done on stock V15 on Nov 10. The firmware has changed since, so the identity has to be read again before any install.

## S0 on the device (log)

Unit: Puya flash, JEDEC ID 0x856014 (M-Flasher, Nov 13, 2026). Every install is done by Partho through the update page. A step that fails stops the run. After each install (steps c and f) the log also records the PLAY-at-power-on check: whether m-bootloader's recovery menu still appears.

| Step | What | Result |
| --- | --- | --- |
| a | Identity on Connect (nothing installed) | **Pass**, Nov 10, 2026: hosted workbench (open.m-vave.com/fm1-workbench, Official Open firmware V14 page), Connect device only, identity `FM-1_015` (stock V15) |
| b | Stock baseline: save a preset, off, unplugged 1 minute, on | **Pass**, Nov 10, 2026: on stock V15 a changed ALGORITHM on preset 001 was saved with SAVE and was still there after switch-off, USB unplugged for 1 minute, switch-on. Stock firmware saves on this chip |
| c | Build unmodified FM1 Open (size report), install through the update page | **Build pass**, Nov 10, 2026 (FM1 Open V14 at 4c7496d, JieLi toolchain 2.5.3, `clang.exe` hash matches): app image 453,576 bytes; flash image 471,040; VM 32,768; BTIF 4,096; user partition 536,576 (0x7C000 to 0xFF000); total 1,044,480 of 1,048,576 with no spare gap (`vm_extra_gap_bytes` 0). Package `FM1_OFFICIAL_OPEN.fwsc` 492,084 bytes, build id `bb7938ce…757d`. **Install pass**, Nov 14, 2026: official FM1 Open V14 from the hosted workbench (Partho's choice; not the local build), force-flash from identity `FM-1_102` (Baud Girl), ended with "Update complete. Your FM1 is ready to use; no further flashing is needed." Identity re-read and PLAY-at-power-on check: not run yet |
| c2 | Identity on Baud Girl (Connect only, nothing installed) | **Pass**, Nov 14, 2026: hosted workbench, Connect device only, identity `FM-1_102` (Baud Girl FM-1+VA version 102). This is the expected identity for the FM1 Open install. Decision, Nov 14, 2026: S0 carries on from Baud Girl instead of going back to stock first. Partho allows force-flash once for the FM1 Open install, from exactly the identity this read shows; anything else means stop |
| d | Power-off test × 3: a project and a DX7 voice survive | not run yet |
| e | Free flash, audio deadline and overruns, screen format, sample slots, sound gap while saving | not run yet |
| f | Back to stock V15 through the page; boots; presets save | not run yet |

## Numbers to read in the spike (measure on device)

| Number | Where it comes from |
| --- | --- |
| App image size | `build/dx7/firmware-report.json` → `app_image_bytes`; the build prints "FM1: N bytes app image" |
| Free flash | `build/dx7/package-report.json` → `FLASH_BIN_SIZE`, `VM_REAL_SIZE`, `user_bytes`; the build prints "Offline package fits 1 MiB: …" |
| Audio deadline | `fm1_audio_stats_read()` → `peak_ticks`, `overruns` (ticks of `fm1_device_clock()`; one block = clock × 128 ÷ 44,118) |
| Sound gap while saving | Save a project while a pattern plays on FM1 Open and listen; `fm1.h` says writes "briefly silence audio" (PRD "Playback rules" 10 saves 30 s after the last change, possibly mid-play) |
| User flash at run time | `fm1_flash_size()`; 0 means the SDK could not validate the partition on this chip |
| Engine CPU | `groove_audio_stats()` → `render_ticks`, `peak_ticks`, `deadline_ticks`, `overloads` |
| PRD worst case | 8 drum + 4 FM + 2 sample voices + 6 effects (PRD "Success, milestones and risks", Spike); also time `dsp/` once it runs on the device |

## Decisions for Partho

Answered Nov 10, 2026: **1(b) SEA5 at 32 kHz** (`dsp/` now uses FM1 Open's SEA5 encoder, decoder and tables) and **2: AJ Box's own engine**. Partho installs the JieLi toolchain for the firmware build.

1. **Sample codec and rate.** PRD "Storage math" assumes IMA ADPCM at 16 kHz (8,000 bytes per second). The stock sampler is SEA5 at 32 kHz (22,800 bytes per second, 2.85× more, with twice the bandwidth). The PRD's 12.8 s of audio (4 × 2.4 s loops + 4 × 0.8 s notes) would take about **285 KiB as SEA5**, which still fits the stock 456 KiB bank (62%), against **100 KiB as IMA**. Options:
   - **(a) Keep IMA ADPCM 16 kHz** with AJ Box's own sampler, as the PRD says. `dsp/` does this, per the S1 brief. It uses less flash but needs its own Flash region, and the slicing code is new.
   - **(b) Use the stock SEA5 bank and its native decoder.** Better treble, reuses the workbench's sample manager (PRD parent mode "swap any of the 8 sampled sounds over USB"), but uses about 2.85× the space.

   Swapping the codec in `dsp/` is a small change. I'd decide after the free-flash number comes in.
2. **Stock sequencer or AJ Box clock.** The stock instrument ("groove": 8 tracks, 64-step patterns at 16, 32, 48 or 64 steps, per-track source DX7, 808, sampler and others) is inside the sealed library, driven by `groove_*` calls. It doesn't do phase-locked swaps or sliced loops. `app/board/audio.c` (APP code) is what calls `fm1_engine_render()`, so S2 can call AJ Box's renderer there instead and drive the native `drum808_kit_*` and `synth_dx7_*` kernels directly. Those symbols are in the archive (`Build.ps1` requires them in SRAM). Whether the APP may call them without the groove engine initialised needs checking in S2.
3. **Build plan wording.** The plan says "go back to V15" while the PRD bases on FM1 Open **V14**. These are consistent: V14 is the open firmware and V15 is M-VAVE's original stock firmware, the recovery image.
