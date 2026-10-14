# Native Saturn Boot: Execution Evidence

Companion to [NATIVE_SATURN_BOOT_PLAN.md](NATIVE_SATURN_BOOT_PLAN.md). This file records what was actually run and observed. It makes **no** claim of physical Satiator/SAROO success; Gate P5 remains open.

Last updated: 2026-10-14. Emulator evidence only.

## Commands executed and results

| Check | Command | Result |
| --- | --- | --- |
| Boot image tests | `python -m pytest tests/test_boot_image.py` | 8 passed |
| Native IP tests | `python -m pytest tests/test_native_ip.py` | 9 passed |
| Source manifest | `python tests/tools/test_library_source_manifest.py` | OK (151 sources, 3 startup objects) |
| CD deadline host test | `g++ -std=c++17 -I. -Iinclude tests/host/test_cd_deadlines.cpp` (fresh binary) | passed |
| Six packages | `harness/build-optical-packages.ps1` | minimal/full x yaul/sbl/native built; app hashes agree across IP profiles |
| Strict ISO check | `tools/check_iso.py --strict-ip --app ...` per package | sbl and native pass; **yaul fails** with `nonzero_reserved_field, invalid_slave_stack` |
| Injection run (minimal/native) | `harness/run-optical-tester.ps1 -BootMode injection` | PASS, stage 9. **Not a disc boot.** |
| Ymir BIOS boot, no CD-block firmware | `harness/run-optical-tester.ps1 -BootMode bios` | **UNKNOWN**: Master PC parked in BIOS (0x1D3E), disc never reached. Not a pass. |
| Mednafen BIOS boot (minimal/native) | `harness/run-optical-bios.ps1 -BootSeconds 25` | first diagnostic frame shown: stage 9, heartbeat advancing, CD commands disabled |
| Mednafen BIOS boot (full/native, `-RunCdTests`) | same, with key script | five CD steps OK; 32-sector read 65536 B, CRC OK, 0 timeouts, HIRQ/CR snapshot valid |

## What this does and does not show

- The native IP boots the tester through the real BIOS in Mednafen, and the bounded CD diagnostics complete there.
- The strict validator independently reproduces the plan's observation 12: the YAUL template carries a nonzero `0x0E4` and a `0x0EC` Slave stack the validator rejects, while SBL and native do not. This is a *candidate* cause (H6) for SAROO, not a proven one. It is untested on hardware.
- Ymir without user-supplied CD-block firmware cannot perform a disc boot, and the tooling reports that as UNKNOWN rather than passing.
- The Mednafen run needed a 25 s settle time; at 8 s the capture was still the BIOS logo and was inconclusive. `run-optical-bios.ps1` now takes `-BootSeconds` (default 8).
- Mednafen results are captured screenshots reviewed by eye. PC/HIRQ tracing for the BIOS path exists only in the Ymir probe.

## Open items

1. Physical Satiator and SAROO runs of all six packages, including SAROO `/BIN/` versus `/ISO/` (Phase 5, Gate P5).
2. Header-only `0x0E4`/`0x0EC` A/B images on hardware (Phase 4, set H).
3. Ymir BIOS boot with user-provided CD-block firmware to obtain PC/HIRQ stage traces.
4. Provenance and licence decision for the security/area payload used by the native IP (Gate P2).
5. Native profile stays non-default until Gates P1A and P5 are met (Phase 6).
