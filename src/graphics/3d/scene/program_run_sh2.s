/* The SH-2 routine of program_run_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* Registers through the loop:
 *   r4 order   r5 indices   r6 entries   r7 command slot   r8 faces left
 *   r9 cached faces   r10 24 (entry size)   r11 -1   r13 the job
 *   r1 entry index, then entry   r2 program
 *   r3 kind:cached:CMDGRDA, then CMDGRDA   r0, r12 scratch
 * One loop per way of finding the entry, chosen once. */

/* The face whose entry index is in r1; loops to `face`. */
.macro emit_face face, mode, shaded
	mulu.w r10,r1
	sts macl,r1
	add r6,r1
	mov.l @(20,r1),r3
	mov.l @(16,r1),r2
	/* kind == program, else stop before this face. */
	mov r3,r0
	shlr16 r0
	mov r0,r12
	shlr8 r12
	dt r12
	bf .Lpr_stop
	tst #255,r0
	subc r11,r9
	extu.w r3,r3
	/* CMDCTRL, then the draw mode and colour. */
	mov.w @r2,r0
	tst r3,r3
	shll16 r0
	bf.s \shaded
	mov.l r0,@r7
	mov.l @(4,r2),r0
\mode:
	mov.l @(8,r2),r12
	mov.l r0,@(4,r7)
	mov.l @r1,r0
	mov.l r12,@(8,r7)
	mov.l @(4,r1),r12
	mov.l r0,@(12,r7)
	mov.l @(8,r1),r0
	mov.l r12,@(16,r7)
	mov.l @(12,r1),r12
	mov.l r0,@(20,r7)
	shll16 r3
	mov.l r12,@(24,r7)
	mov.l r3,@(28,r7)
	dt r8
	bf.s \face
	add #32,r7
.endm

/* Shaded: the program's shaded draw mode beside its colour word. */
.macro shaded_mode mode, shaded
\shaded:
	mov.w @(6,r2),r0
	extu.w r0,r12
	mov.w @(2,r2),r0
	shll16 r0
	bra \mode
	or r12,r0
.endm

	.section .text.sat_scene3d_program_run_sh2,"ax",@progbits
	.align 2
	.global _sat_scene3d_program_run_sh2
	.type _sat_scene3d_program_run_sh2,@function
_sat_scene3d_program_run_sh2:
	mov.l r8,@-r15
	mov.l r9,@-r15
	mov.l r10,@-r15
	mov.l r11,@-r15
	mov.l r12,@-r15
	mov.l r13,@-r15
	mov r4,r13
	mov.l @(4,r13),r5
	mov.l @(8,r13),r6
	mov.l @(12,r13),r7
	mov.l @(16,r13),r8
	mov.l @r13,r4
	mov #0,r9
	mov #24,r10
	tst r5,r5
	bt.s .Lpr_direct
	mov #-1,r11
	.align 2
.Lpr_indirect:
	mov.w @r4+,r0
	extu.w r0,r0
	add r0,r0
	mov.w @(r0,r5),r1
	extu.w r1,r1
	emit_face .Lpr_indirect, .Lpr_mode_i, .Lpr_shaded_i
.Lpr_stop:
	/* Written = max - faces left. */
	mov.l @(16,r13),r0
	mov.l r9,@(20,r13)
	sub r8,r0
	mov.l @r15+,r13
	mov.l @r15+,r12
	mov.l @r15+,r11
	mov.l @r15+,r10
	mov.l @r15+,r9
	rts
	mov.l @r15+,r8
	shaded_mode .Lpr_mode_i, .Lpr_shaded_i
	.align 2
.Lpr_direct:
	mov.w @r4+,r1
	extu.w r1,r1
	emit_face .Lpr_direct, .Lpr_mode_d, .Lpr_shaded_d
	bra .Lpr_stop
	nop
	shaded_mode .Lpr_mode_d, .Lpr_shaded_d
	.size _sat_scene3d_program_run_sh2,.-_sat_scene3d_program_run_sh2
	.text
