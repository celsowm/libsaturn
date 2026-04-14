# <30/31>SCU DSP Assembler Instruction Manual

[Japanese](javascript:window.location.href = window.location.pathname.replace('.htm', '_j.htm');)

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm)★ [HARDWARE Manual](../../index.md) ★ [SCU User's Manual](../index.md)

---

**▲ [Back](p04_59.md) ｜ [Forward](dspsim.md) ▼**

---

# SCU DSP Assembler Instruction Manual

'94/07/08

---

## table of contents

[1. Assembler functions](dspasm.md) [2. Executing assembler](dspasm.md) [3. Program description format](dspasm.md) [4. Command list](dspasm.md) [5. Sample program](dspasm.md)

## 1. Overview of assembler functions

This assembler is for generating instruction codes for DSP, and is intended to run on MS-DOS and UNIX. The output code is directly output in Motorola's S format, so there is no need to link.  
The DSP assembler requires a lot of knowledge about the hardware, so please deepen your understanding of the DSP hardware before using the assembler.

## 2. Executing assembler

dspasm [option]< source file name>

1. The options can be specified as follows: (The file is created only when it ends normally):   -l[file name]: List output specification:   -a[file name] : Data format output specification for SH assembler:   -c[file name]: C language data format output specification:   -m: Specify when using MODEL M development environment- Source file names have no default extension.- Only the first error detected will be displayed, so please repeat the correction and assembly until the error no longer occurs.

## 3. Program description format

**[Label] [△Operation [△Operand]]…[Comment]**  
Example: LABEL: MOV MC0,X ; Comment

### (1) Label

- Can be defined by the programmer and used as a destination name for JMP instructions.- When writing a label, start from the first column or add a ``:'' at the end, such as ``LABEL:''.- The maximum length of a label is 32 characters, and the characters that can be used are uppercase letters, lowercase letters, numbers, and underscores (\_). Anything other than numbers can be placed at the beginning. In addition, uppercase and lowercase letters are considered the same.

### (2) Operation

- Write the DSP execution command.- When starting with an operation, put a space character in front of it.- Up to 6 operations may be arranged, including only arithmetic instructions.

### (3) Operand

- Describes what the operation will be performed on.- Use a space character to separate it from the operand.

### (4) Comments

- You can write annotations to make the program easier to understand.- A comment starts from the point where the ";" is added to the end of the line.

**\* Notes on description**

- Basically, operations ~ operands are written so that they fit on one line, but if they do not fit on one line, you can connect them to the next line by adding "\" before the line feed code. However, if you want to connect an operation after a line with a comment, add "\" before ";". At this time, please make sure that the maximum length of one line is 255 bytes.- Operations and operands can be in uppercase or lowercase letters.- Please use hexadecimal ($xx), decimal (xxx), and binary (%xxxxxxxx) to represent numerical values.- The address where the output code is placed can be specified using the ORG pseudo-instruction.- The program area in the DSP is only 256 instructions long, but in order to ensure that processing division and optimization work goes smoothly, a warning can be issued and 2048 instructions worth of code can be output. This code can only be supported by a simulator, so when using it in an actual DSP, it must be shortened to 256 instructions or less and assembled. Also, if you use a label with an address that exceeds 256 instructions, please note that you cannot assign an 8-bit value.

**\* About reserved words**

- The following names used for operands cannot be used for labels.  
  {ALH, ALL, ALU, M0, M1, M2, M3, MC0, MC1, MC2, MC3, MUL}

**\* About numerical operations**

- The following operators can be used when setting values for labels or using numbers for operands. (However, when using it as an operand, please note that blank characters are not allowed. Example: ○ JMP $+2, × JMP $ + 2)

### Arithmetic relationship

+ … addition - … Subtraction \* … Multiplication / … division % … Remainder ‾ … bit negation & … bit product | … bit sum ^ … Exclusive bitwise sum << … left shift >> … right shift ### Priority 1. + - ~ (unary operator) 2. \* / % 3. + - 4.<<>> 5. & 6. | ^ ## 4. Command list 1. Arithmetic instruction: NOP AND OR XOR ADD SUB AD2 SR RR SL RL RL8 CLR MOV- Load immediate instruction: MVI- DMA command: DMA DMAH- JUMP instruction: JMP- LOOP BOTTOM instruction: BTM LPS- END command: END ENDI ### pseudo-instruction - EQU (=) ……………… Used to define a label.- ORG ...... Specifies the starting address where the instruction will be placed.- ENDS ……………… If placed at the end of the program, it will be ignored thereafter.- IF <number/label> ... If the calculation result of the number or label is other than 0, then assemble up to ELSE or ENDIF.- IFDEF< label> ...... If the label is defined at the front, then ELSE or ENDIF will be assembled. The nesting level for IF and IFDEF is up to 16. ) ## 5. Sample program ### (1) When copying the contents of internal RAM 0 of the DSP to internal RAM 1. ``` ; ------- sample (1) start ------- COPY_SIZE = 12 ; Copy size RAM0_ADR = $00 ; Copy source address RAM1_ADR = $00 ; Copy destination address MOV RAM0_ADR,CT0 ; Set the copy source address of RAM0 MOV RAM1_ADR,CT1 ; Set the copy destination address of RAM1 MOV COPY_SIZE-1,LOP ; Set transfer size - 1 to LOP register LPS ; Execute 1-instruction loop MOV MC0,MC1 ; Transfer from RAM0 to RAM1 ENDI ; ------- sample (1) end ------- ``` ### (2) When calculating 2 x 3 + 4 x 5. (RAM0 x RAM1 + RAM0 x RAM1 = RAM2) (Sample 2b is an optimized version of 2a.) ``` ; ------- sample (2a) start ------- RAM0_ADR = $00 ; 2, 4 Storage address first RAM1_ADR = $00 ; 3, 5 Storage address first RAM2_ADR = $00 ; Result storage address MOV RAM0_ADR,CT0 ; Set RAM0 address MOV RAM1_ADR,CT1 ; Set RAM1 address MVI #2,MC0 ; Set "2" to RAM0 MVI #3,MC1 ; Set "3" to RAM1 MVI #4,MC0 ; Set "4" to RAM0 MVI #5,MC1 ; Set "5" to RAM1 MOV RAM0_ADR,CT0 ; Set the address of RAM0 MOV RAM1_ADR,CT1 ; Set the address of RAM1 MOV RAM2_ADR,CT2 ; Set the address of RAM2 MOV MC0,X ; Transfer data from RAM0 to RX MOV MC1,Y ; Transfer data from RAM1 to RY MOV MUL,P ; Store the integration results of RX and RY in PH,PL MOV MC0,X ; Transfer data from RAM0 to RX MOV MC1,Y ; Transfer data from RAM1 to RY CLR A ; Set "0" to ACH,ACL AD2 MOV ALU,A ; Store the addition result of PH,PL and ACH,ACL in ACH,ACL MOV MUL,P ; Store the integration result of RX and RY in PH,PL AD2 MOV ALL,MC2 ; Store the addition result of PH, PL and ACH, ACL in RAM2 ENDI ; ------- sample (2a) end ------- ; ------- sample (2b) start ------- RAM0_ADR = $00 ; 2, 4 Storage address first RAM1_ADR = $00 ; 3, 5 Storage address first RAM2_ADR = $00 ; Result storage address MOV RAM0_ADR,CT0 MOV RAM1_ADR,CT1 MVI #2,MC0 MVI #3,MC1 MVI #4,MC0 MVI #5,MC1 MOV RAM0_ADR,CT0 MOV RAM1_ADR,CT1 MOV MC0,X MOV MC1,Y MOV RAM2_ADR,CT2 MOV MC0,X MOV MUL,P MOV MC1,Y CLR A AD2 MOV MUL,P MOV ALU,A AD2 MOV ALL,MC2 ENDI ; ------- sample (2b) end ------- ``` ### (3) When calculating movement processing for a matrix. (RAM0×RAM1=RAM2) ``` ／Ｍ００ Ｍ０１ Ｍ０２ Ｍ０３＼ ／１００ｘ＼ 　 ／Ｍ００ Ｍ０１Ｍ０２ Ｍ０３＼ ｜ Ｍ１０ Ｍ１１ Ｍ１２ Ｍ１３ ｜｜ ０１０ｙ ｜ → ｜ Ｍ１０ Ｍ１１Ｍ１２ Ｍ１３ ｜ ＼Ｍ２０ Ｍ２１ Ｍ２２ Ｍ２３／ ｜ ００１ｚ ｜ 　 ＼Ｍ２０ Ｍ２１Ｍ２２ Ｍ２３／ 　　　　 　　　 　　　 　　　　 ＼０００１／ ; ------- sample (3) start ------- DATA_TOP = $10000 >> 2 ; External memory address is in 4-byte units MAT_SIZE = $0C ; Array size RAM0_ADR = $00 ; Start address for storing X, Y, Z movement amount RAM1_ADR = $00 ; Array work address RAM2_ADR = $00 ; Original array address ; (Transfer the array with the movement amount set from external memory to RAM0) ; MVI DATA_TOP,RA0 MOV RAM0_ADR,CT0 DMA D0,MC0,#$02 ; ; (Copying the array to be operated from RAM2 to RAM1) MOV RAM2_ADR,CT2 MOV RAM1_ADR,CT1 MOV MAT_SIZE-1,LOP LPS MOV MC2,MC1 WAITING: JMP T0,WAITING ; ; (Execute array calculation) MOV RAM0_ADR,CT0 MOV RAM1_ADR,CT1 MOV MC0,X MOV MC1,Y MOV MC0,X MOV MUL,P MOV MC1,Y CLR A AD2 MOV MC0,X MOV MUL,P MOV MC1,Y MOV ALU,A MOV RAM0_ADR,CT0 AD2 MOV MUL,P MOV MC1,Y MOV ALU,A MOV #1,RX AD2 MOV MC0,X MOV MUL,P MOV MC1,Y MOV ALU,A MOV RAM2_ADR+3,CT2 AD2 MOV MC0,X MOV MUL,P MOV MC1,Y CLR A MOV ALL,MC2 AD2 MOV MC0,X MOV MUL,P MOV MC1,Y MOV ALU,A MOV RAM0_ADR,CT0 AD2 MOV MUL,P MOV MC1,Y MOV ALU,A MOV #1,RX AD2 MOV MC0,X MOV MUL,P MOV MC1,Y MOV ALU,A MOV RAM2_ADR+7,CT2 AD2 MOV MC0,X MOV MUL,P MOV MC1,Y CLR A MOV ALL,MC2 AD2 MOV MC0,X MOV MUL,P MOV MC1,Y MOV ALU,A MOV RAM0_ADR,CT0 AD2 MOV MUL,P MOV MC1,Y MOV ALU,A MOV #1,RX AD2 MOV MUL,P MOV ALU,A MOV RAM2_ADR+11,CT2 AD2 MOV ALL,MC2 ENDI ; ------- sample (3) end ------- ``` that's all --- **▲ [Back](p04_59.md) ｜ [Forward](dspsim.md) ▼** --- [![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm) ★ [HARDWARE Manual](../../index.md) ★ [SCU User's Manual](../index.md) ---
