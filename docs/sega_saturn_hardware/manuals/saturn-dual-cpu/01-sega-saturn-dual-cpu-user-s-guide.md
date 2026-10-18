# SEGA Saturn Dual CPU User's Guide

SEGA Saturn Dual CPU User’s Guide — SEGA ST-202-R1-120994, December 9, 1994.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=2)

Physical PDF pages 2–12. Text extraction; consult the PDF for figures.

## Bookmarks

- [Contents](#pdf-page-5)
- [1.0 Dual CPU Configuration in Sega Saturn](#pdf-page-6)
- [1.1 Complete Shared Mode (Memory Sharing)](#pdf-page-6)
- [1.2 Communication by FRT Input Capture Signal](#pdf-page-6)
- [2.0 Settings for Debugging Environment](#pdf-page-7)
- [2.1 E7000 ICE Settings](#pdf-page-7)
- [2.2 EVA Board Settings](#pdf-page-7)
- [3.0 Clock Change](#pdf-page-7)
- [4.0 Programming](#pdf-page-8)
- [4.1 Program Sharing](#pdf-page-8)
- [4.2 Stack Variables and Static and Global Variables](#pdf-page-8)
- [4.3 Slave CPU Initialization](#pdf-page-8)
- [4.4 Initialization (Vector, Stack) by the Boot ROM](#pdf-page-8)
- [5.0 Communication from Master CPU to Slave CPU](#pdf-page-9)
- [5.1 FRT Input Capture Interrupt](#pdf-page-9)
- [5.2 Polling of the FRT Input Capture Flag](#pdf-page-9)
- [6.0 Communication from Slave CPU to Master CPU](#pdf-page-10)
- [6.1 FRT Input Capture Interrupt](#pdf-page-10)
- [6.2 Polling of the FRT Input Capture Flag](#pdf-page-10)
- [7.0 Data Transfer Between Master and Slave CPUs](#pdf-page-10)
- [7.1 Cache-through Read](#pdf-page-10)
- [7.2 Cache InValidate](#pdf-page-10)
- [8.0 Programming Examples Using the Slave CPU](#pdf-page-11)
- [8.1 Slave CPU Initialization](#pdf-page-11)
- [8.2 Function Execution Request from Master CPU to Slave CPU](#pdf-page-11)
- [Virtual CD Tool (September 27, 1994 Version)](#pdf-page-12)

## PDF page 2

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=2)

```text
               TM















                                   SEGA Saturn
                                      Dual CPU
                                     User's Guide

                                      Doc. # ST-202-R1-120994
           SEGA Confidential
                                       © 1994 SEGA.  All Rights Reserved.
```

## PDF page 3

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=3)

```text
                                   READER CORRECTION/COMMENT SHEET


          Keep us updated!
           If you should come across any incorrect or outdated information while reading through the attached
          document, or come up with any questions or comments, please let us know so that we can make the
          required changes in subsequent revisions.  Simply fill out all information below and return this form to
          the Developer Technical Support Manager at the address below.   Please make more copies of this form if
          more space is needed.  Thank you.

          General Information:
          Your Name                                                                                                              Phone

          Document number                                                                                                 DateST-202-R1-120994

          Document name           SEGA Saturn Dual CPU User's Guide

          Corrections:

           Chpt.    pg. #     Correction























          Questions/comments:






                                                   Where to send your corrections:

                        Fax:       (415) 802-3963                             Mail:     SEGA OF AMERICA
                                  Attn:  Manager,                                       Attn:  Manager,
                                  Developer Technical Support                           Developer Technical Support
                                                                                        275 Shoreline Dr.  Ste 500
                        SEGA ConfidentialRedwood City, CA  94065
```

## PDF page 4

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=4)

```text
Update History


                    Date                   Version                                             Update History
          August 5, 1994                     1.0
          September 28, 1994             1.1         Correction of typographical errors in address values for 2.2 EVA
                                                    board settings
           December 9, 1994                               1.2         Corrected translated error on page 5.  Address 28000000H under
                                                    section 6.1 was changed to 21800000H.





                  SEGA Confidential
```

## PDF page 5

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=5)

```text
                                                                  Contents




1.0   Dual CPU Configuration in Sega Saturn......................... 1
           1.1   Complete Shared Mode (Memory Sharing ............ 1
           1.2   Communication by FRT Input Capture Signal ....... 1

2.0   Settings for Debugging Environment ............................. 2
           2.1   E7000 ICE Settings ............................................... 2
           2.2   EVA Board Settings ............................................... 2

3.0   Clock Changes ..................................................................2

4.0   Programming .....................................................................3
           4.1   Program Sharing ................................................... 3
           4.2   Stack Variables and Static and Global Variables ... 3
           4.3   Slave CPU Initialization ......................................... 3
           4.4   Initialization (Vector, Stack) by the Boot ROM ....... 3

5.0   Communication from Master CPU to Slave CPU ........... 4
           5.1   FRT Input Capture Interrupt ................................... 4
           5.2   Polling of the FRT Input Capture Flag ................... 4

6.0   Communication from Slave CPU to Master CPU ........... 5
           6.1   FRT Input Capture Interrupt ................................... 5
           6.2   Polling of the FRT Input Capture Flag ................... 5

7.0   Data Transfer Between Master and Slave CPUs ............ 5
           7.1   Cache-through Read ............................................. 5
           7.2   Cache Invalidate .................................................... 5

8.0   Programming Examples Using the Slave CPU .............. 6
           8.2   Function Execution Request from Master CPU
                  to Slave CPU ......................................................... 6

Virtual CD Tool...........................................................................7

                    SEGA Confidential
```

## PDF page 6

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=6)

```text
1.0    Dual CPU Configuration in Sega Saturn

1.1      Complete Shared Mode (Memory Sharing)
In the Sega Saturn, the slave CPU shares all external devices with the master CPU.
The programs executed by the slave CPU and the reset vector obtained during reset
are exactly the same as those of the master CPU.  However, identification of the
master and slave CPUs is performed in the boot ROM because each CPU must be
initialized separately.
When the slave and master CPUs compete for external access, one of the CPUs is
forced to wait for access and execution speed decreases.

1.2      Communication by FRT Input Capture Signal
The Sega Saturn is designed so that the FRT Input Capture signal of the SH2 built-in
module can be used to implement communication between the master and slave
CPUs.  Specifically, to input the FRT Input Capture signal to the slave CPU, write
16-bit data to address 21000000H; to input the FRT Input Capture signal to the mas-
ter CPU, write 16-bit data to address 21800000H.



   SEGA Confidential


 SEGA Saturn Dual CPU User's Guide                                                                                                         1
```

## PDF page 7

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=7)

```text
    2.0    Settings for Debugging Environment

    2.1      E7000 ICE Settings
    To use the E7000 ICE in the slave CPU, slave mode must be specified with the ICE
    MODE command.

    Setting Procedure
    :mode;c [Return]
    E7000 MODE(MD5-0)=xx? 2E [Return]
    MODE SET (C:CONFIGURATION/U:USER/M:MASTER-SLAVE)=X? C [Return]
    CONFIGURATION WRITE OK?(Y/N)? Y [Return]

    2.2      EVA Board Settings
    To use the EVA board in the slave CPU, the following settings are required:

    1.   Release the slave CPU reset in the master CPU emulator.

    Release Procedure
        :m 2010001f:b [Return]
        2010001f   xx        ? 02 [Return]
        20100020  xx        ? . [Return]

    2.   The EVA board in the slave CPU is started.
    3.   Execute em ct=d on the EVA board in the slave CPU.

    When using the EVA board in the slave CPU, note that the EVA board cannot be
    operated until the slave CPU reset is released.


    3.0    Clock Change

    When the clock is changed, the SMPC switches the slave CPU to reset state. The
    slave CPU must therefore be restarted by the SMPC command (SSH_ON = 02H) after
    the clock is changed.
                   SEGA Confidential

      2
```

## PDF page 8

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=8)

```text
4.0    Programming

4.1      Program Sharing
Because the slave and master CPUs share all external devices, either CPU can ex-
ecute an execution file from any location.  (If the cache is used as internal RAM,
programs placed in internal RAM are not shared.)

4.2     Stack Variables and Static and Global Variables
Re-entrant functions can be executed concurrently for the slave and master CPUs
because a stack area is allocated separately for each CPU (boot ROM setting).  How-
ever, if both CPUs execute programs that rewrite static or global variables, special
attention must be paid to the execution sequence and to concurrent execution in
each CPU.

4.3      Slave CPU Initialization
In slave CPU activation by the master CPU, the boot ROM rewrites the execution
entry (SYS_SETSINT(164H, EntryFunc);) after read initialization.  The master CPU
then uses the SMPC command (SSH_ON = 02H) to release the slave reset.
(EntryFunc is the slave entry function).

4.4     Initialization (Vector, Stack) by the Boot ROM
The boot ROM initializes the slave CPU as follows:
1.   Vector VBR is set to address 6000400H.
2.   Stack SP is set to address 6001000H.
3.   Of the interrupts, only the FRT Input Capture interrupt is enabled (64H, level 1).


   SEGA Confidential


SEGA Saturn Dual CPU User's Guide                                                                                                         3
```

## PDF page 9

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=9)

```text
     5.0    Communication from Master CPU to Slave CPU

     Bidirectional communication is necessary for coordinating operation between the
     master and slave CPUs.  For communication from the master CPU to the slave CPU,
     the Sega Saturn provides the methods described in the following sections.

     5.1      FRT Input Capture Interrupt
     During boot ROM initialization, the FRT Input Capture interrupt is set for the slave
     CPU.  To set interrupt processing from the master CPU, use the application initial-
     ization routine to register the interrupt processing routine to interrupt vector 64H of
     the slave CPU.  (When the interrupt processing routine is registered from the master
     PCU, the interrupt vector is 164H.)

     To issue this interrupt from the master CPU, write 16-bit data to address 21000000H.
     The slave CPU then starts the interrupt processing that was previously registered.

     5.2      Polling of the FRT Input Capture Flag
     When the SH CPU accepts the FRT Input Capture signal, it sets a flag to an internal
     register in the FRT unit.  The SH CPU then monitors change in the flag during pro-
     cessing wait state (or synchronous wait state).  This method is especially effective
     when there are many synchronous or processing waits because it has a shorter
     reception time than the interrupt processing method.  When using this method, be
     sure to disable the FRT Input Capture interrupt in the application initialization
     routine.  (Set 01H to TIER.)


                    SEGA Confidential

      4
```

## PDF page 10

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=10)

```text
6.0    Communication from Slave CPU to Master CPU

In addition to providing communication methods from the master CPU to the slave
CPU, the Sega Saturn also provides all of the same communication methods for
communication from the slave CPU to the master CPU.

6.1      FRT Input Capture Interrupt
During boot ROM initialization, the FRT Input Capture interrupt is set for the mas-
ter CPU.  To set interrupt processing from the slave CPU, set up the application
initialization routine to register the interrupt processing routine to interrupt vector
64H of the master CPU.
To issue this interrupt from the slave CPU, write 16-bit data to address 21800000H.
The slave CPU then starts the interrupt processing that was previously registered.

6.2      Polling of the FRT Input Capture Flag
When the SH CPU accepts the FRT Input Capture signal, it sets a flag to an internal
register in the FRT unit.  The SH CPU then monitors change in the flag during pro-
cessing wait state (or synchronous wait state).  This method is especially effective
when there are many synchronous or processing waits because it has a shorter
reception time than the interrupt processing method.  When using this method, be
sure to disable the FRT Input Capture interrupt in the application initialization
routine.


7.0    Data Transfer Between Master and Slave CPUs

The cache unit of the SH CPU does not support a bus snoop function.  Therefore
when data is transferred between the master and slave CPUs, the CPU reading the
data must either perform a cache-through read or read the data after the cache of the
target area is invalidated.

7.1      Cache-through Read
In cache-through read, the CPU reads data starting from the address obtained when
20000000H is added to the target address.

7.2     Cache InValidate
To invalidate the cache, the CPU writes 0 by 16-bit access to the address obtained
when 40000000H is added to the target address.  This operation invalidates a 16-byte
area that includes the target address.
   SEGA Confidential


 SEGA Saturn Dual CPU User's Guide                                                                                                          5
```

## PDF page 11

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=11)

```text
    8.0    Programming Examples Using the Slave CPU

    The following sections shows program examples that use the slave CPU
    (when the FRT Input Capture Flag is polled.)

    8.1      Slave CPU Initialization
    The following examples show a program that initializes the slave CPU and an execu-
    tion entry function of the slave CPU.

    void  /* Initialize/start slave CPU (from master CPU) */
        InitSlaveCPU(void)
    {
            volatile Uint 16 i;

        /* Set Slave SH reset status in Slave SH */
            while((*SMPC_SF & 0x01) == 0x01);
            *SMPC_SF = 1;                 /* — SMPC StatusFlag SET */
            *SMPC_COM = SMPC_SSHOFF;      /* — Slave SH OFF SET */
            while((*SMPC_SF & 0x01 == 0x01);
            for(i = 0; i < 10; i++);      /* slave RESET assert length */
            SETSINT(0x94,(void *)&SlaveCPUmain);      /* Set Entry Function */
        /* Release reset status of Slave SH */
            *SMPC_SF = 1;                 /* — SMPC Status Flag SET */
            *SMPC_COM = SMPC_SSHON;       /* — Slave SH ON SET */
            while((*SMPC_SF & 0x01) == 0x01);
    }

    void  /* Slave CPU entry function (FRT Input Capture Polling) */
        SlaveCPUmain(void)
    {
            /* Wait until SlaveCommand is set */
            /* then call function for SlaveCommand */
            set_imask(0xf);
            *IPRA = 0x0000;   /* Disable IPRA interrupt */
            *IPRB = 0x0000;   /* Disable IPRB interrupt */
            *TIER = 0x01;     /* Disable TIER FRT Input Capture interrupt */
            while(1){
            /* Use “FRT InputCaptureFlag” Polling for wait command from Master */
               if((*FTCSR & 0x80) == 0x80){
                   *FTCSR = 0x00;          /* FTCSR clear */
            /* Execute function requested from master CPU */
               (*(void(*)(void))*(void**)((Uint32)&SlaveCommand+0x20000000))();
               SlaveCommand = (void*)0;   /* RESET request code */
            }
        }
    }

    8.2      Function Execution Request from Master CPU to Slave CPU
    extern void SlaveFunction(void);
    SlaveCommand = SlaveFunction;
    *(Uint16 *)0x21000000 = 0xffff;      /* FRT Input Capture for Slave */
                   SEGA Confidential

      6
```

## PDF page 12

[Original page](https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf#page=12)

```text
                  Virtual CD Tool (September 27, 1994 Version)

This document provides details about the virtual CD tool 9/27/94.

Note that to use the virtual CD, virtual internal CD Board ROM version 3.1 must be
replaced with version 3.2.  Also note that the script syntax changed when the
VCDPRE.EXE was upgraded from version 1.xx to version 2.xx.

1.   Virtual CD interface board EPROM (version 3.2)
    This version corrects a fault that prevented CD-DA data on the virtual CD from
    being played in multi-player mode.
2.   VCDEMU.EXE (version 1.70)
    This version implements multi-index and scan play.  Scan play is valid only in
    real-time emulation mode.
    The current version does not support file interleaving during direct DOS file
    access.

    Note:     To operate VCDEMU.EXE version 1.70, you must upgrade the ROM on the virtual CD
             board to version 3.2.

3.   VCDPRE.EXE, VCDBUILD.EXE (version 2.13)
    Modifications from version 2.11
    •    The directory record is always located at the beginning of a sector when the
        number of files is large and the directory file extends over multiple sectors.
    •    Processing is interrupted if the same file name exists.

    [Restriction]
    The total number of files that are currently operating is only a little more than 500.  Although the
    specification allows up to 1300 files to be executed as options when XMS is used, this specifica-
    tion has not been announced due to a bug that was found.

    Furthermore, an urgent request to support 2000 or more files has been made.

    Note:    In version 2.xx, the syntax of the script file is different from the syntax used in version
             1.xx.  Users of version 1.xx must change the script file.  For details, refer to the syntax in
             item 7, EXSAMPLE.SCR.

4.   VCDUTL.EXE (version 1.00)
5.   VCDMKTOC.EXE (version 1.20)
6.   SWAP.EXE (version 1.00)
    SWAP.EXE is an endian conversion program for CD-DA data.
7.   EXSAMPLE.SCR
    EXSAMPLE.SCR is an example of new script file for VCDPRE version 2.xx.  Refer
    to this example during script modification.
   SEGA Confidential


SEGA Saturn Dual CPU User's Guide                                                                                                         7
```
