/* The SH-2 routine of projected_bounds_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* Registers through the loop:
 *   r4 vertex   r5 vertices left   r6 out
 *   r3 smallest XY word   r7 largest XY word   r8 min y   r9 max y   r10 min w
 *   r0 XY word   r1 y   r2 w */

	.section .text.sat_scene3d_projected_bounds_sh2,"ax",@progbits
	.align 2
	.global _sat_scene3d_projected_bounds_sh2
	.type _sat_scene3d_projected_bounds_sh2,@function
_sat_scene3d_projected_bounds_sh2:
	mov.l r8,@-r15
	mov.l r9,@-r15
	mov.l r10,@-r15
	mov.l @r4+,r3
	mov.l @r4+,r10
	mov r3,r7
	exts.w r3,r8
	dt r5
	bt.s .Lpb_done
	mov r8,r9
	.align 2
.Lpb_vertex:
	mov.l @r4+,r0
	mov.l @r4+,r2
	cmp/gt r0,r3
	bt .Lpb_min_x
.Lpb_1:
	cmp/gt r7,r0
	bt .Lpb_max_x
.Lpb_2:
	exts.w r0,r1
	cmp/gt r1,r8
	bt .Lpb_min_y
.Lpb_3:
	cmp/gt r9,r1
	bt .Lpb_max_y
.Lpb_4:
	cmp/gt r2,r10
	bt .Lpb_min_w
.Lpb_5:
	dt r5
	bf .Lpb_vertex
.Lpb_done:
	swap.w r3,r0
	swap.w r7,r1
	exts.w r0,r0
	exts.w r1,r1
	mov.l r0,@r6
	mov.l r1,@(4,r6)
	mov.l r8,@(8,r6)
	mov.l r9,@(12,r6)
	mov.l r10,@(16,r6)
	mov.l @r15+,r10
	mov.l @r15+,r9
	rts
	mov.l @r15+,r8
.Lpb_min_x:
	bra .Lpb_1
	mov r0,r3
.Lpb_max_x:
	bra .Lpb_2
	mov r0,r7
.Lpb_min_y:
	bra .Lpb_3
	mov r1,r8
.Lpb_max_y:
	bra .Lpb_4
	mov r1,r9
.Lpb_min_w:
	bra .Lpb_5
	mov r2,r10
	.size _sat_scene3d_projected_bounds_sh2,.-_sat_scene3d_projected_bounds_sh2
	.text
