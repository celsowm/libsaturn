# Read the executing SH-2's cache state

`<saturn/sh2.h>` exposes `sat_sh2_cache_control()`, also available from the
umbrella header. It reads the current CPU's raw CCR byte, without enabling,
disabling or purging anything. It does not depend on `sat_init`.

```c
uint8_t ccr = sat_sh2_cache_control();
uint8_t cache_enabled = (ccr & SAT_SH2_CCR_CACHE_ENABLE) != 0;
```

The primary [SH7604 cache manual, section 8.2](sega_saturn_hardware/manuals/sh7604/12-section-8-cache.md)
defines CCR at `0xFFFFFE92`, CE in bit 0, and reset state zero. Reading from the
Master reports the Master; invoking it inside a verified Slave job reports
the Slave. A headless harness's cache-emulation option is independent of the
guest's actual control bits.

The implementation reuses the HAL's CCR read. Target disassembly consists of
loading the on-chip register address, reading one byte, returning and unsigned
extension. It performs no CCR write and does not change SR. The installed
package consumer links the API, and the standalone C/C++ public-header smoke
and all five package checks pass. The portable host cannot read this hardware
register; there is no fabricated host cache-state result.

## Initial native-CD control

The sibling Tekken port's `T3_CPU_DIAGNOSTICS=ON` captures the Master at main
entry and after setup, and uses `sat_parallel_for` for a one-element-per-CPU
snapshot. It accepts the Slave result only when an empty healthy runtime's
submitted/completed/Slave-task counters prove that this job ran there; a
Master fallback is shown as unknown, never as the Slave's state.

A private actual Mednafen CD boot visibly reports MCCR=1 and SCCR=1 at second
34. The independently inspected Ymir dump reports entry Master=1, post-setup
Master=1, Slave=1 and verified-Slave flag=1. Thus disabled cache does not
explain the observed native/emulator cadence gap in this control. This
diagnostic does not certify memory wait states, divider timing or physical
hardware performance.

Private evidence: the sibling port's
`assets/imported/boot-cache-inspect-clear-mednafen/` and
`assets/imported/boot-cache-inspect-ymir/`. The former is CD boot; the latter is
injection. The SDK API is format/application independent and can be called
from any correctly dispatched task.
