.global fix16_assembly_mul

fix16_assembly_mul:
    dmuls.l r4, r5
    xtrct r5, r4, r0
    rts
    nop
