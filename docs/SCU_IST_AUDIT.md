# SCU interrupt status register audit

The SCU manual, section 3.5, prohibits writing the interrupt status register
at `0x25FE00A4`: an interrupt arriving during a write may fail to be recorded.
See `sega_saturn_hardware/hard/scu_/hon/p03_05.md`. This audit identifies
existing writes; it does not establish that an interrupt has been lost on a
particular Saturn or that removing the writes alone is safe.

Sega's [Master CD-ROM Release Form ST-242-R2-010496](https://antime.kapsi.fi/sega/files/ST-242-R2-010496.pdf)
also lists keeping SMPC interrupts enabled for INTBACK and avoiding writes to
IST. The same checklist lists avoiding access to the DMA status and stop
registers, both used by the current DMA implementation. This is additional
evidence that replacing the DMA acknowledgement cannot be reduced to merely
removing an IST store; the checklist does not by itself describe a safe
replacement protocol or establish the behavior of every emulator.

## Accesses

| Location | Current purpose | Consequence for a fix |
| --- | --- | --- |
| `src/hal/scu/dma.cpp` | No longer writes IST. A shared DMA-illegal event marks outstanding transfers as suspect; the dispatcher records it before calling any public handler. If an observed pending error cannot be acknowledged, new transfers are refused. DMA-end status remains pending until delivered. | Ymir injection covers masked and public-handler cases between jobs and after `start` but before `wait`; host tests model unavailable IRQs and multiple jobs. The injection does not prove the hardware DMA engine was still busy or cover every interrupt ordering. DMA status and stop accesses also remain. |
| `src/hal/smpc/smpc.cpp`, `clear_smpc_ist` | Clears IST before CONTINUE. The unnecessary clear before initial INTBACK was removed. | An experimental IRQ counter path was removed after a modeled old in-flight interrupt caused it to accept an ignored CONTINUE. The remaining production path still writes IST and needs a safe replacement. |
| `src/storage/cd/exec_hal.cpp`, `stop_machine` | Previously cleared all IST bits during `sat_exec` handoff. The write has been removed; the CPU and SCU masks stay closed and BIOS vectors are restored while handing off. | The next image owns its interrupt setup. The demo alternates the normal and keep-display handoffs. Pending-source edge cases still need targeted testing. |

The initial searches for literal register addresses and named aliases in source,
headers, examples and tests found these three write sites. A direct address
search cannot rule out a dynamically computed or differently aliased access.
The Tekken source has no matching direct access; it uses SMPC pad reads and
`sat_exec`, including the keep-display variant. DMA may also be used through
library paths; this audit did not trace every runtime transfer.

## Follow-up and validation

1. Define ownership and acknowledgement of each interrupt using the SCU and
   SMPC manuals and the existing IRQ dispatch. In particular, determine how to
   obtain a fresh SMPC event when the IRQ subsystem is unavailable or an
   interrupt is masked; do not assume polling a latched bit provides this.
2. Define how shared DMA-illegal events are recorded and attributed across
   levels, and how timeout and recovery work. Preserve the existing public
   results and consider whether an uncertain attribution must be reported.
3. Evaluate pending interrupt handling across the executable handoff and new
   startup, without writing IST. Keep BIOS vector and interrupt-mask ordering
   explicit.
4. Exercise stale events, events from unrelated sources during each operation,
   delayed and missing SMPC chunks, DMA errors and simultaneous levels, and
   both handoff variants. Run host tests and emulator integration tests; the
   manual's hardware race is not proven absent by an emulator-only run.

The current IRQ dispatch counts delivered sources and restores the IMS shadow,
but its availability and interaction with masked sources, startup, timeouts
and BIOS handoff must be resolved before using those counters as a replacement.
The IRQ HAL now has internal retain/release operations, separate from the
application's handler, so a library consumer can keep a source unmasked without
stealing that handler. Host tests cover shared ownership, nesting, shutdown and
an already-masked CPU; the existing IRQ demo passed four Ymir checks. The new
operations are used by DMA when acknowledging an observed illegal event;
SMPC does not yet use them. Retaining a source alone does not prove that an
already-pending or in-flight interrupt belongs to a new command.

The first CONTINUE now also checks the SMPC SR PDL bit: if the result still
identifies the first data block, the read fails instead of replaying that
block. A host scenario injects an old event after the current IST clear and
an ignored CONTINUE; disabling the PDL check makes the test fail. PDL is zero
for all later blocks, so this does not solve freshness after the second block
or replace the prohibited IST write.

SF alone has not been established as a reliable per-CONTINUE edge. The
SMPC manual's general command section describes SF as busy while a command
runs, but its [English ST-169 INTBACK description](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf)
also states that SF is set busy when an interrupt is requested. It does not
specify a minimum observable SF pulse for every CONTINUE. Ymir currently sets
SF on a CONTINUE and clears it on INTBACK completion, but that emulator
behavior alone does not establish a hardware guarantee. Requiring a sampled
SF rise could reject a fast valid response; accepting SF clear alone can replay
an ignored request. Do not use either as an established replacement without
additional evidence.
The host protocol model now includes a response whose SF busy interval ends
before the first poll. Current code accepts the modeled three-block report;
a temporary mutation requiring the SF-high observation fails that case. This
is an adversarial model, not a measurement of a Saturn's minimum response time.

An independent DMA completion defect was fixed while investigating error
reporting: `busy()` previously finalized an illegal transfer and discarded
the error, so a later `wait()` returned success. The result now stays pending
until `wait()` or a subsequent start reports it. Indirect-list preparation
also checks the channel before rewriting its table. On an observed shared
DMA-illegal status, all outstanding levels now receive an error because the
hardware does not identify the responsible level. A host test exercises the
real completion code with simulated registers; the Ymir DMA demo passed all
13 acceptance checks. The dispatcher now records illegal DMA interrupts before
calling the application's handler. `scu_dma_fault_probe` together with Ymir's
`--inject-dma-illegal` exercised the source once while masked and once with a
public handler. Both runs recorded one library error, continued valid DMA jobs,
and recorded zero IST writes after program entry; the handler was called once
in its scenario. Two more runs injected at `dma_fault_after_start`, after a
valid start and before wait, with and without the public handler. Each reported
one failed job, recovered for later jobs and performed no IST writes. The HAL
job was pending; Ymir reported level 0 inactive at the injection point even
with a 16 KiB transfer, so these runs do not test an actively moving DMA.
The no-IRQ error state and later acknowledgement are covered by the host model.

The Tekken `build/vram-banks` fight executable was rebuilt against the
current SDK prefix. Its map placed `__bss_end` at `0x060E7A74`, leaving
83,340 bytes below the end of Work RAM-H after the 16 KiB master-stack
reserve. A Ymir run injected that rebuilt fight executable with the existing
asset disc for 1,800 fields; it reached 1,049 simulation ticks, with zero
`faces_rejected`, drawing timeouts, first error and panic record. Executable
SHA-256 was `a02bd40a4407b30d6ab3693e6a1b89a374752eaf5ecf5df5b39a7e3d22a9a3c5`.
This is a single fight smoke test; it does not validate disc boot, menu handoff,
multitap input or real hardware.
An additional Ymir run injected the event while the guest had raised the
SH-2 interrupt mask to 15 and the SCU source was masked. The first DMA start
returned `SAT_ERR_IO`; after restoring the CPU mask, a retry acknowledged
the error and succeeded. No IST writes were observed. IRQ infrastructure was
live in this run, so it is not a test of initialization without IRQ delivery.

Two additional runs changed the emulated level-0 source address immediately
before the GO register write, making Ymir's own DMA engine generate an illegal
same-bus transfer. With the source masked and with a public handler, the HAL
reported one failed job and one illegal event, subsequently completed valid
jobs and wrote nothing to IST. This exercises Ymir's causal error path, while
remaining an emulator-only fault and not establishing the ordering on hardware.

An instruction-level injection after the final IST observation but before
`finish` cleared the pending job recorded the error globally while that job
returned success, both with a masked source and a public handler. A trial
that kept a `closing` flag and performed another observation caught that
particular point; injecting immediately before clearing `closing` reproduced
the same success result. The trial was removed. In both cases Ymir reported
the hardware DMA inactive, so whether the synthetic event should be attributed
to that already-idle transfer depends on the completion contract; it does not
demonstrate a real hardware DMA error arriving after idle. The current code
does not establish a general ordering guarantee for this transition.

In the Ymir source used here, both modeled illegal-transfer paths set the
channel inactive and call `TriggerDMAIllegal()` within the same emulator
operation before recalculating the DMA channel. The late instruction-level
injection above deliberately bypasses that causal sequence. This explains why
it is a useful adversarial ordering test but is not evidence that Ymir's own
illegal-transfer model emits a late event. The available SCU manual text was
not found to specify the corresponding physical-hardware ordering; no hardware
guarantee is inferred from Ymir's implementation.

To reproduce the two Ymir captures, build `scu_dma_fault_probe` and `probe`,
then run the probe with the example's `.cue` and `.app.bin`, `--frames 240`,
`--watch-write 0x25FE00A4:4:90`, `--dump-wram-high` and `--out`. Use
`--inject-dma-illegal 100` for the masked source and `190` for the public
handler. To inject after start, add `--inject-dma-illegal-pc` with the address
of `_dma_fault_after_start` from the current executable's symbol table.
For the emulated invalid-source case, use `--inject-dma-invalid-source-pc`
at the level-0 GO write instruction in the current build's disassembly instead.
For the temporarily masked CPU case, inject at `_dma_fault_masked_start` with
`--inject-dma-illegal-pc` and a frame threshold before the guest's frame 80.
Supply the dump/JSON pairs and the example map through the
`LIBSATURN_DMA_FAULT_*` variables named in
`harness/tests/test_scu_dma_fault_probe.py`. This injects an SCU event in Ymir;
the guest deliberately does not attempt an invalid DMA on hardware.
After removing normal-completion IST writes, the DMA demo's 13 Ymir checks
passed, including its enabled DMA-end handler. A watch over `0x25FE00A4`
recorded nine writes in that run, all at BIOS dispatcher PC `0x060007D8` and
none in the library DMA path; this run did not inject an illegal transfer.
An end event left masked can now remain pending until a handler is enabled.
On timeout, the existing forced-stop command affects all levels in Ymir. The
HAL now marks every outstanding transfer as timed out and retains pending
buffers until its level is observed idle; a host register test covers two
levels and a stop that does not immediately complete. This limits a separate
failure mode but does not resolve the checklist's warning about DSTP access.

The SMPC manual requires each CONTINUE request to toggle IREG0 bit 7. The
peripheral reader now toggles it instead of always writing `0x80`. The host
logic test covers three successive values. Ymir input-device acceptance passed
11 checks for analog, digital and mouse cases; these do not prove a report with
more than two chunks or execution on real hardware.
BREAK now preserves the current CONT bit. Previously a BREAK after an odd
number of CONTINUE requests changed CONT and set BR simultaneously, which the
SMPC manual leaves undefined. The host protocol model rejects that conflicting
combination; restoring the old BREAK value deliberately makes the test fail.
The Ymir harness can now inject two fixed six-device reports, 86 bytes total,
and models CONTINUE as an IREG0 bit-7 transition. The input demo completed 417
polls with no errors, recognized all 12 synthetic analog devices and passed
`test_input_multitap_probe.py`; the normal analog/mouse and digital-pad tests
also passed. This is a synthetic report stream, not an electrical 6Player
model or real hardware. It exercises the existing IST-writing implementation
and supplies a reproducible integration test for its eventual replacement.
After removing the initial IST clear, the same 11 input-device acceptance
checks passed again. A Ymir register watch also observed IST writes from the
BIOS dispatcher and from `dma::finish`; this audit can correct the library's
own writes but does not control BIOS implementation behavior.

`tests/host/test_smpc_chunk_protocol.cpp` runs the actual SMPC HAL against a
controlled register model. It exercises a three-chunk report from two six-pad
adapters, delayed completion, a stale SMPC event followed by ignored CONTINUE,
a missing event and recovery. Suppressing the current IST clear deliberately
caused this test to fail: it issued extra CONTINUE requests after the ignored
request. The fake register model cannot establish electrical timing or which
events the real SCU delivers, but provides a regression test for a replacement
protocol.

An experimental IRQ-active path passed the model with a latched old interrupt
drained, but a modified model then delivered a previously forwarded old event
after the new command began and delayed the new command's event. The path
treated an ignored CONTINUE as completed. It was removed rather than leave
that unverified behavior in the library. The scenario is a controlled model,
not proof of a particular hardware timing; it establishes the protocol must
handle this ordering or show it cannot occur.

A second temporary mutation models delivery of the SMPC interrupt consuming
the IST indication before the polling loop sees it. With that ordering, the
unmodified HAL fails the three-chunk read even though the modeled device
completed it. The model deliberately assumes this ordering; Ymir's SCU code
also clears an internal status bit as it raises the interrupt. This demonstrates
why polling IST and allowing a public SMPC handler cannot independently own
the same completion signal. The mutation was not added to production or the
normal passing test suite.

An initial Ymir run of `exec_demo` completed four sequential handoffs, alternating
`sat_exec` and `sat_exec_keep_display`, with the Slave running and VBlank
interrupts active. A masked SMPC status bit was observed before each handoff;
all six `test_exec_demo.py` checks passed. This does not
cover every pending interrupt timing or real hardware. The run injected the
program after BIOS initialization. A separate BIOS-boot attempt did not reach
the application entry point after 600 fields, so it supplies no evidence about
the handoff. The native host suite (`gmake test`) also passed.
