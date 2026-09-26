; The batch 3x3 transform with the data moved by the DSP itself: it DMAs the
; input vectors out of Work RAM, transforms them (the same 23-instruction body
; as dsp_transform.dsp) and DMAs the results back, so the SH-2 only writes a few
; parameters and starts it.
;
; Data RAM 0: the matrix, nine words row by row (16.16 fixed point)
; Data RAM 3: word 0 = source address / 4, word 1 = destination address / 4,
;             words 2 and 3 = 3 * vectors (the two DMA lengths),
;             word 63 = vectors - 1
; Data RAM 1 and 2 hold the input and the output vectors, as in the plain program.
;
; The DMA uses address addition 2: four bytes per longword (the manual counts
; the addition in 16-bit units, so 1 would step two bytes; measured on Ymir and
; Mednafen).

        ORG     32
STARTD: MOV     0,CT3
        MOV     MC3,RA0                 ; source
        MOV     MC3,WA0                 ; destination
        MOV     0,CT1
        DMA2    D0,M1,MC3               ; 3 * vectors words into data RAM 1
WAITIN: JMP     T0,WAITIN               ; T0 is set while the DMA runs
        NOP
        MOV     0,CT0
        MOV     0,CT1
        MOV     0,CT2
        MOV     63,CT3
        MOV     M3,LOP                  ; LOP = vectors - 1
        MOV     VLOOPD,TOP
VLOOPD: MOV     0,CT3                   ; copy this vector into the scratch words
        MOV     MC1,MC3
        MOV     MC1,MC3
        MOV     MC1,MC3
        MOV     0,CT3
        MOV     0,CT0

        MOV     MC0,X   MOV     MC3,Y
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     0,CT3
        AD2     MOV     ALU,A   MOV     ALL,MC2
        MOV     MC0,X   MOV     MC3,Y
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     0,CT3
        AD2     MOV     ALU,A   MOV     ALL,MC2
        MOV     MC0,X   MOV     MC3,Y
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P
        AD2     MOV     ALU,A   MOV     ALL,MC2

        BTM
        NOP
        MOV     0,CT2
        MOV     3,CT3                   ; the length of the way back
        DMA2    M2,D0,MC3
WAITOUT: JMP    T0,WAITOUT
        NOP
        ENDI
