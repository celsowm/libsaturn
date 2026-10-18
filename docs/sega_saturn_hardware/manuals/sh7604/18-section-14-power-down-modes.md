# Section 14 Power-Down Modes

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=401)

Physical PDF pages 401–410. Text extraction; consult the PDF for figures.

## Bookmarks

- [14.1 Overview](#pdf-page-401)
- [14.1.1 Power-Down Modes](#pdf-page-401)
- [14.1.2 Register](#pdf-page-402)
- [14.2 Description of Register](#pdf-page-403)
- [14.2.1 Standby Control Register (SBYCR)](#pdf-page-403)
- [14.3 Sleep Mode](#pdf-page-405)
- [14.3.1 Transition to Sleep Mode](#pdf-page-405)
- [14.3.2 Canceling Sleep Mode](#pdf-page-405)
- [14.4 Standby Mode](#pdf-page-405)
- [14.4.1 Transition to Standby Mode](#pdf-page-405)
- [14.4.2 Canceling Standby Mode](#pdf-page-406)
- [14.4.3 Standby Mode Cancellation by NMI](#pdf-page-407)
- [14.4.4 Clock Pause Function](#pdf-page-407)
- [14.4.5 Notes on Standby Mode](#pdf-page-409)
- [14.5 Module Standby Function](#pdf-page-409)
- [14.5.1 Transition to Module Standby Function](#pdf-page-409)
- [14.5.2 Clearing the Module Standby Function](#pdf-page-409)

## PDF page 401

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=401)

```text
                          Section 14   Power-Down Modes


14.1      Overview

The SH7604 has a module standby function (which selectively halts operation of some on-chip
peripheral modules), a sleep mode (which halts CPU function), and a standby mode (which halts
all functions).


14.1.1      Power-Down Modes

In addition to the sleep mode and standby mode, the SH7604 also has a third power-down mode,
the module standby function, which halts the DMAC, multiplication unit, division unit, free-
running timer, and SCI on-chip peripheral modules.

Table 14.1 shows the transition conditions for entering the modes from the program execution
state, as well as the CPU and peripheral module states in each mode and the procedures for
canceling each mode.





































                                                                                                               385
```

## PDF page 402

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=402)

```text
Table 14.1    Power-Down Modes

                                                                   State
                                               CPU,                      FRT, SCI,
             Transition                        MULT,      UBC,           DMAC, DIV,                        Canceling
Mode         Condition            Clock        Cache      BSC            INTC, WDT,  Pins                  Procedure
Sleep        SLEEP                 Runs      Halted    Runs           Runs             Runs             1.  Interrupt
mode         instruction                                                                                   2.  DMA
             executed with                                                                                     address
             SBY bit set to 0                                                                                  error
             in SBYCR                                                                                      3.  Power-

                                                                                                               on reset
                                                                                                           4.  Manual
                                                                                                               reset
Standby      SLEEP                 Halted    Halted    Held            Halted           Held or high       1.  NMI
mode         instruction                                                                  impedance            interrupt
             executed with                                                                                 2.  Power-
             SBY bit set to 1                                                                                  on reset
             in SBYCR                                                                                      3.  Manual

                                                                                                               reset
Module       MSTP bit for          Runs      Run          Runs           When             FRT and          Clear MSTP
standby      relevant module                   (MULT                     MSTP bit         SCI pins         bit to 0
function     is set to 1                       is held)                  is 1, the        are
                                                                         supply of        initialized,
                                                                         the clock to     and others
                                                                         the relevant     operate.
                                                                         module is
                                                                         halted.


14.1.2      Register

Table 14.2 shows the register configuration.

Table 14.2    Register Configuration

Name                                                Abbreviation       R/W     Initial Value           Address
Standby control register                    SBYCR                R/W     H'60                       H'FFFFFE91










386
```

## PDF page 403

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=403)

```text
14.2      Description of Register


14.2.1      Standby Control Register (SBYCR)

The standby control register (SBYCR) is an 8-bit read/write register that sets the power-down
mode. SBYCR is initialized to H'00 by a reset.

                    Bit:         7              6              5              4              3              2              1              0
            Bit name:      SBY          HIZ           —        MSTP4    MSTP3    MSTP2    MSTP1    MSTP0
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:      R/W         R/W          —           R/W         R/W         R/W         R/W         R/W


•    Bit 7—Standby (SBY): Specifies transition to standby mode. The SBY bit cannot be set to 1
    while the watchdog timer is running (when the TME bit in the WDt’s WTCSR register is 1).
    To enter the standby mode, halt the WDT (set the TME bit in WTCSR to 0) and set the SBY
    bit.

Bit 7:  SBY            Description
0                            Executing a SLEEP instruction puts the chip into sleep mode           (Initial value)
1                            Executing a SLEEP instruction puts the chip into standby mode


•    Bit 6—Port High Impedance (HIZ): Selects whether output pins are set to high impedance or
    retain the output state in standby mode. When HIZ = 0 (initial state), the specified pin retains
    its output state. When HIZ = 1, the pin goes to the high-impedance state. See Appendix A.1,
    Pin States during Resets, Power-Down States and Bus Release State, for which pins are
    controlled.

Bit 6:  HIZ              Description
0                            Pin state retained in standby mode                                                    (Initial value)
1                            Pin goes to high impedance in standby mode


•    Bit 5—Reserved: This bit always reads 0. The write value should always be 0.

•    Bit 4: Module stop 4 (MSTP4): Specifies halting the clock supply to the DMAC. When
    MSTP4 bit is set to 1, the supply of the clock to the DMAC is halted. When the clock halts, the
    DMAC retains its pre-halt state. When MSTP4 is cleared to 0 and the DMAC begins running
    again, its starts operating from its pre-halt state. Set this bit while the DMAC is halted; this bit
    cannot be set while the DMAC is operating (transferring data).





                                                                                                                              387
```

## PDF page 404

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=404)

```text
Bit 4:  MSTP4       Description
0                            DMAC running                                                                                    (Initial value)
1                            Clock supply to DMAC halted


•    Bit 3—Module Stop 3 (MSTP3): Specifies halting the clock supply to the multiplication unit
    (MULT). When the MSTP3 bit is set to 1, the supply of the clock to MULT is halted. When
    the clock halts, MULT retains its pre-halt state. This bit should be set when the MULT is
    halted.

Bit 3:  MSTP3       Description
0                            MULT running                                                                                     (Initial value)
1                            Clock supply to MULT halted


•    Bit 2—Module Stop 2 (MSTP2): Specifies halting the clock supply to the division unit
    (DIVU). When the MSTP2 bit is set to 1, the supply of the clock to DIVU is halted. When the
    clock halts, the DIVU registers retain their pre-halt state. This bit should be set when the DIVU
    is halted.

Bit 2:  MSTP2      Description
0                           DIVU running                                                                                        (Initial value)
1                           Clock supply to DIVU halted


•    Bit 1—Module Stop 1 (MSTP1): Specifies halting the clock supply to the 16-bit free-running
    timer (FRT). When the MSTP1 bit is set to 1, the supply of the clock to the FRT is halted.
    When the clock halts, all FRT registers are initialized except the FRT interrupt vector register
    in INTC, which holds its previous value. When MSTP1 is cleared to 0 and the FRT begins
    running again, its starts operating from its initial state.

Bit 1:  MSTP1      Description
0                           FRT running                                                                                         (Initial value)
1                           Clock supply to FRT halted


•    Bit 0—Module Stop 0 (MSTP0): Specifies halting the clock supply to the serial
    communication interface (SCI). When the MSTP0 bit is set to 1, the supply of the clock to the
    SCI is halted. When the clock halts, all SCI registers are initialized except the SCI interrupt
    vector register in INTC, which holds its previous value. When MSTP0 is cleared to 0 and the
    SCI begins running again, its starts operating from its initial state.





388
```

## PDF page 405

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=405)

```text
Bit 0:  MSTP0      Description
0                           SCI running                                                                                          (Initial value)
1                           Clock supply to SCI halted


14.3      Sleep Mode


14.3.1      Transition to Sleep Mode

Executing the SLEEP instruction when the SBY bit in SBYCR is 0 causes a transition from the
program execution state to sleep mode. Although the CPU halts immediately after executing the
SLEEP instruction, the contents of its internal registers remain unchanged. The on-chip peripheral
modules continue to run in sleep mode.


14.3.2      Canceling Sleep Mode

Sleep mode is canceled by an interrupt, DMA address error, power-on reset, or manual reset.

Cancellation by an Interrupt: When an interrupt occurs, sleep mode is canceled and interrupt
exception handling is executed. Sleep mode is not canceled if the interrupt cannot be accepted
because its priority level is equal to or less than the mask level set in the CPU’s status register
(SR) or if an interrupt by an on-chip peripheral module is disabled at the peripheral module.

Cancellation by a DMA Address Error: If a DMA address error occurs, sleep mode is canceled
and DMA address error exception handling is executed.

Cancellation by a Power-On Reset: A power-on reset cancels sleep mode.

Cancellation by a Manual Reset: A manual reset cancels sleep mode.

14.4      Standby Mode


14.4.1      Transition to Standby Mode

To enter standby mode, set the SBY bit to 1 in SBYCR, then execute the SLEEP instruction. The
chip switches from the program execution state to standby mode. The NMI interrupt cannot be
accepted when the SLEEP instruction is executed, or for the following five cycles. In standby
mode, power consumption is greatly reduced by halting not only the CPU, but the clock and on-
chip peripheral modules as well. CPU register contents are held, and some on-chip peripheral
modules are initialized.




                                                                                                                      389
```

## PDF page 406

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=406)

```text
Table 14.3    Register States in Standby Mode

                                                                      Registers that Retain              Registers with
Module                             Registers Initialized              Data                               Undefined Contents
Interrupt controller               —                                     All registers                       —
(INTC)
User break controller              —                                     All registers                       —
(UBC)
Bus state controller               —                                     All registers                       —
(BSC)
DMAC                               DMA channel control                All registers except DMA           —
                                   register 0                         channel control register
                                   DMA channel control                0, DMA channel control
                                   register 1                         register 1, and DMA
                                                                      operation register
                                   DMA operation register
DIVU                                 —                                     —                                      All registers
Watchdog timer (WDT)     Bits 7–5 of the timer                        Bits 2–0 of the timer              —
                                   control/status register            control/status register
                                   Reset control/status               Timer counter
                                   register
16-bit free-running timer          All registers                      —                                      —
(FRT)
Serial communication               All registers                      —                                      —
interface (SCI)
Others                               —                                     Standby control register      —
                                                                      Frequency modification
                                                                      register


14.4.2      Canceling Standby Mode

Standby mode is canceled by an NMI interrupt, a power-on reset, or a manual reset.

Cancellation by an NMI: When a rising edge or falling edge is detected in the NMI signal, after
the elapse of the time set in the WDT timer control/status register, clocks are supplied to the entire
chip, standby mode is canceled, and NMI exception handling begins.

Cancellation by a Power-On Reset: A power-on reset cancels standby mode.

Cancellation by a Manual Reset: A manual reset cancels standby mode.





390
```

## PDF page 407

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=407)

```text
14.4.3      Standby Mode Cancellation by NMI

The following example describes moving to the standby mode upon the fall of the NMI signal and
clearing the standby when the NMI signal rises. Figure 14.1 shows the timing.

When the NMI pin level changes from high to low after the NMI edge select bit (NMIE) of the
interrupt control register (ICR) has been set to 0 (detect falling edge), an NMI interrupt is
accepted. When the NMIE bit is set to 1 (detect rising edge) by the NMI exception service routine,
the standby bit (SBY) of the standby control register (SBYCR) is set to 1 and a SLEEP instruction
is executed, the standby mode is entered. The standby mode is cleared the next time the NMI pin
level changes from low level to high level.



 Oscillator


     CKIO
  (output)


       NMI



     NMIE



      SBY                                                                         Oscillation
                                                                                 settling time
                         NMI
                      exception                                                           WDT
                      handling                         Standby mode                      set time
                                   Exception                                  Start of              NMI exception
                               service routine,                             oscillation                handling
                                   SBY = 1,
                              SLEEP instruction

                           Figure 14.1   Standby Mode Cancellation by NMI


14.4.4      Clock Pause Function

When the clock is input from the CKIO pin, the clock frequency can be modified or the clock
stopped. The SH7604 has a CKPREQ/CKM pin for this purpose. The clock pause function is used
as described below. Note that clock pauses are not accepted while the watchdog timer (WDT) is
operating (i.e. when the timer enable bit (TME) in the WDT’s timer control/status register
(WTCSR) is 1).



                                                                                                                  391
```

## PDF page 408

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=408)

```text
1.   Set the TME bit in the watchdog timer’s WTCSR register to 0.
2.   Set the overflow time in bits CKS2 to CKS0 bits in the watchdog timer’s WTCSR register
    (overflow time should be calculated using the clock frequency after modification).
3.   After the SLEEP instruction is executed and standby mode is entered, apply a low level from
    the CKPREQ/CKM pin.
4.   When the chip is internally ready to modify the operating clock, a low level is output from the
    CKPACK pin.
5.   After the CKPACK pin goes low, the clocks are stopped and the frequency is modified. The
    internal chip state is the same as in standby mode.
6.   When the clock pause state (standby) is canceled, the WDT starts to count up at the falling
    edge or rising edge of the NMI pin (when the NMIE bit of INTC is set).
7.   When a frequency is modified, the CKPACK pin goes high after the time set by the WDT, and
    the clock pause function gives external notification that the chip can again be operated
    (standby mode is canceled).
8.   When a clock is halted, the clock is applied again to the CKIO pin and NMI input is generated.
    After the time set by the WDT, the CKPACK pin goes high, and the clock pause function gives
    external notification that the chip can again be operated (standby mode is canceled).

The standby state, all internal functions and all pin states during clock pause are equivalent to
those of the normal standby mode. Figure 14.2 shows the timing chart for the clock pause
function.


                                                           Clock frequency modification



               CKIO input

    CKPREQ/CKM input

                                                    Clock pause request cancellation
         CKPACK output


                 NMI input

                                                                                                           NMI
                                  Waiting for                                             WDT           exception
                                  clock pause                                         setting time      handling

                                                                    Standby time


                                Figure 14.2   Clock Pause Function Timing



392
```

## PDF page 409

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=409)

```text
14.4.5      Notes on Standby Mode

1.   When the SH7604 enters standby mode during use of the cache, disable the cache before
    making the mode transition. Initialize the cache beforehand when the cache is used after
    returning to standby mode. The contents of the on-chip RAM are not retained in standby mode
    when cache is used as on-chip RAM.
2.   If an on-chip peripheral register is written in the 10 clock cycles before the SH7604 transits to
    standby mode, read the register before executing the SLEEP instruction.
3.   When using clock mode 0, 1, or 2, the CKIO pin is the clock output pin. Note the following
    when standby mode is used in these clock modes. When standby mode is canceled by NMI, an
    unstable clock is output from the CKIO pin during the oscillation settling time after NMI input.
    This also applies to clock output in the case of cancellation by a power-on reset or manual
    reset. Power-on reset and manual reset input should be continued for a period at least equal to
    for the oscillation settling time.

14.5      Module Standby Function


14.5.1      Transition to Module Standby Function

By setting one of standby control register bits MSTP4–MSTP0 to 1, the supply of the clock to the
corresponding on-chip peripheral module can be halted. This function can be used to reduce the
power consumption in sleep mode. Do not perform read/write operations for a module in module
standby mode.

The external pins and registers of the DMAC, MULT, and DIVU on-chip peripheral modules
retain their states prior to halting. The external pins of the FRT and SCI are reset and all their
registers are initialized.

Do not switch on-chip peripheral modules to module standby mode while they are running.


14.5.2      Clearing the Module Standby Function

Clear the module standby function by clearing the MSTP4–MSTP0 bits, or by a power-on reset or
manual reset.

To effect a module stop, halt the relevant module or disable interrupts.










                                                                                                                    393
```

## PDF page 410

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=410)

```text
394
```
