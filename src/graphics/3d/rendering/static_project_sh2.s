/* Exact Q16 static prefix, experimental. ABI in static_project_sh2.hpp.
 * r4 input, r5 matrix, r6 output (previous until pending Y is stored),
 * r7 remaining, r8 product scratch, r9 DIVU base, r10 width, r11 -1,
 * r12 mult, r13 reach, r14 double reach, r1 X/Y, r2 W, r3 row, r0 scratch.
 * Stack: X/Y/Z at 0/4/8, w_last at 12, y_scale at 16, count at 20.
 * 52 bytes stack including saved r8-r14. Leaf; MACH/MACL and T clobbered.
 * The divider belongs to this CPU; same interrupt contract as existing
 * projection kernels. No MAC.L accumulation: floor EACH 32x32 product. */

.macro term off
    mov.l @r3+,r8
    mov.l @(\off,r15),r0
    dmuls.l r0,r8
    sts mach,r0
    sts macl,r8
    xtrct r0,r8
.endm
.macro row out
    term 0
    mov r8,\out
    term 4
    add r8,\out
    term 8
    add r8,\out
    mov.l @r3+,r0
    add r0,\out
.endm
.macro record
    mov.l @r4+,r0
    mov.l r0,@r15
    mov.l @r4+,r0
    mov.l r0,@(4,r15)
    mov.l @r4+,r0
    mov.l r0,@(8,r15)
    mov r5,r3
.endm
.macro xw
    row r1
    add #32,r3
    row r2
.endm
/* Long conditional branch via nearby inverted branch: SH-2 BT/BF reach
 * is only +/-256 bytes. */
.macro reject_if_t target
    bf 1f
    bra \target
    nop
1:
.endm
.macro inside stop
    mov.l @(12,r15),r13
    mov r2,r0
    add #-1,r0
    cmp/hs r13,r0
    reject_if_t \stop
    mul.l r12,r2
    sts macl,r13
    add #-1,r13
    mov r13,r14
    add r13,r14
    mov r1,r0
    add r13,r0
    cmp/hi r14,r0
    reject_if_t \stop
.endm
.macro divide
    sts mach,r0
    sts macl,r1
    mov.l r0,@(16,r9)
    mov.l r1,@(20,r9)
.endm
.macro quotient
    mov.l @(20,r9),r0
    cmp/pz r0
    subc r11,r0
    shar r0
.endm

    .section .text.sat_static_project_sh2,"ax",@progbits
    .align 2
    .global _sat_static_project_sh2
    .type _sat_static_project_sh2,@function
_sat_static_project_sh2:
    mov.l r8,@-r15
    mov.l r9,@-r15
    mov.l r10,@-r15
    mov.l r11,@-r15
    mov.l r12,@-r15
    mov.l r13,@-r15
    mov.l r14,@-r15
    add #-24,r15
    mov.l @(28,r4),r0
    mov.l @(20,r4),r1
    mov.l @(12,r4),r7
    mov.l r0,@(12,r15)
    mov.l r1,@(16,r15)
    mov.l r7,@(20,r15)
    mov.l @(16,r4),r10
    mov.l @(24,r4),r12
    mov.l @(4,r4),r5
    mov.l @(8,r4),r6
    mov.l @r4,r4
    mov #-1,r9
    shll8 r9
    mov #-1,r11
    add #-8,r6
    record
    xw
    inside .Lstop
    bra .Lx
    dmuls.l r10,r1
    .align 2
.Lvertex:
    xw
    inside .Lpending
    dmuls.l r10,r1
    quotient
    mov.w r0,@(2,r6)
.Lx:
    add #8,r6
    mov.l r2,@r9
    divide
    mov.l r2,@(4,r6)
    /* Y row while X divide runs. */
    add #-48,r3
    row r1
    mov r1,r0
    add r13,r0
    cmp/hi r14,r0
    bt .Lreject_y
    mov.l @(16,r15),r0
    dmuls.l r0,r1
    dt r7
    bt .Llast
    record
    quotient
    mov.w r0,@r6
    /* Start Y divide, then prepare next vertex's X/W. */
    sts mach,r0
    sts macl,r1
    mov.l r0,@(16,r9)
    bra .Lvertex
    mov.l r1,@(20,r9)
.Llast:
    quotient
    mov.w r0,@r6
    divide
.Lpending:
    quotient
    mov.w r0,@(2,r6)
    bra .Lstop
    nop
.Lreject_y:
    /* Drain outstanding X; do not publish either coordinate on refusal. */
    mov.l @(20,r9),r0
.Lstop:
    mov.l @(20,r15),r0
    add #24,r15
    sub r7,r0
    mov.l @r15+,r14
    mov.l @r15+,r13
    mov.l @r15+,r12
    mov.l @r15+,r11
    mov.l @r15+,r10
    mov.l @r15+,r9
    rts
    mov.l @r15+,r8
    .size _sat_static_project_sh2,.-_sat_static_project_sh2
    .text
