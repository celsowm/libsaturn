# Dino Satiator boot investigation

## Report and scope

The physical Saturn freezes on the "SEGA Licensed" screen after the loading
screen was introduced. This is a boot regression report, not a confirmed music
loading failure. The last visible image cannot establish whether the application
entry point was reached. The loading-screen commit (`920023e`) also changed
frame submission, hi-res configuration and the red-light presentation, so its
changes must not be treated as one isolated loading-bar change.

## Official downloads and sources

Downloaded the packages linked by the [official update page](https://wiki.satiator.net/start#software_updates)
into `harness/build/satiator-investigation/`:

| Artifact | Identification | SHA-256 |
| --- | --- | --- |
| `firmware/satiator.fw` | Binary header: `SatiatorFirmware163 2505030818` | `524b5bf7fbb959e0edffd19be429b64d9555697f3cf8ae16b51a5d6c0d81495a` |
| `menu/menu.bin` | Latest official package; accompanying changelog starts at v71 | `860053b144984a008aa272847c4c2dcb14d140a72b6cb5957689e164cd9607e9` |

These files were downloaded for inspection; no device was flashed. The menu
source was checked out at `75bf6e58cb801baeba9cf0159f6c7c73a8e0edb0`, including its
pinned Iapetus and libsatiator submodules. The firmware implementation is not
open source; downloading its binary does not make its CD emulation reproducible
in Mednafen or Ymir. See the [official menu repository](https://github.com/satiator/satiator-menu).

## Findings

1. The official menu's `launch_game()` mounts the selected image, enters CD-ROM
   mode and calls `boot_disc()`. `jhloader.c` resets the CD state, supplies a
   patched region header and delegates the remaining boot to the BIOS.
   Direct ELF launch is a separate path, left in API mode, and is not an
   equivalent test of a failure on the licensed screen.
2. The current Dino ISO has a valid first-read address (`0x06004000`), an
   executable size consistent with `0.BIN`, valid sector packaging and linker
   space for the Master stack. These checks exclude several packaging errors,
   but cannot prove that the physical BIOS completed the transfer.
3. Disassembly of the original `assets/boot/ip_yaul_template.bin` found a CD command hook.
   It sends `0x6000` (set sector size), polls `HIRQ.CMOK` with a counter of
   `0x900`, and reads the result registers even if the counter expires.
   The loop is at IP offsets `0xED8..0xEE0`, with its counter literal at
   `0xEF0`. This is an observable timing assumption in the boot path, not in
   the progress callback. The SBL template contains the same hook; changing
   to SBL alone would not remove this assumption.
4. The [official firmware changelog](https://info.satiator.net/firmware/CHANGELOG)
   describes CD-command latency compatibility fixes in v160 and an earlier
   SD latency issue in v157. This makes the short boot poll worth isolating;
   it does **not** demonstrate that firmware v163 caused this Dino failure.
5. Dino's music progress callback runs after a complete sample load, outside
   `sat_cdfs_read_at()`. It is not nested inside an active CD transfer. Normal
   startup music reads begin after the 40% loading-screen update.
6. The Ymir harness injects the executable after BIOS initialization. It skips
   this IP/BIOS file-loading path and does not emulate Satiator firmware.
   Its successful runtime checks cannot certify physical Satiator boot.

## Official boot change

At the user's request, the CMOK wait is now official in
`assets/boot/ip_yaul_template.bin`. The normal PowerShell/Make build and installed
CMake disc builder use this template by default. The hook exits only after the
CD block reports completion; it no longer reads the response after 2304 polling
iterations expire. Exactly four template bytes changed. The existing image
validator now also requires the CMOK instructions when using the default
template, even if a supplied template and image both contain the old loop.

The change preserves the loading screen. It does not modify firmware/menu files
or the explicitly selected alternative boot templates. See
[the template notes](../assets/boot/README.md) for the instructions and offsets.
An unresponsive CD block can keep this boot wait pending indefinitely. This
policy replaces proceeding with an incomplete command; a bounded error-reporting
path would require additional early boot code.

## Controlled images

Run `./harness/build-dino-satiator-triage.ps1` to build the following files in
`harness/build/dino_satiator/`. The project build script is used for both normal
variants, and the normal example output is restored to the enabled loading
screen. `images.json` records output sizes and hashes.
Matching CUE/BIN files are generated and checked against each ISO for emulator
boot checks. Use the standalone ISOs for the physical comparison, each in its
own game folder; avoid putting a matching ISO and CUE in the same game folder.

| Image | Difference | Purpose |
| --- | --- | --- |
| `with_bar.iso` | Current normal build | Baseline |
| `without_bar.iso` | `DINO_LOADING_SCREEN=0` | Skip loading-bar draws, palette setup and its extra waits; keep font, music, red light and normal frame mode |
| `cmok_wait.iso` | Same as `with_bar.iso` with the official template | Retain the original candidate filename |
| `legacy_poll.iso` | Same executable as `with_bar.iso`; restore the old four boot bytes | Compare against the previous counter-based boot |

The CMOK hook replaces `dt r7; bf poll` with `bra poll; nop`. The earlier CMOK
conditional branch and every other ISO byte are preserved. The preparation
script recognizes both hooks and refuses to patch a different instruction
sequence. `legacy_poll.iso` is a diagnostic image, not the default.

Interpretation on the same console, SD card and installed firmware/menu:

- Only `without_bar.iso` boots: the loading-screen path or the changed executable
  layout is implicated; the IP timing hypothesis remains unconfirmed.
- `with_bar.iso` boots while `legacy_poll.iso` fails: evidence favors the old
  boot-hook timeout. Repeat to establish reliable physical reproduction.
- All variants fail: investigate BIOS/region, menu handoff and executable loading;
  removing the loading screen did not suffice.
- All variants boot: the original failure was not reproduced with these artifacts.

## Validation limits

Executed `build-example.ps1` for both loading-screen variants and
`build-dino-satiator-triage.ps1` for the complete image set. ISO/header/raw-sector
checks passed. Each variant ran for 500 fields in the Ymir harness, using its own
map and Work RAM capture, then passed `harness/tests/dino_runtime.py`:

| Runtime metric | With bar | Without bar |
| --- | --- | --- |
| Completed application frames | 142 | 145 |
| Completed Slave tasks | 284 | 290 |
| Failed tasks / music error / draw overflow | 0 / 0 / 0 | 0 / 0 / 0 |
| Panic state | Clear | Clear |

Launched the normal disc with `run-example.ps1 dino_demo -MednafenCart none`.
A fresh screenshot shows the animated dinosaur, 68000 events and zero music
errors after BIOS boot. The captures and logs are under
`harness/build/satiator-investigation/`. These are emulator checks, not physical
ODE acceptance.

The `cmok_wait.cue` candidate also completed BIOS boot through the project
Mednafen launcher with no cartridge. Its fresh screenshot shows the dinosaur
and active 68000 score events. The initial direct `.iso` attempt was rejected by
Mednafen's Saturn loader; the automatic alternative used the generated and
validated MODE1/2352 CUE/BIN. A simultaneous launch was rejected by Mednafen's
instance lock, then retried after the earlier instance exited. Neither launcher
failure is evidence about the physical Satiator regression.

The physical Satiator is not accessible from this workspace. Final attribution
requires the controlled image comparison there; no confirmed hardware fix is
claimed in this investigation.

## Official-default validation

After promotion, `build-dino-satiator-triage.ps1` rebuilt both normal variants
through `build-example.ps1`, and all four ISO/CUE/BIN sets passed their image
checks. The default IP validator reported `cmok_wait_ok=True`. The normal
`build/examples/dino_demo.iso` matches `with_bar.iso` and `cmok_wait.iso`.
The Dino executable is byte-identical to the previously validated with-bar
executable; comparison against the committed template found exactly four changed
bytes, at offsets `0x0EDE..0x0EE1`.

All six `tests/test_gen_ip_bin.py` tests passed, including rejection of a matching
template/image pair containing the old hook. Launched the rebuilt normal disc
with `run-example.ps1 dino_demo -MednafenCart none`; a fresh screenshot after BIOS
boot shows the animated dinosaur and active music score events. Logs and the
capture are `official-cmok-build.log`, `official-cmok-tool-tests.log` and
`mednafen-official-cmok.png` under `harness/build/satiator-investigation/`.
Physical Satiator confirmation remains pending.
