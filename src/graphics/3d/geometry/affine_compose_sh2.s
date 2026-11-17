/* Exact Q16 affine matrix composition, disjoint out, canonical bottom rows.
 * r4 output, r5 parent row, r6 child, r7 row count.
 * r8-r10 parent coefficients; r0 next child operand, r1 high, r2 low,
 * r3 accumulated Q16. Leaf; no DIVU/SR.S dependence. Save r8-r10.
 * Each product independently floors (bits 16..47); do NOT use MAC.L. */
.macro dot c
    mov.l @(\c,r6),r0
    dmuls.l r8,r0
    mov.l @(\c+16,r6),r0
    sts mach,r1
    sts macl,r3
    dmuls.l r9,r0
    xtrct r1,r3
    mov.l @(\c+32,r6),r0
    sts mach,r1
    sts macl,r2
    dmuls.l r10,r0
    xtrct r1,r2
    add r2,r3
    sts mach,r1
    sts macl,r2
    xtrct r1,r2
    add r2,r3
.endm

    .section .text.sat_affine_compose_sh2,"ax",@progbits
    .align 2
    .global _sat_affine_compose_sh2
    .type _sat_affine_compose_sh2,@function
_sat_affine_compose_sh2:
    mov.l r8,@-r15
    mov.l r9,@-r15
    mov.l r10,@-r15
    mov #3,r7
.Lrow:
    mov.l @r5+,r8
    mov.l @r5+,r9
    mov.l @r5+,r10
    dot 0
    mov.l r3,@r4
    dot 4
    mov.l r3,@(4,r4)
    dot 8
    mov.l r3,@(8,r4)
    dot 12
    mov.l @r5+,r0
    add r0,r3
    mov.l r3,@(12,r4)
    dt r7
    bf/s .Lrow
    add #16,r4
    mov #0,r0
    mov.l r0,@r4
    mov.l r0,@(4,r4)
    mov.l r0,@(8,r4)
    mov #1,r0
    shll16 r0
    mov.l r0,@(12,r4)
    mov.l @r15+,r10
    mov.l @r15+,r9
    rts
    mov.l @r15+,r8
    .size _sat_affine_compose_sh2,.-_sat_affine_compose_sh2
    .text
