# Section 13 Serial Communication Interface

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=349)

Physical PDF pages 349–400. Text extraction; consult the PDF for figures.

## Bookmarks

- [13.1 Overview](#pdf-page-349)
- [13.1.1 Features](#pdf-page-349)
- [13.1.2 Block Diagram](#pdf-page-350)
- [13.1.3 Pin Configuration](#pdf-page-350)
- [13.1.4 Register Configuration](#pdf-page-351)
- [13.2 Register Descriptions](#pdf-page-351)
- [13.2.1 Receive Shift Register (RSR)](#pdf-page-351)
- [13.2.2 Receive Data Register (RDR)](#pdf-page-351)
- [13.2.3 Transmit Shift Register (TSR)](#pdf-page-352)
- [13.2.4 Transmit Data Register (TDR)](#pdf-page-352)
- [13.2.5 Serial Mode Register (SMR)](#pdf-page-352)
- [13.2.6 Serial Control Register (SCR)](#pdf-page-355)
- [13.2.7 Serial Status Register (SSR)](#pdf-page-358)
- [13.2.8 Bit Rate Register (BRR)](#pdf-page-362)
- [13.3 Operation](#pdf-page-368)
- [13.3.1 Overview](#pdf-page-368)
- [13.3.2 Operation in Asynchronous Mode](#pdf-page-370)
- [13.3.3 Multiprocessor Communication](#pdf-page-380)
- [13.3.4 Clocked Synchronous Operation](#pdf-page-387)
- [13.4 SCI Interrupt Sources and the DMAC](#pdf-page-396)
- [13.5 Usage Notes](#pdf-page-396)

## PDF page 349

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=349)

```text
                Section 13   Serial Communication Interface


13.1      Overview

The SH7604 has a serial communication interface (SCI) that supports both asynchronous and
clocked synchronous serial communication. It also has a multiprocessor communication function
for serial communication among two or more processors.


13.1.1      Features

Selection of asynchronous or clock synchronous as the serial communication mode

•    Asynchronous mode:
    ¾    Serial data communication is synchronized by the start-stop method in character units. The
        SCI can communicate with a universal asynchronous receiver/transmitter (UART), an
        asynchronous communication interface adapter (ACIA), or any other chip that employs
        standard asynchronous serial communication. It can also communicate with two or more
        other processors using the multiprocessor communication function. There are twelve
        selectable serial data communication formats.
    ¾    Data length: seven or eight bits
    ¾    Stop bit length: one or two bits
    ¾    Parity: even, odd, or none
    ¾    Multiprocessor bit: one or none
    ¾    Receive error detection: parity, overrun, and framing errors
•    Clocked synchronous mode:
    ¾    Serial data communication is synchronized with a clock signal. The SCI can communicate
        with other chips having a clocked synchronous communication function. There is one serial
        data communication format.
    ¾    Data length: eight bits
    ¾    Receive error detection: overrun errors
•    Full duplex communication. The transmitting and receiving sections are independent, so the
    SCI can transmit and receive simultaneously. Both sections use double buffering, so
    continuous data transfer is possible in both the transmit and receive directions.
•    Built-in baud rate generator with selectable bit rates
•    Internal or external transmit/receive clock source. Baud rate generator (internal) or SCK pin
    (external)
•    Four types of interrupts. Transmit-data-empty, transmit-end, receive-data-full, and receive-
    error interrupts are requested independently. The transmit-data-empty and receive-data-full
    interrupts can start the direct memory access controller (DMAC) to transfer data.


                                                                                                                    333
```

## PDF page 350

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=350)

```text
13.1.2      Block Diagram

Figure 13.1 shows a block diagram of the SCI.







                                            Module data bus                                                       Internaldata bus
                                                                                                  Bus interface

                      RDR               TDR                  SSR                 BRR
                                                             SCR                                           φ/4
    RxD               RSR                                          SMRTSR   Baud rate                      φ/16
                                                       Transmit/            generator                      φ/64
                                                        receive
                                                         control                                           φ/256
     TxD                           Parity generation                   Clock
                                     Parity check                 External clock
    SCK                                                                                                    TEI
                                                                                                           TXI
                                                                                                           RXI
                                                                             SCI                           ERI


  RSR: Receive shift register                                 SMR: Serial mode register
  RDR: Receive data register                                 SCR: Serial control register
  TSR: Transmit shift register                                 SSR: Serial status register
  TDR: Transmit data register                                BRR: Bit rate register

                                       Figure 13.1   SCI Block Diagram


13.1.3      Pin Configuration

Table 13.1 summarizes the SCI pins.

Table 13.1    SCI Pins

Pin Name                    Abbreviation   Input/Output       Function
Serial clock pin            SCK                  Input/output          Clock input/output
Receive data pin         RxD                  Input                     Receive data input
Transmit data pin        TxD                   Output                  Transmit data output




334
```

## PDF page 351

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=351)

```text
13.1.4      Register Configuration

Table 13.2 summarizes the SCI internal registers. These registers select the communication mode
(asynchronous or clock synchronous), specify the data format and bit rate, and control the
transmitter and receiver sections.

Table 13.2    Registers

Name                                       Abbreviation   R/W       Initial Value   Address         Access size
Serial mode register                 SMR                 R/W       H'00                H'FFFFFE00   8
Bit rate register                         BRR                  R/W       H'FF                H'FFFFFE01   8
Serial control register               SCR                  R/W       H'00                H'FFFFFE02   8
Transmit data register              TDR                  R/W       H'FF                H'FFFFFE03   8
Serial status register                SSR                  R/(W)*    H'84                H'FFFFFE04   8
Receive data register               RDR                 R            H'00                H'FFFFFE05   8
Note:   The only value that can be written is a 0 to clear the flags.


13.2      Register Descriptions


13.2.1      Receive Shift Register (RSR)

The receive shift register (RSR) receives serial data. Data input at the RxD pin is loaded into RSR
in the order received, LSB (bit 0) first, converting the data to parallel form. When one byte has
been received, it is automatically transferred to RDR. The CPU cannot read or write to RSR
directly.

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:
                  R/W:        —            —            —            —            —            —            —            —


13.2.2      Receive Data Register (RDR)

The receive data register (RDR) stores serial receive data. The SCI completes the reception of one
byte of serial data by moving the received data from the receive shift register (RSR) into RDR for
storage. RSR is then ready to receive the next data. This double buffering allows the SCI to
receive data continuously.

The CPU can read but not write to RDR. RDR is initialized to H'00 by a reset and in standby and
module standby mode.


                                                                                                                                 335
```

## PDF page 352

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=352)

```text
                     Bit:         7              6              5              4              3              2              1              0
            Bit name:
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R             R             R             R             R             R             R             R


13.2.3      Transmit Shift Register (TSR)

The transmit shift register (TSR) transmits serial data. The SCI loads transmit data from the
transmit data register (TDR) into TSR, then transmits the data serially from the TxD pin, LSB (bit
0) first. After transmitting one data byte, the SCI automatically loads the next transmit data from
TDR into TSR and starts transmitting again. If the TDRE bit in SSR is 1, however, the SCI does
not load the TDR contents into TSR. The CPU cannot read or write to TSR directly.

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:
                   R/W:        —            —            —            —            —            —            —            —


13.2.4      Transmit Data Register (TDR)

The transmit data register (TDR) is an 8-bit register that stores data for serial transmission. When
the SCI detects that the transmit shift register (TSR) is empty, it moves transmit data written in
TDR into TSR and starts serial transmission. Continuous serial transmission is possible by writing
the next transmit data in TDR during serial transmission from TSR.

The CPU can always read and write to TDR. TDR is initialized to H'FF by a reset and in standby
and module standby mode.

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:
         Initial value:         1              1              1              1              1              1              1              1
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


13.2.5      Serial Mode Register (SMR)

The serial mode register (SMR) is an 8-bit register that specifies the SCI serial communication
format and selects the clock source for the baud rate generator.

The CPU can always read and write to SMR. SMR is initialized to H'00 by a reset and in standby
and module standby mode.


336
```

## PDF page 353

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=353)

```text
                     Bit:         7              6              5              4              3              2              1              0
             Bit name:      C / A         CHR         PE          O/E       STOP         MP         CKS1      CKS0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


•    Bit 7—Communication Mode (C/A): Selects whether the SCI operates in asynchronous or
    clocked synchronous mode.

Bit 7:  C/A                          Description
0                                         Asynchronous mode                                                              (Initial value)
1                                         Clocked synchronous mode


•    Bit 6—Character Length (CHR): Selects 7-bit or 8-bit data in asynchronous mode. In clocked
    synchronous mode, the data length is always eight bits, regardless of the CHR setting.

Bit 6:  CHR                        Description
0                                         8-bit data                                                                                (Initial value)
1                                         7-bit data. (When 7-bit data is selected, the MSB (bit 7) of the transmit
                                      data register is not transmitted.)


•    Bit 5—Parity Enable (PE): Selects whether to add a parity bit to transmit data and to check the
    parity of receive data, in asynchronous mode. In clocked synchronous mode, a parity bit is
    neither added nor checked, regardless of the PE setting.

Bit 5:  PE                           Description
0                                         Parity bit not added or checked                                             (Initial value)
1                                         Parity bit added and checked. When PE is set to 1, an even or odd
                                      parity bit is added to transmit data, depending on the parity mode (O/E)
                                      setting. Receive data parity is checked according to the even/odd (O/E)
                                      mode setting.


•    Bit 4—Parity Mode (O/E): Selects even or odd parity when parity bits are added and checked.
    The O/E setting is used only in asynchronous mode and only when the parity enable bit (PE) is
    set to 1 to enable parity addition and checking. The O/E setting is ignored in clocked
    synchronous mode, and in asynchronous mode when parity addition and checking is disabled.









                                                                                                                                      337
```

## PDF page 354

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=354)

```text
Bit 4:  O/E                          Description
0                                         Even parity                                                                             (Initial value)
                                     If even parity is selected, the parity bit is added to transmit data to make
                                     an even number of 1s in the transmitted character and parity bit
                                     combined. Receive data is checked to see if it has an even number of
                                     1s in the received character and parity bit combined.
1                                         Odd parity
                                     If odd parity is selected, the parity bit is added to transmit data to make
                                     an odd number of 1s in the transmitted character and parity bit
                                     combined. Receive data is checked to see if it has an odd number of 1s
                                     in the received character and parity bit combined.


•    Bit 3—Stop Bit Length (STOP): Selects one or two bits as the stop bit length in asynchronous
    mode. This setting is used only in asynchronous mode. It is ignored in clocked synchronous
    mode because no stop bits are added.

•    In receiving, only the first stop bit is checked, regardless of the STOP bit setting. If the second
    stop bit is 1, it is treated as a stop bit, but if the second stop bit is 0, it is treated as the start bit
    of the next incoming character.

Bit 3:  STOP                      Description
0                                         One stop bit                                                                           (Initial value)
                                     In transmitting, a single 1-bit is added at the end of each transmitted
                                     character
1                                         Two stop bits
                                     In transmitting, two 1-bits are added at the end of each transmitted
                                     character


•    Bit 2—Multiprocessor Mode (MP): Selects multiprocessor format. When multiprocessor
    format is selected, settings of the parity enable (PE) and parity mode (O/E) bits are ignored.
    The MP bit setting is used only in asynchronous mode; it is ignored in clocked synchronous
    mode. For the multiprocessor communication function, see section 13.3.3, Multiprocessor
    Communication.

Bit 2:  MP                           Description
0                                         Multiprocessor function disabled                                           (Initial value)
1                                         Multiprocessor format selected








338
```

## PDF page 355

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=355)

```text
•    Bits 1 and 0—Clock Select 1 and 0 (CKS1 and CKS0): These bits select the internal clock
    source of the built-in baud rate generator. Four clock sources are available. f/4, f/16, f/64 and
    f/256. For further information on the clock source, bit rate register settings, and baud rate, see
    section 13.2.8, Bit Rate Register.

Bit 1:  CKS1        Bit 0:  CKS0        Description
00f/4                                                                           (Initial value)
                       1                              f/16
10f/64
                       1                              f/256


13.2.6      Serial Control Register (SCR)

The serial control register (SCR) operates the SCI transmitter/receiver, selects the serial clock
output in asynchronous mode, enables/disables interrupts, and selects the transmit/receive clock
source. The CPU can always read and write to SCR. SCR is initialized to H'00 by a reset and in
standby and module standby modes.

                    Bit:         7              6              5              4              3              2              1              0
            Bit name:       TIE          RIE          TE           RE         MPIE        TEIE       CKE1      CKE0
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


•    Bit 7—Transmit Interrupt Enable (TIE): Enables or disables the transmit-data-empty interrupt
    (TXI) requested when the transmit data register empty bit (TDRE) in the serial status register
    (SSR) is set to 1 due to transfer of serial transmit data from TDR to TSR.

Bit 7:  TIE                           Description
0                                         Transmit-data-empty interrupt request (TXI) is disabled       (Initial value)
                                   The TXI interrupt request can be cleared by reading TDRE after it has
                                   been set to 1, then clearing TDRE to 0, or by clearing TIE to 0.
1                                         Transmit-data-empty interrupt request (TXI) is enabled


•    Bit 6—Receive Interrupt Enable (RIE): Enables or disables the receive-data-full interrupt
    (RXI) requested when the receive data register full bit (RDRF) in the serial status register
    (SSR) is set to 1 due to transfer of serial receive data from RSR to RDR. It also enables or
    disables receive-error interrupt (ERI) requests.





                                                                                                                             339
```

## PDF page 356

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=356)

```text
Bit 6:  RIE                          Description
0                                         Receive-data-full interrupt (RXI) and receive-error interrupt (ERI)
                                     requests are disabled                                                            (Initial value)
                                     RXI and ERI interrupt requests can be cleared by reading the RDRF
                                     flag or error flag (FER, PER, or ORER) after it has been set to 1, then
                                     clearing the flag to 0, or by clearing RIE to 0.
1                                         Receive-data-full interrupt (RXI) and receive-error interrupt (ERI)
                                     requests are enabled


•    Bit 5—Transmit Enable (TE): Enables or disables the SCI serial transmitter.

Bit 5:  TE                            Description
0                                         Transmitter disabled                                                              (Initial value)
                                     The transmit data register empty bit (TDRE) in the serial status register
                                     (SSR) is locked at 1
1                                         Transmitter enabled
                                     Serial transmission starts when the transmit data register empty (TDRE)
                                     bit in the serial status register (SSR) is cleared to 0 after writing of
                                     transmit data into TDR. Select the transmit format in SMR before setting
                                     TE to 1.


•    Bit 4—Receive Enable (RE): Enables or disables the SCI serial receiver.

Bit 4:  RE                           Description
0                                         Receiver disabled                                                                  (Initial value)
                                     Clearing RE to 0 does not affect the receive flags (RDRF, FER, PER,
                                     ORER). These flags retain their previous values.
1                                         Receiver enabled
                                     Serial reception starts when a start bit is detected in asynchronous
                                     mode, or synchronous clock input is detected in clocked synchronous
                                     mode. Select the receive format in SMR before setting RE to 1.


•    Bit 3—Multiprocessor Interrupt Enable (MPIE): Enables or disables multiprocessor interrupts.
    The MPIE setting is used only in asynchronous mode, and only if the multiprocessor mode bit
    (MP) in the serial mode register (SMR) is set to 1 during reception. The MPIE setting is
    ignored in clocked synchronous mode or when the MP bit is cleared to 0.









340
```

## PDF page 357

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=357)

```text
Bit 3:  MPIE                       Description
0                                         Multiprocessor interrupts are disabled (normal receive operation)
                                                                                                              (Initial value)
                                  MPE is cleared to 0 when MPIE is cleared to 0, or the multiprocessor bit
                                  (MPB) is set to 1 in receive data.
1                                         Multiprocessor interrupts are enabled
                                  Receive-data-full interrupt requests (RXI), receive-error interrupt
                                  requests (ERI), and setting of the RDRF, FER, and ORER status flags
                                  in the serial status register (SSR) are disabled until the multiprocessor
                                  bit is set to 1.
                                  The SCI does not transfer receive data from RSR to RDR, does not
                                  detect receive errors, and does not set the RDRF, FER, and ORER
                                  flags in the serial status register (SSR). When it receives data that
                                  includes MPB = 1, MPB is set to 1 in SSR, and the SCI automatically
                                  clears MPIE to 0, generates RXI and ERI interrupts (if the TIE and RIE
                                  bits in SCR are set to 1), and enables the FER and ORER bits to be set.


•    Bit 2—Transmit-End Iinterrupt Enable (TEIE): Enables or disables the transmit-end interrupt
    (TEI) requested if TDR does not contain new transmit data when the MSB is transmitted.

Bit 2:  TEIE                        Description
0                                         Transmit-end interrupt (TEI) requests are disabled*              (Initial value)
1                                         Transmit-end interrupt (TEI) requests are enabled*
Note:   The TEI request can be cleared by reading the TDRE bit in the serial status register (SSR)
         after it has been set to 1, then clearing TDRE to 0; by clearing the transmit end (TEND) bit
         to 0; or by clearing the TEIE bit to 0.


•    Bits 1 and 0—Clock Enable 1 and 0 (CKE1 and CKE0): These bits select the SCI clock source
    and enable or disable clock output from the SCK pin. Depending on the combination of CKE1
    and CKE0, the SCK pin can be used for general-purpose input/output, serial clock output, or
    serial clock input.

    The CKE0 setting is valid only in asynchronous mode, and only when the SCI is internally
    clocked (CKE1 = 0). The CKE0 setting is ignored in clocked synchronous mode, or when an
    external clock source is selected (CKE1 = 1). Select the SCI operating mode in the serial mode
    register (SMR) before setting CKE1 and CKE0. For further details on selection of the SCI
    clock source, see table 13.9 in section 13.3, Operation.









                                                                                                                          341
```

## PDF page 358

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=358)

```text
Bit 1:   Bit 0:
CKE1     CKE0  Description
0          0          Asynchronous mode               Internal clock, SCK pin used for input pin (input signal
                                                           is ignored or output pin output level is undefined)*1
                   Clocked synchronous mode    Internal clock, SCK pin used for synchronous clock
                                                           output*1
0          1          Asynchronous mode               Internal clock, SCK pin used for clock output*2
                   Clocked synchronous mode    Internal clock, SCK pin used for synchronous clock
                                                           output
1          0          Asynchronous mode               External clock, SCK pin used for clock input*3
                   Clocked synchronous mode    External clock, SCK pin used for synchronous clock
                                                           input
1          1          Asynchronous mode               External clock, SCK pin used for clock input*3
                   Clocked synchronous mode    External clock, SCK pin used for synchronous clock
                                                           input
Notes:  1.   Initial value
          2.   The output clock frequency is the same as the bit rate.
          3.   The input clock frequency is 16 times the bit rate.


13.2.7      Serial Status Register (SSR)

The serial status register (SSR) is an 8-bit register containing multiprocessor bit values, and status
flags that indicate the SCI operating status.

The CPU can always read and write to SSR, but cannot write 1 in the status flags (TDRE, RDRF,
ORER, PER, and FER). These flags can be cleared to 0 only if they have first been read (after
being set to 1). Bits 2 (TEND) and 1 (MPB) are read-only bits that cannot be written. SSR is
initialized to H'84 by a reset and in standby and module standby mode.

                    Bit:         7              6              5              4              3              2              1              0
            Bit name:     TDRE      RDRF      ORER       FER        PER        TEND       MPB       MPBT
        Initial value:         1              0              0              0              0              1              0              0
                  R/W:    R/(W)*     R/(W)*     R/(W)*     R/(W)*     R/(W)*          R             R           R/W
Note:   The only value that can be written is a 0 to clear the flag.

•    Bit 7—Transmit Data Register Empty (TDRE): Indicates that the SCI has loaded transmit data
    from TDR into TSR and new serial transmit data can be written in TDR.






342
```

## PDF page 359

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=359)

```text
Bit 7:  TDRE        Description
0                           TDR contains valid transmit data
                       TDRE is cleared to 0 when software reads TDRE after it has been set to 1, then
                       writes 0 in TDRE, or the DMAC writes data in TDR.
1                           TDR does not contain valid transmit data                                            (Initial value)
                       TDRE is set to 1 when the chip is reset or enters standby mode, the TE bit in the
                       serial control register (SCR) is cleared to 0, or TDR contents are loaded into
                       TSR, so new data can be written in TDR.


•    Bit 6—Receive Data Register Full (RDRF): Indicates that RDR contains received data.

Bit 6:  RDRF        Description
0                           RDR does not contain valid receive data                                             (Initial value)
                       RDRF is cleared to 0 when the chip is reset or enters standby mode, software
                       reads RDRF after it has been set to 1, then writes 0 in RDRF, or the DMAC
                       reads data from RDR.
1                           RDR contains valid received data
                       RDRF is set to 1 when serial data is received normally and transferred from RSR
                       to RDR.
Note:   RDR and RDRF are not affected by detection of receive errors or by clearing of the RE bit
         to 0 in the serial control register. They retain their previous contents. If RDRF is still set to 1
         when reception of the next data ends, an overrun error (ORER) occurs and the received
         data is lost.


•    Bit 5—Overrun Error (ORER): Indicates that data reception ended abnormally due to an
    overrun error.

Bit 5:  ORER       Description
0                           Receiving is in progress or has ended normally*1                                (Initial value)
                       ORER is cleared to 0 when the chip is reset or enters standby mode, or software
                       reads ORER after it has been set to 1, then writes 0 in ORER.
1                           A receive overrun error occurred*2
                       ORER is set to 1 if reception of the next serial data ends when RDRF is set to 1.
Notes:  1.   Clearing the RE bit to 0 in the serial control register does not affect the ORER bit, which
               retains its previous value.
          2.   RDR continues to hold the data received before the overrun error, so subsequent
               receive data is lost. Serial receiving cannot continue while ORER is set to 1. In clocked
               synchronous mode, serial transmitting is disabled.







                                                                                                                             343
```

## PDF page 360

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=360)

```text
•    Bit 4—Framing Error (FER): Indicates that data reception ended abnormally due to a framing
    error in asynchronous mode.

Bit 4:  FER           Description
0                           Receiving is in progress or has ended normally                                  (Initial value)
                       Clearing the RE bit to 0 in the serial control register does not affect the FER bit,
                       which retains its previous value.
                       FER is cleared to 0 when the chip is reset or enters standby mode, or software
                       reads FER after it has been set to 1, then writes 0 in FER.
1                           A receive framing error occurred
                       When the stop bit length is two bits, only the first bit is checked. The second stop
                       bit is not checked. When a framing error occurs, the SCI transfers the receive
                       data into RDR but does not set RDRF. Serial receiving cannot continue while
                       FER is set to 1. In clocked synchronous mode, serial transmitting is also
                       disabled.
                       FER is set to 1 if the stop bit at the end of receive data is checked and found to
                       be 0.


•    Bit 3—Parity Error (PER): Indicates that data reception (with parity) ended abnormally due to
    a parity error in asynchronous mode.

Bit 3:  PER          Description
0                           Receiving is in progress or has ended normally                                  (Initial value)
                       Clearing the RE bit to 0 in the serial control register does not affect the PER bit,
                       which retains its previous value.
                       PER is cleared to 0 when the chip is reset or enters standby mode, or software
                       reads PER after it has been set to 1, then writes 0 in PER.
1                           A receive parity error occurred
                       When a parity error occurs, the SCI transfers the receive data into RDR but does
                       not RDRF. Serial receiving cannot continue while PER is set to 1. In clocked
                       synchronous mode, serial transmitting is also disabled.
                       PER is set to 1 if the number of 1s in receive data, including the parity bit, does
                       not match the even or odd parity setting of the parity mode bit (O/E) in the serial
                       mode register (SMR).













344
```

## PDF page 361

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=361)

```text
•    Bit 2—Transmit End (TEND): Indicates that when the last bit of a serial character was
    transmitted, TDR did not contain valid data, so transmission has ended. TEND is a read-only
    bit and cannot be written.

Bit 2:  TEND        Description
0                           Transmission is in progress
                        TEND is cleared to 0 when software reads TDRE after it has been set to 1, then
                        writes 0 in TDRE, or the DMAC writes data in TDR.
1                           End of transmission                                                                              (Initial value)
                        TEND is set to 1 when the chip is reset or enters standby mode, TE is cleared to
                        0 in the serial control register (SCR), or TDRE is 1 when the last bit of a one-byte
                        serial character is transmitted.


•    Bit 1—Multiprocessor Bit (MPB): Stores the value of the multiprocessor bit in receive data
    when a multiprocessor format is selected for receiving in asynchronous mode. MPB is a read-
    only bit and cannot be written.

Bit 1:  MPB          Description
0                           Multiprocessor bit value in receive data is 0                                        (Initial value)
                        If RE is cleared to 0 when a multiprocessor format is selected, MPB retains its
                        previous value.
1                           Multiprocessor bit value in receive data is 1


•    Bit 0—Multiprocessor Bit Transfer (MPBT): Stores the value of the multiprocessor bit added
    to transmit data when a multiprocessor format is selected for transmitting in asynchronous
    mode. The MPBT setting is ignored in clocked synchronous mode, when a multiprocessor
    format is not selected, or when the SCI is not transmitting.

Bit 0:  MPBT        Description
0                           Multiprocessor bit value in transmit data is 0                                       (Initial value)
1                           Multiprocessor bit value in transmit data is 1
















                                                                                                                               345
```

## PDF page 362

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=362)

```text
13.2.8      Bit Rate Register (BRR)

The bit rate register (BRR) is an 8-bit register that, together with the baud rate generator clock
source selected by the CKS1 and CKS0 bits in the serial mode register (SMR), determines the
serial transmit/receive bit rate.

The CPU can always read and write to BRR. BRR is initialized to H'FF by a reset and in standby
mode.

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:
         Initial value:         1              1              1              1              1              1              1              1
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


Table 13.3 shows examples of BRR settings in asynchronous mode; table 13.4 shows examples of
BBR settings in clocked synchronous mode.







































346
```

## PDF page 363

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=363)

```text
Table 13.3    Bit Rates and BRR Settings in Asynchronous Mode

                                                                                   f (MHz)
                                     4                            4.9152                            8                            9.8304
Bit Rate              n         N        Error          n        N        Error          n        N        Error                                Error
(bits/s)                                   (%)                               (%)                              (%)       n        N              (%)
110                1         70       0.03      1        86       0.31      1        141     0.03      1        174     –0.26
150                0         207     0.16      0        255     0.00      1        103     0.16      1        127     0.00
300                0         103     0.16      0        127     0.00      0        207     0.16      0        255     0.00
600                0         51       0.16      0        63       0.00      0        103     0.16      0        127     0.00
1200              0         25       0.16      0        31       0.00      0        51       0.16      0        63       0.00
2400              0         12       0.16      0        15       0.00      0        25       0.16      0        31       0.00
4800              —       —       —         0        7         0.00      0        12       0.16      0        15       0.00
9600              —       —       —         0        3         0.00      —       —       —         0        7         0.00
19200            —       —       —         0        1         0.00      —       —       —         0        3         0.00
31250            0         0         0.00      —       —       —         0        1         0.00      —       —       —
38400            —       —       —         0        0         0.00      —       —       —         0        1         0.00


                                                                                   f (MHz)
                                    12                          14.7456                         16                          19.6608
Bit Rate                                   Error                             Error                            Error                             Error
(bits/s)           n         N             (%)       n        N              (%)       n        N             (%)       n        N              (%)
110                1         212     0.03      2        64       0.70      2        70       0.03      2        86       0.31
150                1         155     0.16      1        191     0.00      1        207     0.16      1        255     0.00
300                1         77       0.16      1        95       0.00      1        103     0.16      1        127     0.00
600                0         155     0.16      0        191     0.00      0        207     0.16      0        255     0.00
1200              0         77       0.16      0        95       0.00      0        103     0.16      0        127     0.00
2400              0         38       0.16      0        47       0.00      0        51       0.16      0        63       0.00
4800              0         19       –2.34    0        23       0.00      0        25       0.16      0        31       0.00
9600              0         9         –2.34    0        11       0.00      0        12       0.16      0        15       0.00
19200            0         4         –2.34    0        5         0.00      —       —       —         0        7         0.00
31250            0         2         0.00      —       —       —         0        3         0.00      0        4         –1.70
38400            —       —       —         0        2         0.00      —       —       —         0        3         0.00






                                                                                                                                                       347
```

## PDF page 364

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=364)

```text
Table 13.3    Bit Rates and BRR Settings in Asynchronous Mode (cont)

                                                                                   f (MHz)
                                    20                               24                          24.576                          28.7
Bit Rate                                   Error                             Error                             Error                            Error
(bits/s)           n         N             (%)       n        N              (%)       n        N              (%)       n        N             (%)
110                2         88       –0.25    2        106     –0.44    2        108     0.08      2        126     0.31
150                2         64       0.16      2        77       0.16      2        79       0.00      2        92       0.46
300                1         129     0.16      1        155     0.16      1        159     0.00      1        186     -0.08
600                1         64       0.16      1        77       0.16      1        79       0.00      1        92       0.46
1200              0         129     0.16      0        155     0.16      0        159     0.00      0        186     -0.08
2400              0         64       0.16      0        77       0.16      0        79       0.00      0        92       0.46
4800              0         32       –1.36    0        38       0.16      0        39       0.00      0        46       -0.61
9600              0         15       1.73      0        19       –2.34    0        19       0.00      0        22       1.55
19200            0         7         1.73      0        9         –2.34    0        9         0.00      0        11       -2.68
31250            0         4         0.00      0        5         0.00      0        5         2.40      0        6         2.50
38400            0         3         1.73      0        4         –2.34    0        4         0.00      0        5         -2.68


































348
```

## PDF page 365

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=365)

```text
Table 13.4    Bit Rates and BRR Settings in Clocked Synchronous Mode

                                                                            f (MHz)

Bit Rate                                          4                           8                         16                        28.7
(bits/s)                               n            N            n            N            n            N            n            N
110                                     2            141        3            70          3            141        3            254
250                                     1            249        2            124        2            249        3            111
500                                     1            124        1            249        2            124        2            223
1k                                       0            249        1            124        1            249        2            111
2.5k                                    0            99          0            199        1            99          1            178
5k                                       0            49          0            99          0            199        1            89
10k                                     0            24          0            49          0            99          0            178
25k  09019039071
50k  0409019035
100k                                   —           — 0409017
250k                                   0            0* 0103——
500k                                                                0            0*           0            1            —           —
1M                                                                                                0            0*           —           —
Note:   Settings with an error of 1% or less are recommended.
         Explanation of symbols:
           Blank:     No setting possible
               —:     Setting possible, but error occurs
                 *:     Continuous transmission/reception not possible
         The BRR setting is calculated as follows:
         Asynchronous mode:

            N =         φ256 ×  22n – 1 ×  B    ×  106 – 1


         Clocked synchronous mode:

            N =         φ32 ×  22n – 1 ×  B   ×  106 – 1


         B:   Bit rate (bit/s)
         N:  BRR setting for baud rate generator (0 £ N £ 255)
         f:    Operating frequency (MHz)
         n:   Baud rate generator clock source (n = 0, 1, 2, 3) (For the clock sources and values of n,
              see table 13.6.)





                                                                                                                           349
```

## PDF page 366

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=366)

```text
Table 13.5    SMR Settings

                                                                                                                SMR Settings
n                                     Clock Source                 CKS1                              CKS0
0                                          f/4                                   0                                      0
1                                          f/16                                 0                                      1
2                                          f/164                               1                                      0
3                                          f/256                               1                                      1
The bit rate error for asynchronous mode is given by the following equation:
               Error (%)  =                    φ  ×  106                                     – 1
                                      (N + 1) ×  B  ×  256 ×  22n – 1                                × 100
                                                                                                    

Table 13.6 shows the maximum bit rates in asynchronous mode when the baud rate generator is
being used. Tables 13.7 and 13.8 show the maximum rates for external clock input.

Table 13.6    Maximum Bit Rates for Various Frequencies with Baud Rate Generator
                      (Asynchronous Mode)

                                                                                                                               Settings
f (MHz)                              Maximum Bit Rate (bits/s)              n                           N
4                                         31250                                                0                           0
4.9152                                38400                                                0                           0
8                                         62500                                                0                           0
9.8304                                76800                                                0                           0
12                                       93750                                                0                           0
14.7456                              115200                                              0                           0
16                                       125000                                              0                           0
19.6608                              153600                                              0                           0
20                                       156250                                              0                           0
24                                       187500                                              0                           0
24.576                                192000                                              0                           0
28.7                                    224218                                              0                           0











350
```

## PDF page 367

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=367)

```text
Table 13.7    Maximum Bit Rates with External Clock Input (Asynchronous Mode)

f (MHz)                                        External Input Clock (MHz)       Maximum Bit Rate (bits/s)
4                                                   0.2500                                          15625
4.9152                                          0.3072                                          19200
8                                                   0.5000                                          31250
9.8304                                          0.6144                                          38400
12                                                 0.7500                                          46875
14.7456                                        0.9216                                          57600
16                                                 1.0000                                          62500
19.6608                                        1.2288                                          76800
20                                                 1.2500                                          78125
24                                                 1.5000                                          93750
24.576                                          1.5360                                          96000
28.7                                              1.79375                                        112109


Table 13.8    Maximum Bit Rates with External Clock Input (Clocked Synchronous Mode)

f (MHz)                                        External Input Clock (MHz)       Maximum Bit Rate (bits/s)
8                                                   0.3333                                          333333.3
16                                                 0.6667                                          666666.7
24                                                 1.0000                                          1000000.0
28.7                                              1.1958                                          1195833.3
























                                                                                                                                                            351
```

## PDF page 368

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=368)

```text
13.3      Operation


13.3.1      Overview

For serial communication, the SCI has an asynchronous mode in which characters are
synchronized individually, and a clocked synchronous mode in which communication is
synchronized with clock pulses. Asynchronous/clocked synchronous mode and the communication
format are selected in the serial mode register (SMR), as shown in table 13.9. The SCI clock
source is selected by the C/A bit in the serial mode register (SMR) and the CKE1 and CKE0 bits
in the serial control register (SCR), as shown in table 13.10.

Asynchronous Mode:

•    Data length is selectable. seven or eight bits.
•    Parity and multiprocessor bits are selectable, as is the stop bit length (one or two bits). The
    preceding selections constitute the communication format and character length.
•    In receiving, it is possible to detect framing errors, parity errors, overrun errors, and the break
    state.
•    An internal or external clock can be selected as the SCI clock source.
    ¾    When an internal clock is selected, the SCI operates using the built-in baud rate generator,
         and can output a serial clock signal with a frequency matching the bit rate.
    ¾    When an external clock is selected, the external clock input must have a frequency 16 times
         the bit rate. (The built-in baud rate generator is not used.)

Clocked Synchronous Mode:

•    The communication format has a fixed eight-bit data length.
•    In receiving, it is possible to detect overrun errors.
•    An internal or external clock can be selected as the SCI clock source.
    ¾    When an internal clock is selected, the SCI operates using the built-in baud rate generator,
         and outputs a synchronous clock signal to external devices.
    ¾    When an external clock is selected, the SCI operates on the input synchronous clock. The
         built-in baud rate generator is not used.














352
```

## PDF page 369

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=369)

```text
Table 13.9    Serial Mode Register Settings and SCI Communication Formats

                                           SMR Settings                               SCI Communication Format
                          Bit 7     Bit 6       Bit 5      Bit 2       Bit 3        Data           Parity        Multipro-         Stop Bit
Mode                      C/A       CHR         PE         MP          STOP         Length         Bit           cessor Bit        Length
Asynchronous      0 0000  8-bit         Not set    Not set        1 bit
                                                                       1                                                                2 bits
                                                1                       0                             Set                             1 bit
                                                                       1                                                                2 bits
                                    1          0                       0            7-bit         Not set                       1 bit
                                                                       1                                                                2 bits
                                                1                       0                             Set                             1 bit
                                                                       1                                                                2 bits
Asynchronous      0         0          *           1          0            8-bit         Not set    Set              1 bit
(multiprocessor                            *                       1                                                                2 bits
format)                             1          *                       0            7-bit                                            1 bit

                                                *                       1                                                                2 bits
Clocked                   1****  8-bit         Not set    Not set        None
synchronous
Note:   Asterisks (*) in the table indicate don’t care bits.


Table 13.10    SMR and SCR Settings and SCI Clock Source Selection

                         SMR       SCR Settings                          SCI Transmit/Receive Clock
                        Bit 7      Bit 1         Bit 0
Mode                    C/A        CKE1          CKE0      Clock Source   SCK Pin Function
Asynchronous    0          0            0              Internal              SCI does not use the SCK pin
                                                 1                                        Outputs a clock with frequency
                                                                                        matching the bit rate
                                   1            0              External             Inputs a clock with frequency 16 times
                                                                                        the bit rate
                                                 1
Clocked synch-  1          0            0              Internal              Outputs the synchronous clock
ronous                                          1

                                   1            0              External             Inputs the synchronous clock
                                                 1




                                                                                                                                            353
```

## PDF page 370

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=370)

```text
13.3.2      Operation in Asynchronous Mode

In asynchronous mode, each transmitted or received character begins with a start bit and ends with
a stop bit. Serial communication is synchronized one character at a time.

The transmitting and receiving sections of the SCI are independent, so full duplex communication
is possible. The transmitter and receiver are both double buffered, so data can be written and read
while transmitting and receiving are in progress, enabling continuous transmitting and receiving.

Figure 13.2 shows the general format of asynchronous serial communication. In asynchronous
serial communication, the communication line is normally held in the mark (high) state. The SCI
monitors the line and starts serial communication when the line goes to the space (low) state,
indicating a start bit. One serial character consists of a start bit (low), data (LSB first), parity bit
(high or low), and stop bit (high), in that order.

When receiving in asynchronous mode, the SCI synchronizes on the falling edge of the start bit.
The SCI samples each data bit on the eighth pulse of a clock with a frequency 16 times the bit rate.
Receive data is latched at the center of each bit.


                                                                                       Idle (mark) state
              1           (LSB)                                                         (MSB)            1
          Serial     0D0D1D2D3D4D5D6D70/1    1       1
          data    State                                                            Parity     Stop
                     bit                                                              bit       bit
                                          Transmit/receive data
                   1 bit                         7 or 8 bits                         1 or      1 or
                                                                                    no bit    2 bits

                            One unit of communication data (character or frame)


             Figure 13.2   Example of Data Format in Asynchronous Communication
                                (8-Bit Data with Parity and Two Stop Bits)















354
```

## PDF page 371

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=371)

```text
Transmit/Receive Formats. Table 13.11 shows the 12 communication formats that can be
selected in asynchronous mode. The format is selected by settings in the serial mode register
(SMR).

Table 13.11    Serial Communication Formats (Asynchronous Mode)

          SMR Bits                             Serial Transmit/Receive Format and Frame Length
CHR   PE    MP    STOP         1 23456789 10 11 12

0         0       0       0            START                         8-bit data                         STOP

0         0       0       1            START                         8-bit data                         STOP   STOP

0         1       0       0            START                         8-bit data                             P      STOP

0         1       0       1            START                         8-bit data                             P      STOP   STOP

1         0       0       0            START                   7-bit data                   STOP

1         0       0       1            START                   7-bit data                   STOP   STOP

1         1       0       0            START                   7-bit data                       P      STOP

1         1       0       1            START                   7-bit data                       P      STOP   STOP

0         —     1       0            START                         8-bit data                           MPB    STOP

0         —     1       1            START                         8-bit data                           MPB    STOP   STOP

1         —     1       0            START                   7-bit data                    MPB    STOP

1         —     1       1            START                   7-bit data                    MPB    STOP   STOP

—: Don’t care bits.
START: Start bit
STOP: Stop bit
P: Parity bit
MPB: Multiprocessor bit












                                                                                                                                        355
```

## PDF page 372

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=372)

```text
Clock: An internal clock generated by the built-in baud rate generator or an external clock input
from the SCK pin can be selected as the SCI transmit/receive clock. The clock source is selected
by the C/A bit in the serial mode register (SMR) and bits CKE1 and CKE0 in the serial control
register (SCR) (table 13.9).

When an external clock is input at the SCK pin, it must have a frequency equal to 16 times the
desired bit rate.

When the SCI operates on an internal clock, it can output a clock signal at the SCK pin. The
frequency of this output clock is equal to the bit rate. The phase is aligned as in figure 13.3 so that
the rising edge of the clock occurs at the center of each transmit data bit.




              0D0D1D2D3D4D5D6D70/111

                                              1 frame


            Figure 13.3   Output Clock and Serial Data Timing (Asynchronous Mode)

Transmitting and Receiving Data

SCI Initialization (Asynchronous Mode): Before transmitting or receiving, clear the TE and RE
bits to 0 in the serial control register (SCR), then initialize the SCI as follows.

When changing the operation mode or communication format, always clear the TE and RE bits to
0 before following the procedure given below. Clearing TE to 0 sets TDRE to 1 and initializes the
transmit shift register (TSR). Clearing RE to 0, however, does not initialize the RDRF, PER, FER,
and ORER flags and receive data register (RDR), which retain their previous contents.

When an external clock is used, the clock should not be stopped during initialization or subsequent
operation. SCI operation becomes unreliable if the clock is stopped.

Figure 13.4 shows a sample flowchart for initializing the SCI. The procedure for initializing the
SCI is as follows:

1.   Select the communication format in the serial mode register (SMR).
2.   Write the value corresponding to the bit rate in the bit rate register (BRR) unless an external
    clock is used.
3.   Select the clock source in the serial control register (SCR). Leave RIE, TIE, TEIE, MPIE, TE
    and RE cleared to 0. If clock output is selected in asynchronous mode, clock output starts
    immediately after the setting is made in SCR.



356
```

## PDF page 373

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=373)

```text
4.   Wait for at least the interval required to transmit or receive one bit, then set TE or RE in the
    serial control register (SCR) to 1. Also set RIE, TIE, TEIE and MPIE as necessary. Setting TE
    or RE enables the SCI to use the TxD or RxD pin. The initial states are the mark state when
    transmitting, and the idle state when receiving (waiting for a start bit).



                                                    Initialization


                                      Clear TE and RE bits in SCR to 0


                                            Select transmit/receive                 1
                                                  format in SMR

                                                Set value in BRR                     2


                                       Set CKE1 and CKE0 bits in SCR                3
                                             (TE and RE bits are 0)

                                                                    Wait

                                                    Has a 1-bit                   No
                                                interval elapsed?

                                                             Yes
                                       Set TE and RE bits in SCR to 1               4
                                      and set RIE, TEIE, and MPIE bits


                                                        End


  Note:   Circled numbers refer to the preceding description of the procedure in the text.

                          Figure 13.4   Sample Flowchart for SCI Initialization

















                                                                                                                      357
```

## PDF page 374

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=374)

```text
Transmitting Serial Data (Asynchronous Mode): Figure 13.5 shows a sample flowchart for
transmitting serial data. The procedure for transmitting serial data is as follows:

1.   SCI status check and transmit data write: read the serial status register (SSR), check that the
    TDRE bit is 1, then write transmit data in the transmit data register (TDR) and clear TDRE
    to 0.
2.   To continue transmitting serial data, read the TDRE bit to check whether it is safe to write (if it
    reads 1); if so, write data in TDR, then clear TDRE to 0. When the DMAC is started by a
    transmit-data-empty interrupt request (TXI) in order to write data in TDR, the TDRE bit is
    checked and cleared automatically.



                                              Start transmission



                                           Read TDRE bit in SSR                    1


                                                  TDRE = 1?                    No


                                                           Yes
                                      Write transmit data to TDR and
                                         clear TDRE bit in SSR to 0

                                                                            2
                                             All data transmitted?            No

                                                           Yes

                                           Read TEND bit in SSR


                                                  TEND = 1?                    No


                                                           Yes

                                               End transmission

  Note:   Circled numbers refer to the preceding description of the procedure in the text.

                    Figure 13.5   Sample Flowchart for Transmitting Serial Data







358
```

## PDF page 375

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=375)

```text
In transmitting serial data, the SCI operates as follows:

1.   The SCI monitors the TDRE bit in SSR. When TDRE is cleared to 0, the SCI recognizes that
    the transmit data register (TDR) contains new data, and loads this data from TDR into the
    transmit shift register (TSR).

2.   After loading the data from TDR into TSR, the SCI sets the TDRE bit to 1 and starts
    transmitting. If the transmit-data-empty interrupt enable bit (TIE) is set to 1 in SCR, the SCI
    requests a transmit-data-empty interrupt (TXI) at this time.
    Serial transmit data is transmitted in the following order from the TxD pin:
    a.   Start bit: one 0-bit is output.
    b.   Transmit data: seven or eight bits of data are output, LSB first.
    c.   Parity bit or multiprocessor bit: one parity bit (even or odd parity) or one multiprocessor bit
         is output. Formats in which neither a parity bit nor a multiprocessor bit is output can also
         be selected.
    d.   Stop bit: one or two 1-bits (stop bits) are output.
    e.   Mark state: output of 1-bits continues until the start bit of the next transmit data.

3.   The SCI checks the TDRE bit when it outputs the stop bit. If TDRE is 0, the SCI loads new
    data from TDR into TSR, outputs the stop bit, then begins serial transmission of the next
    frame. If TDRE is 1, the SCI sets the TEND bit to 1 in SSR, outputs the stop bit, then
    continues output of 1-bits (mark state). If the transmit-end interrupt enable bit (TEIE) in SCR
    is set to 1, a transmit-end interrupt (TEI) is requested.

Figure 13.6 shows an example of SCI transmit operation in asynchronous mode.


























                                                                                                                         359
```

## PDF page 376

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=376)

```text
                  Start                      Parity Stop   Start                     Parity
            1  1bit                 Data      bit     bit   bit                 Data                Stopbitbit
    Serial          01D0    D1         D7                              D0    D1         D7    0/10/1             0                                             1Idle
      data                                                                                            (mark)
                                                                                                        state

    TDRE


    TEND

     TXI interrupt     TXI interrupt       TXI interrupt
        request       handler writes          request
       generated        data to TDR         generated                                            TEI interruptrequest
                        and clears                                                       generated
                      TDRE bit to 0

                                 1 frame


  Example: 8-bit data with parity and one stop bit

           Figure 13.6   Example of SCI Transmit Operation in Asynchronous Mode
                               (8-Bit Data with Parity and One Stop Bit)

Receiving Serial Data (Asynchronous Mode): Figure 13.7 shows a sample flowchart for
receiving serial data. The procedure for receiving serial data is as follows:

1.   Receive error handling: if a receive error occurs, read the ORER, PER and FER bits of the SSR
    to identify the error. After executing the necessary error handling, clear ORER, PER and FER
    all to 0. Receiving cannot resume if ORER, PER or FER remain set to 1.
2.   SCI status check and receive-data read: read the serial status register (SSR), check that RDRF
    is set to 1, then read receive data from the receive data register (RDR) and clear RDRF to 0.
    The RXI interrupt can also be used to determine if the RDRF bit has changed from 0 to 1.
3.   To continue receiving serial data: read the RDRF and RDR bits and clear RDRF to 0 before the
    stop bit of the current frame is received. If the DMAC is started by a receive-data-full interrupt
    (RXI) to read RDR, the RDRF bit is cleared automatically so this step is unnecessary.














360
```

## PDF page 377

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=377)

```text
                                    Start reception


                            Read ORER, PER, and FER
                                      bits in SSR


                               PER, FER, ORER = 1?               Yes
                                                                                     1

                                              No                          Error handling

                               Read RDRF bit in SSR                2


                       No             RDRF = 1?


                                              Yes

                           Read receive data in RDR, and           3
                             clear RDRF bit in SSR to 0



                      No          All data received?


                                              Yes

                              Clear RE bit in SCR to 0


                                    End reception


Note:   Circled numbers refer to the preceding description of the procedure in the text.

                    Figure 13.7   Sample Flowchart for Receiving Serial Data

















                                                                                                             361
```

## PDF page 378

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=378)

```text
                                                    Error handling


                                   No                ORER = 1?



                                                             Yes
                                               Overrun error handling



                                   No                  FER = 1?


                                                             Yes

                                               Framing error handling



                                   No                 PER = 1?



                                                             Yes

                                                Parity error handling



                                               Clear ORER, PER, and
                                                FER bits in SSR to 0

                                                          End


  Note:   Circled numbers refer to the preceding description of the procedure in the text.

                  Figure 13.7   Sample Flowchart for Receiving Serial Data (cont)
















362
```

## PDF page 379

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=379)

```text
In receiving, the SCI operates as follows:

1.   The SCI monitors the receive data line. When it detects a start bit (0), the SCI synchronizes
    internally and starts receiving.
2.   Receive data is shifted into RSR in order from LSB to MSB.
3.   The parity bit and stop bit are received. After receiving these bits, the SCI makes the following
    checks:
    a.   Parity check: the number of 1s in the receive data must match the even or odd parity setting
         of the O/E bit in SMR.
    b.   Stop bit check: the stop bit value must be 1. If there are two stop bits, only the first stop bit
         is checked.
    c.   Status check: RDRF must be 0 so that receive data can be loaded from RSR into RDR.

    If these checks all pass, the SCI sets RDRF to 1 and stores the received data in RDR. If one of
    the checks fails (receive error), the SCI operates as indicated in table 13.11.

     Note:    When a receive error flag is set, further receiving is disabled. In reception, the RDRF
                bit is not set to 1. Be sure to clear the error flags.

4.   After setting RDRF to 1, if the receive-data-full interrupt enable bit (RIE) is set to 1 in SCR,
    the SCI requests a receive-data-full interrupt (RXI). If one of the error flags (ORER, PER, or
    FER) is set to 1 and the receive-data-full interrupt enable bit (RIE) in SCR is also set to 1, the
    SCI requests a receive-error interrupt (ERI).

Figure 13.8 shows an example of SCI receive operation in asynchronous mode.

Table 13.12    Receive Error Conditions and SCI Operation

Receive Error      Abbreviation     Condition                                         Data Transfer
Overrun error        ORER                Receiving of next data ends while                 Receive data not loaded
                                             RDRF is still set to 1 in SSR                 from RSR into RDR
Framing error       FER                    Stop bit is 0                                       Receive data loaded from
                                                                                           RSR into RDR
Parity error           PER                   Parity of receive data differs from           Receive data loaded from
                                             even/odd parity setting in SMR                RSR into RDR













                                                                                                                           363
```

## PDF page 380

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=380)

```text
             1  1Start                     Parity  Stop   Start                     Parity
                  bit                 Data   bit     bit   bit                 Data                Stopbitbit
    Serial         01D0    D1         D7                              D0    D1         D7    0/10/1             0                                             0Idle
      data                                                                                             (mark)
                                                                                                         state

   TDRE


                                                 RXI  interrupt
     FER                                      request generated



                                1 frame                           RXI interrupt           ERI  interrupt
                                                                 handler reads         request generated
                                                                data and clears          by framing error
                                                                  RDRF bit to 0

                           Figure 13.8   Example of SCI Receive Operation
                               (8-Bit Data with Parity and One Stop Bit)


13.3.3      Multiprocessor Communication

The multiprocessor communication function enables several processors to share a single serial
communication line. The processors communicate in asynchronous mode using a format with an
additional multiprocessor bit (multiprocessor format).

In multiprocessor communication, each receiving processor is addressed by a unique ID. A serial
communication cycle consists of an ID-sending cycle that identifies the receiving processor, and a
data-sending cycle. The multiprocessor bit distinguishes ID-sending cycles from data-sending
cycles. The transmitting processor starts by sending the ID of the receiving processor with which it
wants to communicate as data with the multiprocessor bit set to 1. Next, the transmitting processor
sends transmit data with the multiprocessor bit cleared to 0.

Receiving processors skip incoming data until they receive data with the multiprocessor bit set to
1. When they receive data with the multiprocessor bit set to 1, receiving processors compare the
data with their IDs. The receiving processor with a matching ID continues to receive further
incoming data. Processors with IDs not matching the received data skip further incoming data
until they again receive data with the multiprocessor bit set to 1. Multiple processors can send and
receive data in this way.

Figure 13.9 shows an example of communication among processors using the multiprocessor
format.




364
```

## PDF page 381

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=381)

```text
           Transmitting
            processor

                                                Serial communication line



            Receiving                      Receiving                     Receiving                    Receiving
           processor A                    processor B                  processor C                   processor D

             (ID = 01)                      (ID = 02)                     (ID = 03)                   (ID = 04)



   Serial                            H'01                                          H'AA
     data
                                            (MPB = 1)                             (MPB = 0)

                                ID transmit cycle =                  Data transmit cycle =
                                 receiving station                   data transmission to
                                   specification                  receiving station specified
                                                                               by ID
  MPB: Multiprocessor bit

 Figure 13.9   Example of Communication among Processors Using Multiprocessor Format
                              (Sending Data H'AA to Receiving Processor A)

Communication Formats: Four formats are available. Parity-bit settings are ignored when the
multiprocessor format is selected. For details see table 13.8.

Clock: See the description in the asynchronous mode section.

Transmitting Multiprocessor Serial Data: Figure 13.10 shows a sample flowchart for
transmitting multiprocessor serial data. The procedure for transmitting multiprocessor serial data is
as follows:

1.   SCI status check and transmit data write: read the serial status register (SSR), check that the
    TDRE bit is 1, then write transmit data in the transmit data register (TDR). Also set the MPBT
    (multiprocessor bit transfer) bit to 0 or 1 in SSR. Finally, clear TDRE to 0.
2.   To continue transmitting serial data, read the TDRE bit to check whether it is safe to write (if it
    reads 1); if so, write data in TDR, then clear TDRE to 0. When the DMAC is started by a
    transmit-data-empty interrupt request (TXI) to write data in TDR, the TDRE bit is checked and
    cleared automatically.






                                                                                                                     365
```

## PDF page 382

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=382)

```text
                                           Start transmission


                                        Read TDRE bit in SSR                          1


                                                TDRE = 1?                   No


                                                       Yes
                                      Write transmit data to TDR
                                       and set MPBT bit in SSR


                                          Clear TDRE bit to 0


                                         Transmission ended?                 No       2


                                                 Yes
                                        Read TEND bit in SSR


                                                TEND = 1?                   No

                                                       Yes
                                            End transmission

  Note:   Circled numbers refer to the preceding description of the procedure in the text.

         Figure 13.10   Sample Flowchart for Transmitting Multiprocessor Serial Data

In transmitting serial data, the SCI operates as follows:

1.   The SCI monitors the TDRE bit in SSR. When TDRE is cleared to 0 the SCI recognizes that
    the transmit data register (TDR) contains new data, and loads this data from TDR into the
    transmit shift register (TSR).

2.   After loading the data from TDR into TSR, the SCI sets the TDRE bit to 1 and starts
    transmitting. If the transmit-data-empty interrupt enable bit (TIE) in SCR is set to 1, the SCI
    requests a transmit-data-empty interrupt (TXI) at this time.
    Serial transmit data is transmitted in the following order from the TxD pin:
    a.   Start bit: one 0 bit is output.
    b.   Transmit data: seven or eight bits are output, LSB first.
    c.   Multiprocessor bit: one multiprocessor bit (MPBT value) is output.
    d.   Stop bit: one or two 1-bits (stop bits) are output.
    e.   Mark state: output of 1-bits continues until the start bit of the next transmit data.

366
```

## PDF page 383

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=383)

```text
3.   The SCI checks the TDRE bit when it outputs the stop bit. If TDRE is 0, the SCI loads data
    from TDR into TSR, outputs the stop bit, then begins serial transmission of the next frame. If
    TDRE is 1, the SCI sets the TEND bit in SSR to 1, outputs the stop bit, then continues output
    of 1-bits in the mark state. If the transmit-end interrupt enable bit (TEIE) in SCR is set to 1, a
    transmit-end interrupt (TEI) is requested at this time.


                                              Multi-                                   Multi-
                                           processor                                processor
                   Start                        bit  Stop   Start                        bit
            1  1bit                 Data              bit    bit                 Data                Stopbit
    Serial          01D0    D1         D7                              D0    D1         D7    0/10/1             0                                             1Idle
      data                                                                                              (mark)
                                                                                                         state

    TDRE


    TEND


     TXI interrupt      TXI interrupt       TXI interrupt                                TEI interrupt
        request        handler writes         request                                      request
       generated       data to TDR           generated                                    generated
                        and clears
                      TDRE bit to 0
                                 1 frame

                Figure 13.11   Example of SCI Multiprocessor Transmit Operation
                        (8-Bit Data with Multiprocessor Bit and One Stop Bit)

Receiving Multiprocessor Serial Data: Figure 13.12 shows a sample flowchart for receiving
multiprocessor serial data. The procedure for receiving multiprocessor serial data is as follows.

1.   ID receive cycle: set the MPIE bit in the serial control register (SCR) to 1.
2.   SCI status check, ID reception and comparison: read the serial status register (SSR), check that
    RDRF is set to 1, then read data from the receive data register (RDR) and compare with the
    processor’s own ID. If the ID does not match the receive data, set MPIE to 1 again and clear
    RDRF to 0. If the ID matches the receive data, clear RDRF to 0.
3.   Receive error handling: if a receive error occurs (figure 13.12 (cont)), read the ORER and FER
    bits in SSR to identify the error. After executing the necessary error handling, clear both
    ORER and FER to 0. Receiving cannot resume if ORER or FER remains set to 1. When a
    framing error occurs, the RxD pin can be read to detect the break state.
4.   SCI status check and data receiving: read SSR, check that RDRF is set to 1, then read data
    from the receive data register (RDR).


                                                                                                                367
```

## PDF page 384

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=384)

```text
                                    Start reception


                               Set MPIE bit in SCR to 1            1

                                Read ORER and FER
                                       bits in SSR

                                FER = 1 or ORER = 1?              Yes

                                                 No
                                Read RDRF bit in SSR               2

                     No               RDRF = 1?

                                                Yes
                              Read receive data in RDR

                     No                 Is ID this
                                    processor’s ID?
                                       Yes
                                Read ORER and FER
                                       bits in SSR

                                FER = 1 or ORER = 1?              Yes

                                                 No
                                Read RDRF bit in SSR               4

                                      RDRF = 1?                    No

                                                Yes
                              Read receive data in RDR

                    No             All data received?                                3

                                                Yes                        Error handling

                               Clear RE bit in SCR to 0

                                     End reception


Note:   Circled numbers refer to the preceding description of the procedure in the text.

           Figure 13.12   Sample Flowchart for Receiving Multiprocessor Serial Data

368
```

## PDF page 385

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=385)

```text
                                            Error handling


                          No                 ORER = 1?


                                                    Yes

                                       Overrun error handling



                          No                  FER = 1?


                                                      Yes

                                       Framing error handling




                                          Clear ORER and
                                        FER bits in SSR to 0


                                                 End

Figure 13.12   Sample Flowchart for Receiving Multiprocessor Serial Data (cont)




























                                                                                                    369
```

## PDF page 386

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=386)

```text
Figure 13.13 shows an example of SCI receive operation using a multiprocessor format.


               Start  Data                Stop  Start  Data
            1  1bit   (ID1)          MPB   bit   bit                                 MPB Stop(data 1)bit
    Serial      01D0    D1         D7                              D0    D1         D7      010  1Idle
     data                                                                          (mark)
                                                                                     state

    MPB


    MPIE


   RDRF


    RDR                                                             ID1
    value


      RXI interrupt request   RXI interrupt handler       ID is not this      No RXI
         (multiprocessor        reads RDR data          processor’s ID,       interrupt
            interrupt)          and clears RDRF          so MPIE bit is      generated;
      generated, MPIE = 0            bit to 0            set to 1 again      RDR state
                                                                               is held

                      Figure 13.13   Example of SCI Receive Operation
   (Own ID Does Not Match Data, 8-Bit Data with Multiprocessor Bit and One Stop Bit)



























370
```

## PDF page 387

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=387)

```text
                 Start    Data                  Stop  Start    Data                  Stop
             1  1bit     (ID2)           MPB                                                 MPBbitbit(Data 2)bit
    Serial         01D0    D1         D7                              D0    D1         D7      010  1Idling
     data                                                                                 (marking)


     MPB


    MPIE


   RDRF


     RDR                  ID1                                              ID2                    Data2
    value


                  RXI interrupt          RXI interrupt               ID is that of           MPIE
                     request                handler                 this processor,       bit set to 1
                 (multiprocessor       reads RDR data                so reception            again
                    interrupt)             and clears           continues unchanged
                   generated,            RDRF bit to 0           and data is received
                    MPIE = 0                                     by the RXI interrupt
                                                                       handler

                        Figure 13.13   Example of SCI Receive Operation
    (Own ID Matches Data, 8-Bit Data with Multiprocessor Bit and One Stop Bit) (cont)


13.3.4      Clocked Synchronous Operation

In clocked synchronous mode, the SCI transmits and receives data in synchronization with clock
pulses. This mode is suitable for high-speed serial communication.

The SCI transmitter and receiver are independent, so full duplex communication is possible while
sharing the same clock. The transmitter and receiver are also double buffered, so continuous
transmitting or receiving is possible by reading or writing data while transmitting or receiving is in
progress.

Figure 13.14 shows the general format in clocked synchronous serial communication.











                                                                                                      371
```

## PDF page 388

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=388)

```text
                      Transfer direction
                                   One unit of communication data (character or frame)

                       **
        Serial clock

                                LSB                                                                            MSB
        Serial data             Bit 0     Bit 1     Bit 2     Bit 3     Bit 4     Bit 5     Bit 6     Bit 7



  Note:   High except in continuous transmitting or receiving.

               Figure 13.14   Data Format in Clocked Synchronous Communication

In clocked synchronous serial communication, each data bit is output on the communication line
from one falling edge of the serial clock to the next. Data is guaranteed valid at the rising edge of
the serial clock. In each character, the serial data bits are transmitted in order from LSB (first) to
MSB (last). After output of the MSB, the communication line remains in the state of the MSB. In
clocked synchronous mode, the SCI transmits or receives data by synchronizing with the falling
edge of the serial clock.

Communication Format: The data length is fixed at eight bits. No parity bit or multiprocessor bit
can be added.

Clock: An internal clock generated by the built-in baud rate generator or an external clock input
from the SCK pin can be selected as the SCI transmit/receive clock. The clock source is selected
by the C/A bit in the serial mode register (SMR) and bits CKE1 and CKE0 in the serial control
register (SCR). See table 13.9.

When the SCI operates on an internal clock, it outputs the clock signal at the SCK pin. Eight clock
pulses are output per transmitted or received character. When the SCI is not transmitting or
receiving, the clock signal remains in the high state.

Figure 13.15 shows an example of SCI transmit operation. In transmitting serial data, the SCI
operates as follows.

1.   The SCI monitors the TDRE bit in SSR. When TDRE is cleared to 0 the SCI recognizes that
    the transmit data register (TDR) contains new data and loads this data from TDR into the
    transmit shift register (TSR).
2.   After loading the data from TDR into TSR, the SCI sets the TDRE bit to 1 and starts
    transmitting. If the transmit-data-empty interrupt enable bit (TIE) in SCR is set to 1, the SCI
    requests a transmit-data-empty interrupt (TXI) at this time.
    If clock output mode is selected, the SCI outputs eight synchronous clock pulses. If an external
    clock source is selected, the SCI outputs data in synchronization with the input clock. Data is
    output from the TxD pin in order from LSB (bit 0) to MSB (bit 7).
372
```

## PDF page 389

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=389)

```text
3.   The SCI checks the TDRE bit when it outputs the MSB (bit 7). If TDRE is 0, the SCI loads
    data from TDR into TSR, transmits the MSB, then begins serial transmission of the next frame.
    If TDRE is 1, the SCI sets the TEND bit in SSR to 1, transmits the MSB, then holds the
    transmit data pin (TxD) in the MSB state. If the transmit-end interrupt enable bit (TEIE) in
    SCR is set to 1, a transmit-end interrupt (TEI) is requested at this time.
4.   After the end of serial transmission, the SCK pin is held in the high state.


                     Transfer direction

       Serial clock


                                  LSB                             MSB
        Serial data              Bit 0     Bit 1                 Bit 7     Bit 0     Bit 1                 Bit 6Bit 7


              TDRE


              TEND

              TXI interrupt           TXI interrupt         TXI interrupt                           TEI interrupt
                 request         handler writes data          request                                  request
                generated         to TDR and clears          generated                               generated
                                     TDRE bit to 0

                                             1 frame

                           Figure 13.15   Example of SCI Transmit Operation

Transmitting and Receiving Data

SCI Initialization (Clocked Synchronous Mode): Before transmitting or receiving, software
must clear the TE and RE bits to 0 in the serial control register (SCR), then initialize the SCI as
follows.

When changing the mode or communication format, always clear the TE and RE bits to 0 before
following the procedure given below. Clearing TE to 0 sets TDRE to 1 and initializes the transmit
shift register (TSR). Clearing RE to 0, however, does not initialize the RDRF, PER, FER, and
ORER flags and receive data register (RDR), which retain their previous contents.

Figure 13.16 shows a sample flowchart for initializing the SCI. The procedure for initializing the
SCI is as follows.

1.   Select the communication format in the serial mode register (SMR).
2.   Write the value corresponding to the bit rate in the bit rate register (BRR) unless an external
    clock is used.
                                                                                                                       373
```

## PDF page 390

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=390)

```text
3.   Select the clock source in the serial control register (SCR). Leave RIE, TIE, TEIE, MPIE, TE
    and RE cleared to 0.
4.   Wait for at least the interval required to transmit or receive one bit, then set TE or RE in the
    serial control register (SCR) to 1. Also set RIE, TIE, TEIE and MPIE.



                                                  Initialization


                                     Clear TE and RE bits in SCR to 0


                                    Set transmit/receive format in SMR              1


                                               Set value in BRR                     2


                                     Set RIE, TIE, TEIE, MPIE, CKE1,
                                            and CKE0 bits in SCR                    3
                                              (TE and RE are 0)
                                                                   Wait

                                                   Has a 1-bit                  No
                                                period elapsed?

                                                           Yes
                                    Set TE and RE bits in SCR to 1 and              4
                                     set RIE, TIE, TEIE, and MPIE bits


                                                       End


  Note:   Circled numbers refer to the preceding description of the procedure in the text.

                         Figure 13.16   Sample Flowchart for SCI Initialization

Transmitting Serial Data (Clocked Synchronous Mode): Figure 13.17 shows a sample
flowchart for transmitting serial data. The procedure for transmitting serial data is as follows.

1.   SCI status check and transmit data write: read the serial status register (SSR), check that the
    TDRE bit is 1, then write transmit data in the transmit data register (TDR) and clear TDRE
    to 0.
2.   To continue transmitting serial data, read the TDRE bit to check whether it is safe to write (if it
    reads 1); if so, write data in TDR, then clear TDRE to 0. When the DMAC is started by a
    transmit-data-empty interrupt request (TXI) to write data in TDR, the TDRE bit is checked and
    cleared automatically.
374
```

## PDF page 391

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=391)

```text
                                           Start transmission


                                         Read TDRE bit in SSR                       1


                                                TDRE = 1?                 No


                                                        Yes
                                   Write transmssion data to TDR
                                    and clear TDRE bit in SSR to 0


                                          All data transmitted?           No        2


                                                        Yes

                                         Read TEND bit in SSR


                                                TEND = 1?                 No


                                                        Yes
                                        Clear TE bit in SCR to 0


                                            End transmission


Note:   Circled numbers refer to the preceding description of the procedure in the text.

                       Figure 13.17   Sample Flowchart for Serial Transmitting

Receiving Serial Data (Clocked Synchronous Mode): Figure 13.18 shows a sample flowchart
for receiving serial data. When switching from asynchronous mode to clocked synchronous mode,
make sure that ORER, PER, and FER are cleared to 0. If PER or FER is set to 1, the RDRF bit
will not be set and both transmitting and receiving will be disabled. Figure 13.19 shows an
example of the SCI receive operation.

The procedure for receiving serial data is as follows:

1.   Receive error handling: if a receive error occurs, read the ORER bit in SSR to identify the
    error. After executing the necessary error handling, clear ORER to 0. Transmitting/receiving
    cannot resume if ORER remains set to 1.




                                                                                                                  375
```

## PDF page 392

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=392)

```text
2.   SCI status check and receive data read: read the serial status register (SSR), check that RDRF
    is set to 1, then read receive data from the receive data register (RDR) and clear RDRF to 0.
    The RXI interrupt can also be used to determine if the RDRF bit has changed from 0 to 1.
3.   To continue receiving serial data: read RDR, and clear RDRF to 0 before the MSB (bit 7) of
    the current frame is received. If the DMAC is started by a receive-data-full interrupt (RXI) to
    read RDR, the RDRF bit is cleared automatically so this step is unnecessary.



                                  Start reception


                            Read ORER bit in SSR


                                   ORER = 1?                  Yes
                                                                                        1

                                            No                              Error processing

                             Read RDRF bit in SSR                 2


                    No              RDRF = 1?


                                            Yes
                        Read receive data in RDR and              3
                          clear RDRF bit in SSR to 0


                   No           All data received?


                                            Yes
                            Clear RE bit in SCR to 0


                                  End reception


Note:   Circled numbers refer to the preceding description of the procedure in the text.

                         Figure 13.18   Sample Flowchart for Serial Receiving










376
```

## PDF page 393

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=393)

```text
                                                Error handling


                               No                ORER = 1?


                                                          Yes
                                           Overrun error handling



                                        Clear ORER bit in SSR to 0


                                                      End

              Figure 13.18   Sample Flowchart for Serial Receiving (cont)


                Transfer direction

Serial clock


      Serial         Bit 7     Bit 0                 Bit 7     Bit 0     Bit 1                 Bit 6Bit 7
        data


      RDRF


     ORER

           RXI interrupt      RXI interrupt             RXI interrupt                      ERI interrupt
             request         handler reads                 request                            request
            generated       data and clears              generated                          generated
                             RDRF bit to 0                                                  by overrun
                                                                                                error
                                     1 frame

                     Figure 13.19   Example of SCI Receive Operation












                                                                                                              377
```

## PDF page 394

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=394)

```text
In receiving, the SCI operates as follows:

1.   The SCI synchronizes with serial clock input or output and initializes internally.
2.   Receive data is shifted into RSR in order from LSB to MSB. After receiving the data, the SCI
    checks that RDRF is 0 so that receive data can be loaded from RSR into RDR. If this check
    passes, the SCI sets RDRF to 1 and stores the received data in RDR. If the check does not pass
    (receive error), the SCI operates as indicated in table 13.8. The RDRF bit is not set to 1. Be
    sure to clear the error flag.
3.   After setting RDRF to 1, if the receive-data-full interrupt enable bit (RIE) is set to 1 in SCR,
    the SCI requests a receive-data-full interrupt (RXI). If the ORER bit is set to 1 and the receive-
    data-full interrupt enable bit (RIE) in SCR is also set to 1, the SCI requests a receive-error
    interrupt (ERI).

Transmitting and Receiving Serial Data Simultaneously (Clocked Synchronous Mode):
Figure 13.20 shows a sample flowchart for transmitting and receiving serial data simultaneously.
The procedure for transmitting and receiving serial data simultaneously is as follows:

1.   SCI status check and transmit data write: read the serial status register (SSR), check that the
    TDRE bit is 1, then write transmit data in the transmit data register (TDR) and clear TDRE to
    0. The TXI interrupt can also be used to determine if the TDRE bit has changed from 0 to 1.
2.   Receive error handling: if a receive error occurs, read the ORER bit in SSR to identify the
    error. After executing the necessary error handling, clear ORER to 0. Transmitting/receiving
    cannot resume if ORER remains set to 1.
3.   SCI status check and receive data read: read the serial status register (SSR), check that RDRF
    is set to 1, then read receive data from the receive data register (RDR) and clear RDRF to 0.
    The RXI interrupt can also be used to determine if the RDRF bit has changed from 0 to 1.
4.   To continue transmitting and receiving serial data: read the RDRF bit and RDR, and clear
    RDRF to 0 before the MSB (bit 7) of the current frame is received. Also read the TDRE bit to
    check whether it is safe to write (if it reads 1); if so, write data in TDR, then clear TDRE to 0
    before the MSB (bit 7) of the current frame is transmitted. When the DMAC is started by a
    transmit-data-empty interrupt request (TXI) to write data in TDR, the TDRE bit is checked and
    cleared automatically. When the DMAC is started by a receive-data-full interrupt (RXI) to read
    RDR, the RDRF bit is cleared automatically.















378
```

## PDF page 395

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=395)

```text
                         Start transmission/reception


                            Read TDRE bit in SSR                 1


                    No             TDRE = 1?


                                            Yes
                           Write transmit data to TDR
                        and clear TDRE bit in SSR to 0


                            Read ORER bit in SSR


                                   ORER = 1?                  Yes
                                                                                      2

                                            No                             Error handling


                            Read RDRF bit in SSR                 3


                 No                RDRF = 1?


                                            Yes
                          Read receive data in RDR               4
                       and clear RDRF bit in SSR to 0

                   No                All data
                             transmitted/received?

                                            Yes
                              Clear TE and RE bits
                                   in SCR to 0

                          End transmission/reception

Note:   When switching from transmitting or receiving to simultaneous transmitting and receiving,
        clear both TE and RE to 0, then set both TE and RE to 1.
        Circled numbers refer to the preceding description of the procedure in the text.

                    Figure 13.20   Sample Flowchart for Serial Transmitting





                                                                                                              379
```

## PDF page 396

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=396)

```text
13.4      SCI Interrupt Sources and the DMAC

The SCI has four interrupt sources in each channel: transmit-end (TEI), receive-error (ERI),
receive-data-full (RXI), and transmit-data-empty (TXI). Table 13.13 lists the interrupt sources and
indicates their priority. These interrupts can be enabled and disabled by the TIE, RIE, and TEIE
bits in the serial control register (SCR). Each interrupt request is sent separately to the interrupt
controller.

TXI is requested when the TDRE bit in SSR is set to 1. TXI can start the direct memory access
controller (DMAC) to transfer data. TDRE is automatically cleared to 0 when the DMAC writes
data in the transmit data register (TDR).

RXI is requested when the RDRF bit in SSR is set to 1. RXI can start the DMAC to transfer data.
RDRF is automatically cleared to 0 when the DMAC reads the receive data register (RDR).

ERI is requested when the ORER, PER, or FER bit in SSR is set to 1. ERI cannot start the DMAC.

TEI is requested when the TEND bit in SSR is set to 1. TEI cannot start the DMAC. Where the
TXI interrupt indicates that transmit data writing is enabled, the TEI interrupt indicates that the
transmit operation is complete.

Table 13.13    SCI Interrupt Sources

Interrupt Source          Description                                               DMAC Availability        Priority
ERI                                Receive error (ORER, PER, or FER)        No                                     High
RXI                                Receive data register full (RDRF)             Yes                                          ›
TXI                                Transmit data register empty (TDRE)       Yes                                          ﬂ
TEI                                Transmit end (TEND)                                No                                     Low


See section 4, Exception Handling, for information on the priority order and relationship to non-
SCI interrupts.

13.5      Usage Notes

Note the following points when using the SCI.

TDR Write and TDRE Flag: The TDRE bit in the serial status register (SSR) is a status flag
indicating loading of transmit data from TDR into TSR. The SCI sets TDRE to 1 when it transfers
data from TDR to TSR. Data can be written to TDR regardless of the TDRE bit status. If new data
is written in TDR when TDRE is 0, however, the old data stored in TDR will be lost because the
data has not yet been transferred to TSR. Before writing transmit data to TDR, be sure to check
that TDRE is set to 1.


380
```

## PDF page 397

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=397)

```text
Simultaneous Multiple Receive Errors: Table 13.14 indicates the state of the SSR status flags
when multiple receive errors occur simultaneously. When an overrun error occurs, the RSR
contents cannot be transferred to RDR, so receive data is lost.

Table 13.14    SSR Status Flags and Transfer of Receive Data


                                                                   SSR Status Flags                   Receive DataTransfer
Receive Error Status                            RDRF            ORER    FER         PER       RSR ﬁ   RDR
Overrun error                                          1                    1            0              0            X
Framing error                                         0                    0            1              0            O
Parity error                                             0                    0            0              1            O
Overrun error + framing error                 1                    1            1              0            X
Overrun error + parity error                    1                    1            0              1            X
Framing error + parity error                    0                    0            1              1            O
Overrun error + framing error + parity              1111X
error
O: Receive data is transferred from RSR to RDR.
X: Receive data is not transferred from RSR to RDR.


Break Detection and Processing: In the break state, the input from the RxD pin consists of all 0s,
so FER is set and the parity error flag (PER) may also be set. In the break state, the SCI receiver
continues to operate, so if the FER bit is cleared to 0, it will be set to 1 again.

Receive Error Flags and Transmitter Operation (Clocked Synchronous Mode Only): When a
receive error flag (ORER, PER, or FER) is set to 1, the SCI will not start transmitting even if
TDRE is set to 1. Be sure to clear the receive error flags to 0 before starting to transmit. Note that
clearing RE to 0 does not clear the receive error flags.

Receive Data Sampling Timing and Receive Margin in Asynchronous Mode: In asynchronous
mode, the SCI operates on a base clock of 16 times the bit rate frequency. In receiving, the SCI
synchronizes internally with the falling edge of the start bit, which it samples on the base clock.
Receive data is latched on the rising edge of the eighth base clock pulse. See figure 13.21.













                                                                                                                              381
```

## PDF page 398

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=398)

```text
                                       16 clocks
                              8 clocks
                CLK      0  1  2  3  4  5  6   7  8  9 101112131415 0  1  2  3  4  5  6   7  8  9 101112131415 0  1  2  3  4  5

                                                 –7.5 clocks     +7.5 clocks
           Receive            Start bit                                         D0                                     D1
        data (RxD)

 Synchronization
          sampling
              timing

               Data
          sampling
              timing

               Figure 13.21   Receive Data Sampling Timing in Asynchronous Mode

The receive margin in asynchronous mode can therefore be expressed as in equation 1.

Equation 1:

           M =  0.5 –  1           – 1                      F  –   D –  0.5      (1  +  F)   ×  100%
                                – L – 0.5(      )
                             2N                                        N

             M :   Receive margin (%)
             N  :   Ratio of clock frequency to bit rate (N = 16)
             D  :   Clock duty cycle (D = 0–1.0)
             L  :   Frame length (L = 9–12)
             F  :   Absolute deviation of clock frequency

From equation (1), if F = 0 and D = 0.5 the receive margin is 46.875%, as given by equation 2.

Equation 2:

           D   = 0.5, F = 0
           M   = (0.5 – 1/(2 · 16)) · 100%
                = 46.875%

This is a theoretical value. A reasonable margin to allow in system designs is 20–30%.











382
```

## PDF page 399

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=399)

```text
Constraints on DMAC Use:

•    When using an external clock source for the serial clock, update TDR with the DMAC, and
    then after twenty system clock cycles or more elapse, input a transmit clock. If a transmit clock
    is input in the first four states after TDR is written, an error may occur (figure 13.22).
•    Before reading the receive data register (RDR) with the DMAC, select the receive-data-full
    interrupt of the SCI as an activation source using the resource select bit (RS) in the channel
    control register (CHCR).



        SCK
                    t


      TDRE



                            D0          D1         D2         D3         D4         D5         D6          D7


  Note:   During external clock operation, an error may occur if t is 4 states or less.

           Figure 13.22   Example of Clocked Synchronous Transmission with DMAC

Cautions for Clocked Synchronous External Clock Mode:

•    Set TE = RE = 1 only when external clock SCK is 1.
•    Do not set TE = RE = 1 until at least four clock cycles after external clock SCK has changed
    from 0 to 1.
•    When receiving, RDRF is set to 1 when RE is cleared to 0 2.5–3.5 clocks after the rising edge
    of the RxD D7 bit SCK input, but it cannot be copied to RDR.

Caution for Clocked Synchronous Internal Clock Mode: When receiving, RDRF is set to 1
when RE is cleared to 0 1.5 clocks after the rising edge of the RxD D7 bit SCK output, but it
cannot be copied to RDR.














                                                                                                                     383
```

## PDF page 400

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=400)

```text
384
```
