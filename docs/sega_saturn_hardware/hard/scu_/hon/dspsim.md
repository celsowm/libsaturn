# <31/31>SCU DSP Simulator Instruction Manual

[Japanese](javascript:window.location.href = window.location.pathname.replace('.htm', '_j.htm');)

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm)★ [HARDWARE Manual](../../index.md) ★ [SCU User's Manual](../index.md)

---

**▲ [Back](dspasm.md) | ■**

---

# SCU DSP simulator instruction manual

'94/08/25

---

## table of contents

[1. Overview](dspsim.md) [2. Execution environment](dspsim.md) [3. Command list](dspsim.md) [4. Command explanation](dspsim.md) [5. Basic specifications](dspsim.md) [6. User guide](dspsim.md)

## 1. Overview

This simulator simulates DSP processing on MS-DOS or UNIX in order to improve the efficiency of DSP software development.

Each simulation function is executed in command line format. Below is a list of supported features.

1. Program area and data area dump display and editing functions.- File load and save function compatible with S format.- Breakpoint setting and display functions.- Program execution and pause functions.- Ability to single-step programs.- Assemble and disassemble functions.- Display and setting functions for all registers.- History function for input commands.

## 2. Execution environment

### (A) MS-DOS version

A model that runs MS-DOS or PC-DOS version 3.00 or higher. Can be executed with 640KB of memory installed.

### (B) UNIX version

SPARC station specification machines and HP machines.

## 3. Command list

:   [[A] Assemble](dspsim.md):   [[B] Setting and displaying breakpoints](dspsim.md):   [[D] Memory dump display](dspsim.md):   [[E] Memory rewriting](dspsim.md):   [[F] Fill memory](dspsim.md):   [[G] Execute the program](dspsim.md):   [[H] History settings/display](dspsim.md):   [[L] Load file](dspsim.md):   [[M] Moving memory data](dspsim.md):   [[Q] Ending the program](dspsim.md):   [[R] Register setting/display](dspsim.md):   [[S] Step execution of program](dspsim.md):   [[U] Disassemble](dspsim.md):   [[V] Development environment type setting](dspsim.md):   [[W] Memory file save](dspsim.md)

## 4. Command explanation

[▲](dspsim.md)### A: Assemble

Input format:: A [starting address] function:: Converts the input mnemonic to a code and sets it from the specified address. When finished, just enter a new line.

[▲](dspsim.md)### B: Setting and displaying breakpoints

Input format:: B< street address> function:: Setting breakpoints.

Input format:: B function:: Displaying breakpoint configuration information.

Input format:: B-[Address] or BX[Setting number] function:: Clear breakpoint. If the address or setting number is omitted, all will be canceled.

[▲](dspsim.md)### D: Memory dump display

Input format:: D[P|Rn|M] [[Starting address [Ending address]] function:: Memory dump display. Specify "P" to specify the program area, "R0" to "R3" to specify the data area, and "M" to specify the external bus. If the starting address is omitted, the display will start from the address that was last displayed.

[▲](dspsim.md)### E: Memory rewriting

Input format:: E[P|Rn|M]< street address> [Setting value] function:: Rewrites memory. Specify "P" to specify the program area, "R0" to "R3" to specify the data area, and "M" to specify the external bus. If the setting value is omitted, the address will be displayed and the system will wait for data input. To advance the address, enter a blank number, to go back, enter "^", and to cancel, enter only ".".

[▲](dspsim.md)### F: Fill memory

Input format:: F[P|Rn|M]< starting address>< Ending address>< Setting value> function:: Rewrite the range from the start address to the end address with the set value. Specify "P" to specify the program area, "R0" to "R3" to specify the data area, and "M" to specify the external bus.

[▲](dspsim.md)### G: Execute the program

Input format:: G [Starting address [Ending address]] function:: Executes the program from the specified address. If the address is omitted, execution starts from the address specified in the PC register. If you want to stop it while it's running, type CTRL-C.

[▲](dspsim.md)### H: History setting/display

Input format:: H+ function:: Allows import into history buffer. (default)

Input format:: H- function:: Importing to the history buffer is prohibited.

Input format:: H@ function:: Clear history buffer.

Input format:: H [Displayed number] function:: Displays the previous execution results for the specified number of steps. If the display number is omitted, it will be 10, and the maximum is 512.

[▲](dspsim.md)### L: Load file

Input format:: L [P | Rn | M] [File name] [Transfer address] function:: Reads the file into the memory area. Specify "P" to specify the program area, "R0" to "R3" to specify the data area, and "M" to specify the external bus. If the file name is omitted, the file name read last time using the L command will be used. If a forwarding address is specified, the data will be read based on that address.

[▲](dspsim.md)### M: Move memory data

Input format:: M[P|Rn|M]< start>< end> [P|Rn|M]< Forwarding destination> function:: Moves the range from the start address to the end address to the transfer destination. Specify "P" to specify the program area, "R0" to "R3" to specify the data area, and "M" to specify the external bus. If the transfer destination program area is omitted, it will be the same as the transfer source specification.

[▲](dspsim.md)### P: Change program size

Input format:: P[R｜E] function:: Adjust the size to store the program code. If the parameter is omitted, the current setting status will be displayed. If "E" is specified, it will be expanded to a maximum of 2048 instructions, and if "R" is specified, it will be returned to a maximum of 256 instructions.

[▲](dspsim.md)### Q: End of program

Input format:: Q function:: Exit the simulator program.

[▲](dspsim.md)### R: Register setting/display

Input format:: R [< Register name | Flag name>< Setting value> ] function:: Sets the value in the specified register or flag. If the parameter is omitted, the values of all registers are displayed. The register name can be PC TOP LOP CT0 CT1 Ct2 RX RY PH PL ACH ACLTN0 RA0 WA0 A1, and the flag name can be PR EP T1 T0 SZCVE ES EX.

Input format:: R@ function:: Set all registers to 0.

[▲](dspsim.md)### S: Step execution of program

Input format:: S [number of steps] function:: Executes instructions for the specified number of steps starting from the address indicated by the PC register. If the parameter is omitted, only one step will be executed.

[▲](dspsim.md)### U: Disassemble

Input format:: U [Starting address [Ending address]] function:: Displays the disassembly of the specified range. If the starting address is omitted, the display will start from the address that was last displayed.

[▲](dspsim.md)### V: Development environment type setting

Input format:: V[S|M] function:: If you specify "M" for MODEL-M and "S" for MODEL-S according to the specified development environment, assembly, disassembly, and execution-related processing will be matched. (Default is "S") If model specification is omitted, the currently selected model will be displayed.

[▲](dspsim.md)### W: Save file in memory

s

Input format:: W[P|Rn|M]< starting address>< Ending address>< file name> function:: Saves the data from the specified start address to end address to a file. Specify "P" to specify the program area, "R0" to "R3" to specify the data area, and "M" to specify the external bus.

### ^: Command history display

Input format:: ^ [Displayed number] function:: Displays the specified number of previously entered lines. If the number to be displayed is omitted, it will be 20, and the maximum is 50. If you want to repeat a command, enter "!!", and if you want to repeat a command on a specified line, enter the line number after "!".

## 5. basic specifications

1. The input values are in hexadecimal, except for the command count specification (decimal).- If the file name used to read or save a data file with the L or W command ends in ".s" or ".mot", it will be treated as Motorola S format, and address information will also be processed at the same time.- If you want to input commands all at once, specify the file that describes the commands to be input as a parameter at startup, or use the "< Specify in the format "FILE".- When accessing memory on an external bus, the address is in bytes, so if you specify an internal address when entering a command, add "L" to the end of the address value and it will be converted by 4 times. .- For the D, S, and U commands, if you want to continue displaying, just press the newline key.

## 6. User guide

1. When simulating DMA and transferring data from external memory to internal memory, please input or transfer the data to the external bus memory of this simulator in advance.- The disassembly function of this simulator aligns the positions according to the type of calculation command, so it can be used as a guide for optimization.

that's all

---

 **▲ [Back](dspasm.md) ｜■**

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm) ★ [HARDWARE Manual](../../index.md) ★ [SCU User's Manual](../index.md)

---
