/* The SH-2 routine of clip_joints_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* Registers through the loop:
 *   r4 joint element   r5 output joint   r6 joints left   r7 column / origin walker
 *   r8 32768 (rounding)   r9 scaled walker   r10 rows walker   r11 scale
 *   r12 placement   r13 2^29 - 1   r14 2^30 - 1   r0-r3 scratch
 * Stack: @(0..32) the joint's 3x3 by columns   @(36..44) origin   @(48) 65536 */

/* One 3x3 element: range-checked, stored in its column. */
.macro element disp, fail
	mov.l @r4+,r0
	mov r0,r1
	add r13,r1
	cmp/hs r14,r1
	bt \fail
	mov.l r0,@(\disp,r15)
.endm

/* origin = scale * translation + offset, as a checked 64-bit sum. */
.macro origin offset, slot, fail
	mov.l @r4+,r0
	dmuls.l r11,r0
	mov.l @(\offset,r12),r0
	mov r0,r3
	shll r3
	subc r3,r3
	sts macl,r1
	sts mach,r2
	clrt
	addc r0,r1
	addc r3,r2
	mov r1,r3
	shll r3
	subc r3,r3
	cmp/eq r3,r2
	bf \fail
	mov.l r1,@(\slot,r15)
.endm

/* Bits 16..47 of the MAC into r0; fails unless bits 47..63 agree. */
.macro take fail
	sts mach,r1
	sts macl,r0
	exts.w r1,r2
	xtrct r1,r0
	cmp/eq r2,r1
	bf \fail
.endm

.macro coefficient disp, fail
	clrmac
	lds r8,macl
	mac.l @r9+,@r7+
	mac.l @r9+,@r7+
	mac.l @r9+,@r7+
	take \fail
	mov.l r0,@(\disp,r5)
.endm

/* One output row: three coefficients, then the translation. r9 ends on the
 * next scaled row, r10 on the next camera row. */
.macro row d0, d1, d2, d3, fail
	mov r15,r7
	coefficient \d0, \fail
	add #-12,r9
	coefficient \d1, \fail
	add #-12,r9
	coefficient \d2, \fail
	clrmac
	lds r8,macl
	mac.l @r10+,@r7+
	mac.l @r10+,@r7+
	mac.l @r10+,@r7+
	mac.l @r10+,@r7+
	take \fail
	mov.l r0,@(\d3,r5)
.endm

	.section .text.sat_skin_clip_joints_sh2,"ax",@progbits
	.align 2
	.global _sat_skin_clip_joints_sh2
	.type _sat_skin_clip_joints_sh2,@function
_sat_skin_clip_joints_sh2:
	mov.l r8,@-r15
	mov.l r9,@-r15
	mov.l r10,@-r15
	mov.l r11,@-r15
	mov.l r12,@-r15
	mov.l r13,@-r15
	mov.l r14,@-r15
	add #-52,r15
	mov.l @(8,r4),r6
	mov.l @(4,r4),r5
	mov.l @(12,r4),r9
	mov.l @(16,r4),r10
	mov.l @(20,r4),r12
	mov.l @r4,r4
	tst r6,r6
	bf .Lcj_start
	bra .Lcj_ok
	nop
.Lcj_start:
	mov.l @r12,r11
	mov #1,r0
	shll16 r0
	mov.l r0,@(48,r15)
	mov r0,r8
	shlr r8
	mov r0,r13
	shll8 r13
	shll2 r13
	shll2 r13
	shll r13
	mov r13,r14
	add #-1,r13
	add r14,r14
	add #-1,r14
	.align 2
.Lcj_joint:
	element 0, .Lcj_fail_a
	element 12, .Lcj_fail_a
	element 24, .Lcj_fail_a
	origin 4, 36, .Lcj_fail_a
	element 4, .Lcj_fail_a
	element 16, .Lcj_fail_a
	element 28, .Lcj_fail_a
	origin 8, 40, .Lcj_fail_a
	element 8, .Lcj_fail_a
	element 20, .Lcj_fail_a
	element 32, .Lcj_fail_a
	origin 12, 44, .Lcj_fail_a
	bra .Lcj_rows
	add #16,r4
.Lcj_fail_a:
	bra .Lcj_fail
	nop
.Lcj_rows:
	row 0, 4, 8, 12, .Lcj_fail_a
	row 16, 20, 24, 28, .Lcj_fail_b
	bra .Lcj_last
	mov #0,r3
.Lcj_fail_b:
	bra .Lcj_fail
	nop
.Lcj_last:
	row 32, 36, 40, 44, .Lcj_fail_b
	mov.l r3,@(48,r5)
	mov.l r3,@(52,r5)
	mov.l r3,@(56,r5)
	mov.l r3,@(60,r5)
	add #-36,r9
	add #-48,r10
	dt r6
	bt.s .Lcj_ok
	add #64,r5
	bra .Lcj_joint
	nop
.Lcj_fail:
	bra .Lcj_return
	mov #1,r0
.Lcj_ok:
	mov #0,r0
.Lcj_return:
	add #52,r15
	mov.l @r15+,r14
	mov.l @r15+,r13
	mov.l @r15+,r12
	mov.l @r15+,r11
	mov.l @r15+,r10
	mov.l @r15+,r9
	rts
	mov.l @r15+,r8
	.size _sat_skin_clip_joints_sh2,.-_sat_skin_clip_joints_sh2
	.text
