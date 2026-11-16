/* The SH-2 routine of blend_project_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* Registers through the loop:
 *   r4 record   r5 joints   r6 output vertex   r7 vertices left
 *   r8 32768 (row rounding)   r9 divider base   r10 width   r11 -1   r12 mult
 *   r13 reach = w * mult - 1   r14 2 * reach   (before the tests: influence 0's X and W)
 *   r1 x, then y   r2 w   r3 joint row pointer   r0 scratch
 * Stack: @(0..8) influence 0's doubled position   @(12..20) influence 1's
 *        @(24) w_last   @(28) y_scale   @(32) count
 *        @(36) influence 0's Y row   @(40) influence 1's Y row   @(44) influence 0's Y
 *
 * The divider works as in rigid_project: the X divide runs under both Y rows, the Y divide under
 * the next vertex's four X and W rows, and DVSR keeps w between the two. */

/* One clip row into `out`: r3 walks the row, r0 the position at r15 + base. */
.macro row out, base
	mov r15,r0
	.if \base
	add #\base,r0
	.endif
	clrmac
	lds r8,macl
	mac.l @r3+,@r0+
	mac.l @r3+,@r0+
	mac.l @r3+,@r0+
	sts mach,r0
	sts macl,\out
	xtrct r0,\out
	mov.l @r3+,r0
	add r0,\out
.endm

/* One influence: its doubled position to r15 + base, its X row in r1 and W row in r2, the address
 * of its Y row saved at r15 + yrow. */
.macro influence joint, pos, base, yrow
	mov.w @(\pos,r4),r0
	add r0,r0
	mov.l r0,@(\base,r15)
	mov.w @(\pos+2,r4),r0
	add r0,r0
	mov.l r0,@(\base+4,r15)
	mov.w @(\pos+4,r4),r0
	add r0,r0
	mov.l r0,@(\base+8,r15)
	mov.b @(\joint,r4),r0
	extu.b r0,r3
	shll2 r3
	shll2 r3
	shll2 r3
	add r5,r3
	row r1, \base
	mov.l r3,@(\yrow,r15)
	add #16,r3
	row r2, \base
.endm

/* b = floor((a + b) / 2); a and r0 are spent. */
.macro half a, b
	mov \a,r0
	and \b,r0
	and #1,r0
	shar \a
	shar \b
	add \a,\b
	add r0,\b
.endm

/* Takes one record, or leaves through `stop` when it is not a half-weight pair: the blended X in
 * r1 and W in r2, r4 on the next record. */
.macro xw stop
	mov.b @(3,r4),r0
	mov r8,r1
	cmp/eq #2,r0
	bf \stop
	mov.w @(4,r4),r0
	shlr r1
	cmp/eq r1,r0
	bf \stop
	influence 0, 8, 0, 36
	mov r1,r13
	mov r2,r14
	influence 1, 14, 12, 40
	half r13, r1
	half r14, r2
	add #26,r4
.endm

/* The divider's quotient halved toward zero, in r0. */
.macro quotient
	mov.l @(20,r9),r0
	cmp/pz r0
	subc r11,r0
	shar r0
.endm

	.section .text.sat_skin_blend_project_sh2,"ax",@progbits
	.align 2
	.global _sat_skin_blend_project_sh2
	.type _sat_skin_blend_project_sh2,@function
_sat_skin_blend_project_sh2:
	mov.l r8,@-r15
	mov.l r9,@-r15
	mov.l r10,@-r15
	mov.l r11,@-r15
	mov.l r12,@-r15
	mov.l r13,@-r15
	mov.l r14,@-r15
	add #-48,r15
	mov.l @(28,r4),r0
	mov.l @(20,r4),r1
	mov.l @(12,r4),r7
	mov.l r0,@(24,r15)
	mov.l r1,@(28,r15)
	mov.l r7,@(32,r15)
	mov.l @(16,r4),r10
	mov.l @(24,r4),r12
	mov.l @(4,r4),r5
	mov.l @(8,r4),r6
	mov.l @r4,r4
	mov #1,r8
	mov #-1,r9
	shll16 r8
	shll8 r9
	shlr r8
	mov #-1,r11
	xw .Lbp_stop_first
	bra .Lbp_have
	nop
.Lbp_stop_first:
	bra .Lbp_stop
	nop
	/* A record refused with the vertex before's Y divide still running: finish that one. */
.Lbp_pending:
	quotient
	bra .Lbp_stop
	mov.w r0,@(2,r6)
	.align 2
.Lbp_vertex:
	xw .Lbp_pending
	/* The vertex before's Y, whose divide ran under those rows. */
	quotient
	mov.w r0,@(2,r6)
	add #8,r6
.Lbp_have:
	/* 0 < w <= w_last, then |x| <= reach. */
	mov.l @(24,r15),r13
	mov r2,r0
	add #-1,r0
	cmp/hs r13,r0
	bt .Lbp_stop
	mul.l r12,r2
	mov.l r2,@(4,r6)
	sts macl,r13
	add #-1,r13
	mov r13,r14
	add r13,r14
	mov r1,r0
	add r13,r0
	cmp/hi r14,r0
	bt .Lbp_stop
	/* X divide: w, then the 64-bit numerator. */
	dmuls.l r10,r1
	mov.l r2,@r9
	sts mach,r0
	sts macl,r1
	mov.l r0,@(16,r9)
	mov.l r1,@(20,r9)
	/* Both Y rows under it, then their average. */
	mov.l @(36,r15),r3
	row r1, 0
	mov.l r1,@(44,r15)
	mov.l @(40,r15),r3
	row r1, 12
	mov.l @(44,r15),r3
	half r3, r1
	mov r1,r0
	add r13,r0
	cmp/hi r14,r0
	bt .Lbp_stop
	mov.l @(28,r15),r0
	dmuls.l r0,r1
	quotient
	mov.w r0,@r6
	/* Y divide: the divisor is still w. */
	sts mach,r0
	sts macl,r1
	mov.l r0,@(16,r9)
	mov.l r1,@(20,r9)
	dt r7
	bt .Lbp_done
	bra .Lbp_vertex
	nop
.Lbp_done:
	quotient
	mov.w r0,@(2,r6)
	/* Finished = count - vertices left. */
.Lbp_stop:
	mov.l @(32,r15),r0
	add #48,r15
	sub r7,r0
	mov.l @r15+,r14
	mov.l @r15+,r13
	mov.l @r15+,r12
	mov.l @r15+,r11
	mov.l @r15+,r10
	mov.l @r15+,r9
	rts
	mov.l @r15+,r8
	.size _sat_skin_blend_project_sh2,.-_sat_skin_blend_project_sh2
	.text
