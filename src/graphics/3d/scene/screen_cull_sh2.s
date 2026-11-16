/* The SH-2 routine of screen_cull_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* Registers through the loop:
 *   r4 offsets   r5 projected   r6 double_sided   r7 face_materials
 *   r8 faces left   r9 entry   r10 key
 *   r1 r2 r3 r13 corners a b c d   r0 r11 r12 r14 scratch
 * Stack: @(0) first key, @(4) programs, @(8) pass bits, @(12) the shaded kind word's bias.
 *
 * The loop exists twice, chosen once: unshaded faces take the kind word from the literal pool;
 * shaded ones add the face's number to it, which is r6 less a constant (r6 has stepped once per
 * face), so the bias is kind word + gouraud_first - (double_sided + 1). */

/* The whole face loop. `shaded` is 0 or 1; `n` makes its labels unique. */
.macro faces shaded, n
	.align 2
	/* The face's four corners; a double-sided face skips the area test. */
.Lsc_face\n:
	mov.w @r4+,r1
	mov.w @r4+,r2
	mov.w @r4+,r3
	mov.w @r4+,r13
	mov.b @r6+,r0
	add r5,r1
	add r5,r2
	add r5,r3
	tst r0,r0
	bf.s .Lsc_two_sided\n
	add r5,r13
	/* (c.x-a.x)*(d.y-b.y) > (d.x-b.x)*(c.y-a.y), or the face is culled.
	 * The second product's operands load while the first multiplies. */
	mov.w @(2,r13),r0
	mov.w @r3,r11
	mov r0,r12
	mov.w @(2,r2),r0
	mov.w @r1,r14
	sub r0,r12
	mov.w @(2,r3),r0
	sub r14,r11
	mul.l r12,r11
	mov r0,r12
	mov.w @(2,r1),r0
	mov.w @r13,r11
	mov.w @r2,r14
	sub r0,r12
	sts macl,r0
	sub r14,r11
	mul.l r12,r11
	mov.w @r7+,r12
	sts macl,r11
	cmp/gt r11,r0
	bf .Lsc_next\n
	/* The entry: four XY words, the program, the kind word. r12 holds the
	 * face's material index. */
.Lsc_keep\n:
	mov.l @r1,r0
	mov.l @r2,r11
	mov.l r0,@r9
	mov.l @r3,r0
	mov.l r11,@(4,r9)
	mov.l @r13,r11
	mov.l r0,@(8,r9)
	mov.l @(4,r15),r0
	shll2 r12
	mov.l r11,@(12,r9)
	shll2 r12
	mov.l @(4,r1),r1
	add r0,r12
	mov.l @(4,r2),r2
	mov.l r12,@(16,r9)
	.if \shaded
	mov.l @(12,r15),r0
	mov.l @(4,r3),r3
	add r6,r0
	.else
	mov.l .Lsc_program_word,r0
	mov.l @(4,r3),r3
	.endif
	mov.l r0,@(20,r9)
	mov.l @(4,r13),r13
	/* The painter key: the four depths' sum over 64, or the maximum when
	 * any depth has one of its top two bits set or the quotient reaches it. */
	mov r1,r0
	or r2,r0
	add r2,r1
	or r3,r0
	add r3,r1
	or r13,r0
	add r13,r1
	mov.l .Lsc_top_bits,r11
	mov.l .Lsc_depth_max,r2
	tst r11,r0
	bf.s .Lsc_key\n
	shlr2 r1
	shlr2 r1
	shlr2 r1
	cmp/hi r1,r2
	bf .Lsc_key\n
	mov r1,r2
.Lsc_key\n:
	mov.l @(8,r15),r0
	add #24,r9
	or r0,r2
	mov.l r2,@r10
	add #4,r10
.Lsc_next\n:
	dt r8
	bf .Lsc_face\n
.endm

	.section .text.sat_scene3d_screen_cull_sh2,"ax",@progbits
	.align 2
	.global _sat_scene3d_screen_cull_sh2
	.type _sat_scene3d_screen_cull_sh2,@function
_sat_scene3d_screen_cull_sh2:
	mov.l r8,@-r15
	mov.l r9,@-r15
	mov.l r10,@-r15
	mov.l r11,@-r15
	mov.l r12,@-r15
	mov.l r13,@-r15
	mov.l r14,@-r15
	/* The bias, or 0 for unshaded faces. */
	mov.l @(36,r4),r0
	mov.l @(8,r4),r1
	tst r0,r0
	bt .Lsc_bias
	mov.l .Lsc_program_word,r2
	sub r1,r0
	add r2,r0
	add #-1,r0
.Lsc_bias:
	mov.l r0,@-r15
	mov.l @(32,r4),r0
	mov.l @(28,r4),r1
	mov.l @(24,r4),r10
	mov.l r0,@-r15
	mov.l r1,@-r15
	mov.l r10,@-r15
	mov.l @(20,r4),r9
	mov.l @(16,r4),r8
	mov.l @(12,r4),r7
	mov.l @(8,r4),r6
	mov.l @(4,r4),r5
	mov.l @r4,r4
	tst r8,r8
	bt .Lsc_done
	mov.l @(12,r15),r0
	tst r0,r0
	bf .Lsc_shaded
	faces 0, _plain
.Lsc_done:
	mov.l @r15+,r1
	mov r10,r0
	sub r1,r0
	add #12,r15
	shlr2 r0
	mov.l @r15+,r14
	mov.l @r15+,r13
	mov.l @r15+,r12
	mov.l @r15+,r11
	mov.l @r15+,r10
	mov.l @r15+,r9
	rts
	mov.l @r15+,r8
.Lsc_two_sided_plain:
	bra .Lsc_keep_plain
	mov.w @r7+,r12
.Lsc_shaded:
	faces 1, _shaded
	bra .Lsc_done
	nop
.Lsc_two_sided_shaded:
	bra .Lsc_keep_shaded
	mov.w @r7+,r12
	.align 2
.Lsc_program_word:
	.long 0x01000000
.Lsc_top_bits:
	.long 0xC0000000
.Lsc_depth_max:
	.long 0x00FFFFFE
	.size _sat_scene3d_screen_cull_sh2,.-_sat_scene3d_screen_cull_sh2
	.text
