; DSP-initiated DMA probe: copy eight longwords from Work RAM into data RAM 1
; and back out to another place, with the address addition chosen at assembly
; time (-D RS1 / RS2 for the read, -D WS1 / WS2 for the write).
;
; Data RAM 3: word 0 = source address / 4, word 1 = destination address / 4.

        MOV     0,CT3
        MOV     MC3,RA0                 ; source
        MOV     MC3,WA0                 ; destination
        MOV     0,CT1
IFDEF RS1
        DMA1    D0,M1,8
ENDIF
IFDEF RS2
        DMA2    D0,M1,8
ENDIF
WAIT1:  JMP     T0,WAIT1                ; T0 is set while the DMA runs
        NOP
        MOV     0,CT1
IFDEF WS1
        DMA1    M1,D0,8
ENDIF
IFDEF WS2
        DMA2    M1,D0,8
ENDIF
        NOP
        ENDI
