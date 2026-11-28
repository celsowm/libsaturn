; Signed dot products, 16 interleaved XYZ normals per tile.
; M0: three light coefficients. M1: 48 input words. M2: 16 outputs.
; M3 in DMA mode: RA0 and WA0 word addresses, tile count, constant one.
; In port mode the CPU fills M1 and reads M2. Both modes use the same body.
; The final add stores the current dot and preloads the next X/Y. The second
; store executes in BTM's delay slot. The prefetch after the last dot reads
; input word 48 within M1 and is discarded. CT0 and CT1 reset for every tile.
IFDEF STREAM
        ORG     32
        MOV     0,CT3
        MOV     MC3,RA0
        MOV     MC3,WA0
TILE:   MOV     0,CT1
        DMA2    D0,M1,48
INWAIT: JMP     T0,INWAIT
        NOP
ENDIF
        MOV     0,CT1
        MOV     0,CT2
        MOV     0,CT0
        MOV     7,LOP
        MOV     PAIR,TOP
        MOV     MC0,X   MOV     MC1,Y
PAIR:   CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC1,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC1,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     0,CT0
        AD2     MOV     ALU,A   MOV     MC0,X   MOV     MC1,Y   MOV     ALL,MC2
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC1,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC1,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     0,CT0
        BTM
        AD2     MOV     ALU,A   MOV     MC0,X   MOV     MC1,Y   MOV     ALL,MC2
IFDEF STREAM
        MOV     0,CT2
        DMA2    M2,D0,16
        MOV     2,CT3
        MOV     MC3,A
        MOV     M3,P
        MOV     2,CT3
        SUB     MOV     ALU,A   MOV     ALL,MC3
OUTWAIT:JMP     T0,OUTWAIT
        NOP
        JMP     NZ,TILE
        NOP
ENDIF
        ENDI
