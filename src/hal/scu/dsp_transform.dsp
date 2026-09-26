; Batch 3x3 matrix * vector transform for the SCU DSP.
;
; Data RAM 0: the matrix, nine words row by row (16.16 fixed point)
; Data RAM 1: the input vectors, three words each (signed integers), x y z
; Data RAM 2: the output vectors, three words each (16.16 fixed point)
; Data RAM 3: word 63 holds the vector count minus one; words 0-2 are scratch
;
; out = M * v: the integer vector times a 16.16 matrix is a 16.16 result, a
; product-sum of three 32x32 multiplies accumulated in the 48-bit ALU.
;
; One vector takes 23 instructions. Each row is a five-step pipeline: load the
; first pair, multiply and load the second, accumulate and multiply and load the
; third, accumulate and multiply, accumulate and store.

START:  MOV     0,CT0                   ; matrix at the start of bank 0
        MOV     0,CT1                   ; input vectors
        MOV     0,CT2                   ; output vectors
        MOV     63,CT3
        MOV     M3,LOP                  ; LOP = count - 1
        MOV     VLOOP,TOP               ; the loop starts at VLOOP
VLOOP:  MOV     0,CT3                   ; copy this vector into the scratch words
        MOV     MC1,MC3                 ;   x
        MOV     MC1,MC3                 ;   y
        MOV     MC1,MC3                 ;   z
        MOV     0,CT3                   ; scratch again from its start
        MOV     0,CT0                   ; matrix again from its start

; row 0
        MOV     MC0,X   MOV     MC3,Y
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     0,CT3
        AD2     MOV     ALU,A   MOV     ALL,MC2
; row 1
        MOV     MC0,X   MOV     MC3,Y
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     0,CT3
        AD2     MOV     ALU,A   MOV     ALL,MC2
; row 2
        MOV     MC0,X   MOV     MC3,Y
        CLR     A       MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P   MOV     MC0,X   MOV     MC3,Y
        AD2     MOV     ALU,A   MOV     MUL,P
        AD2     MOV     ALU,A   MOV     ALL,MC2

        BTM                             ; next vector
        NOP                             ; the instruction after BTM always runs
        ENDI
