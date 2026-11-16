/* The SH-2 routine of rigid_project_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* Registers through the loop:
 *   r4 record   r5 joints   r6 output vertex   r7 vertices left
 *   r8 32768 (row rounding)   r9 divider base   r10 width   r11 -1   r12 mult
 *   r13 reach = w * mult - 1   r14 2 * reach
 *   r1 x, then y   r2 w   r3 joint row pointer   r0 scratch
 * Stack: @(0..8) the doubled position   @(12) w_last   @(16) y_scale
 *        @(20) count */

/* One clip row into `out`: r3 walks the row, r0 the position. */
.macro row out
	mov r15,r0
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

/* Reads one record and forms its X row (r1) and W row (r2); r3 ends past the
 * W row. */
.macro xw 
	mov.w @r4+,r0
	mov.w @r4+,r1
	mov.w @r4+,r2
	mov.w @r4+,r3
	add r0,r0
	add r1,r1
	add r2,r2
	mov.l r0,@r15
	mov.l r1,@(4,r15)
	mov.l r2,@(8,r15)
	extu.w r3,r3
	shlr8 r3
	shll8 r3
	shlr2 r3
	add r5,r3
	row r1
	add #16,r3
	row r2
.endm

/* The divider's quotient halved toward zero, in r0. */
.macro quotient 
	mov.l @(20,r9),r0
	cmp/pz r0
	subc r11,r0
	shar r0
.endm

	.section .text.sat_skin_rigid_project_sh2,"ax",@progbits
	.align 2
	.global _sat_skin_rigid_project_sh2
	.type _sat_skin_rigid_project_sh2,@function
_sat_skin_rigid_project_sh2:
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
	mov #1,r8
	mov #-1,r9
	shll16 r8
	shll8 r9
	shlr r8
	mov #-1,r11
	xw 
	bra .Lrp_have
	nop
	.align 2
.Lrp_vertex:
	xw 
	/* The vertex before's Y, whose divide ran under those rows. */
	quotient 
	mov.w r0,@(2,r6)
	add #8,r6
.Lrp_have:
	/* 0 < w <= w_last, then |x| <= reach. */
	mov.l @(12,r15),r13
	mov r2,r0
	add #-1,r0
	cmp/hs r13,r0
	bt .Lrp_stop
	mul.l r12,r2
	mov.l r2,@(4,r6)
	sts macl,r13
	add #-1,r13
	mov r13,r14
	add r13,r14
	mov r1,r0
	add r13,r0
	cmp/hi r14,r0
	bt .Lrp_stop
	/* X divide: w, then the 64-bit numerator. */
	dmuls.l r10,r1
	mov.l r2,@r9
	sts mach,r0
	sts macl,r1
	mov.l r0,@(16,r9)
	mov.l r1,@(20,r9)
	/* Y row under it. */
	add #-32,r3
	row r1
	mov r1,r0
	add r13,r0
	cmp/hi r14,r0
	bt .Lrp_stop
	mov.l @(16,r15),r0
	dmuls.l r0,r1
	quotient 
	mov.w r0,@r6
	/* Y divide: the divisor is still w. */
	sts mach,r0
	sts macl,r1
	mov.l r0,@(16,r9)
	mov.l r1,@(20,r9)
	dt r7
	bf .Lrp_vertex
	quotient 
	mov.w r0,@(2,r6)
	/* Finished = count - vertices left. */
.Lrp_stop:
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
	.size _sat_skin_rigid_project_sh2,.-_sat_skin_rigid_project_sh2
	.text
