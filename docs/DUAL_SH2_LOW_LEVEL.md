# LibSaturn dual SH-2 low-level infrastructure

This document describes the implementation in this repository. It is a
hardware foundation, not a scheduler, worker pool, physics backend, or renderer
parallelization layer.

## Hardware contract

The Saturn BIOS starts the Master and keeps the Slave in reset until the
Master issues SMPC `SSHON` (`0x02`). `SSHOFF` is `0x03` and is Master-only.
The Saturn dual-CPU guide documents the Slave vector base at `0x06000400`, its
initial stack at `0x06001000`, and entry installation through vector `0x94`.
The FRT input-capture routes are 16-bit writes to `0x21000000` (Master to
Slave) and `0x21800000` (Slave to Master).

The implementation uses FRT capture polling. The Slave disables its capture
interrupt and polls the ICF flag, so it does not replace the Master's existing
frame-clock use of FRT. The FRT module changes only the two prescaler bits when
the Master frame clock is initialized and preserves the remaining timer state.

Primary sources are the checked-in SMPC manual excerpts under
`docs/sega_saturn_hardware/hard/smpc/` and Sega's dual-CPU technical bulletin.
The latter explicitly states the vector/stack initialization, signal addresses,
non-snooping cache behavior, cache-through alias, and 16-byte line purge:
[Saturn Technical Bulletins, pp. 86-93](https://antime.kapsi.fi/sega/files/Sattechs.pdf).

## Reserved memory layout

`src/core/startup/memory_layout.hpp` and `tools/memory_layout.py` are kept in
lockstep and validated by the image generator.

| Range | Owner |
| --- | --- |
| `0x06000000-0x060003FF` | Master BIOS/application vector table |
| `0x06000400-0x060007FF` | Slave BIOS/application vector table |
| `0x06000800-0x06000FFF` | Slave stack area below `0x06001000` |
| `0x06001000-0x06001FFF` | Reserved boot hand-off/stack guard |
| `0x06002000-0x06003FFF` | Dual-SH2 control block and directional mailboxes |
| `0x06004000+` | Master application image |

The Master stack remains at `0x060FFFFC`. The linker origin is
`0x06004000`, so application sections cannot overlap the reserved prefix. The
build generates a 32 KiB IP.BIN from the selected template and updates only
the first-read address and size; template stack fields are checked against the
authoritative layout rather than silently rewritten.

The Slave entry assembly is not `crt0`: it sets its own SR mask, VBR and stack,
then calls `saturn_slave_init`. It never clears BSS, calls `main`, resets VDP/SCU,
or initializes the Master's runtime.

## Cache and shared-memory rules

SH-2 caches are not hardware-coherent. Shared control metadata is therefore
accessed through the uncached P2 alias (`physical | 0x20000000`), with address
validation restricted to Work RAM. The cache HAL also exposes:

* full purge through CCR CP and the supported 4 KiB or 2 KiB+2 KiB modes;
* 16-byte specific-line invalidation through the `+0x40000000` purge alias;
* validated Work RAM cached/uncached conversions.

The implementation does not promise arbitrary write-back or arbitrary address
conversion. Bulk buffers may remain cached only if the application performs a
documented ownership transition and invalidates the receiving CPU's lines, or
uses the uncached alias for the read. Control structures use uncached access by
default. Compiler barriers order publication around the signal; `volatile` is
not treated as a substitute for cache coherence.

Executable code is linked into the common Work RAM image, so the Slave entry is
visible after the Master publishes the vector pointer through the uncached
alias. A restart rewrites the entry vector and resets the communication
generation before `SSHON`.

## Lifecycle

The lifecycle state is:

`OFFLINE -> STARTING -> READY -> STOPPING -> OFFLINE`

Any bounded startup/shutdown timeout enters `FAULT`. `RESET` is implemented as
a safe stop followed by a fresh start; it is not a forced `SSHOFF` while the
Slave may still be using the external bus. This follows the SMPC manual's
restriction on `SSHOFF`.

Startup does the following on the Master:

1. Serialize and issue `SSHOFF` through the existing SMPC command path.
2. Reset the control block and advance its non-zero generation.
3. Publish the application callback/context and write vector `0x94`.
4. Publish `STARTING`, issue `SSHON`, and wait for the Slave's `READY` state.
5. The Slave sets its local CPU state, publishes `READY`, and sends the reverse
   FRT notification.

Shutdown publishes a generation-tagged request, signals the Slave, waits until
the callback returns and the Slave publishes `OFFLINE`, then issues `SSHOFF`.
If the callback does not return before the deadline, the Master reports
`SAT_ERR_TIMEOUT` and leaves the lifecycle in `FAULT` rather than issuing an
unsafe reset.

## Mailbox protocol

There are two independent one-slot channels:

* Master -> Slave: Master owns publication; Slave owns acknowledgment.
* Slave -> Master: Slave owns publication; Master owns acknowledgment.

Each slot contains `sequence`, `acknowledgment`, `command`, two arguments, and
the lifecycle generation. Sequence zero means empty; publication advances
monotonically and skips zero after wraparound. A sender returns `SAT_ERR_BUSY`
if the previous message is not acknowledged. A receiver copies the payload,
then acknowledges the exact sequence. A generation mismatch is consumed and
discarded, which prevents messages from a previous Slave instance becoming a
false restart acknowledgment.

The FRT signal is only a notification that shared state may have changed. It
does not carry payload and signals may coalesce. The message sequence/ack state
is authoritative. Polling mode acknowledges ICF without touching unrelated FRT
compare/overflow flags.

## Public API

Include `saturn/dual_sh2.h` directly or through `saturn/saturn.h`.

```c
sat_dual_sh2_configure_slave(entry, context);
sat_dual_sh2_start(timeout_ticks);
sat_dual_sh2_state();
sat_dual_sh2_available();
sat_dual_sh2_send(command, argument0, argument1);
sat_dual_sh2_receive(&message);
sat_dual_sh2_wait_response(&message, timeout_ticks);
sat_dual_sh2_stop(timeout_ticks);
sat_dual_sh2_reset(timeout_ticks);
```

The configured callback executes on the Slave. Code inside that callback uses
`sat_dual_sh2_slave_receive`, `sat_dual_sh2_slave_send`,
`sat_dual_sh2_slave_stop_requested`, `sat_dual_sh2_slave_heartbeat`, and
`sat_dual_sh2_slave_signal_master`. The API has no predefined job types and no
automatic dispatch policy.

SMPC ownership is intentionally Master-only for SSHON/SSHOFF. Existing SMPC
peripheral and sound commands continue using the same serialized generic
command path.

## Example and diagnostics

`examples/dual_sh2/` starts the Slave, displays `SLAVE STARTING`, changes to
`SLAVE READY` only after the Slave writes its acknowledgment, and reports the
independent Master loop counter and Slave heartbeat. Every request uses input
`100`; the Slave calculates `200`, returns it, and the Master validates the
command, input, result, and sequence-bearing response.

Build with:

```powershell
.\build-example.ps1 dual_sh2
```

Run with the normal launcher when an emulator is installed:

```powershell
.\run-example.ps1 dual_sh2 -Emulator mednafen -BuildFirst
```

For the repository's reproducible two-CPU validation, use the Ymir harness
with a local BIOS dump:

```powershell
.\harness\run-harness.ps1 dual_sh2 -Bios .\bios\saturn_bios_us.bin `
    -Frames 120 -BootFrames 90 `
    -ProfileInstructions .\harness\build\dual_sh2_instructions.csv `
    -Out .\harness\build\dual_sh2_probe.json
python -m unittest harness.tests.test_dual_sh2 -v
```

## Tests and validation status

Host tests cover address validation, aliases, ownership/sequence publication,
acknowledgment, stale generations, wraparound, lifecycle transitions, and
bounded timeout arithmetic. The normal target build is required to link the
Slave entry and startup objects.

The repository's Ymir harness can report separate Master and Slave instruction
counters with `-ProfileInstructions`; this is the evidence required to claim
that both CPUs executed code. A local BIOS is required. Physical Saturn
validation is not inferred from emulator output and must be recorded separately.

Known restrictions: interrupt-driven FRT reception is represented by the
CPU-local interrupt/FRT HAL but is not selected by the public lifecycle path;
the current example is polling-first. No cross-CPU synchronized timestamp is
provided. In the validated 120-frame Ymir run, the profiler recorded
45,233,302 Master instructions, 37,848,111 Slave instructions, and 115
samples in which both counters advanced; these are emulator instruction
counts, not physical-hardware performance measurements. Request/response
latency and startup latency are deliberately not reported as cross-CPU cycle
measurements because the two local FRT counters are not synchronized.
`SSHOFF` cannot be forced safely after a shutdown timeout. Physical Saturn
validation has not been performed in this environment.
