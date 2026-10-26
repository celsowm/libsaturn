# Local FRT waiting for the parallel executor

The executor previously polled the external shared control block continuously,
including a heartbeat write on every iteration. It now waits on the Slave's
on-chip FRT input-capture flag. A 256-tick maintenance interval at the existing
/128 prescaler checks shutdown, updates the heartbeat and rescans the mailbox
sequence, recovering a lost/coalesced notification in about 1.2 ms. Entry always
checks for an already-published request. The mailbox's generation and sequence
protocol, task ownership, completion publication and Master join are unchanged.
No new public API or interrupt handler is introduced.

The implementation follows the Sega Saturn Dual CPU User's Guide section 5.2,
which describes polling the FRT input capture flag with capture interrupts
disabled. `saturn_slave_init` already configures that mode. Host tests cover
spurious flags, publication during flag clear, a missing notification,
consecutive requests, shutdown and 16-bit FRT rollover. Existing protocol and
executor tests remain in place; the 32-bit mapped-WRAM completion test explicitly
skips on native macOS and remains runnable on Linux/Windows.

## Matched original-Paul controls

Both SDKs were freshly compiled with matching CMake/toolchain options; the
control source is SDK `14559f9`. Both port builds use the same original mesh
(556 vertices/773 faces), two actors, 128-entry local pose cache, original
combat, parallel geometry/skinning, stock RAM and complete-list presentation.
The port's `two-paul-release` workload guard accepts both builds.

Cache-on Ymir controls over 5,400 requested fields both complete 4,125 ticks
and 1,031 submissions, with 149 late swaps and no draw errors/timeouts. Thus
this workload shows **no Ymir cadence increase**. The pinned immediate-result
DIVU and memory model remain emulator limitations, not hardware timing.

Independent 79-second Mednafen CD movies contain these visible FRAME values:

| Movie second | Original idle loop | Local FRT wait |
| --- | ---: | ---: |
| 18 | 42 | 45 |
| 78 | 473 | 494 |

Over that 60-second movie interval the counters advance 431 versus 449:
7.183 versus 7.483 submissions/s, **4.18% more**. A preceding original-build
movie reproduces the same endpoint counts. These are visible submission-counter
observations; they are not unique-picture matching, physical Saturn timing or
the 30-image/60-logic milestone. The complete 24-case original combat suite
passes with the FRT wait, including replay, victory, match reset and timeouts.
The supported host/tool suite and all five installed-package CTests pass.

Private evidence is retained in the sibling port's ignored
`assets/imported/structural-v2-{clean-control,clean-frt}-*` and
`structural-v2-frt-combat-suite/`. The preserved installed control/candidate
archives are hashed in each release observation. Retail bytes stay private.
