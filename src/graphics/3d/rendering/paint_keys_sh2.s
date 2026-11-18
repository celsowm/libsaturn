/* The SH-2 routine of paint_keys_sh2.hpp: that header states the contract, the Job layout and the
 * C++ reference the host runs. */
/* range: a new minimum or maximum is rare after the first keys, so the loop
 * falls through its tests and branches only to update.
 *   r4 key   r5 keys left   r6 out   r1 lo   r2 hi   r3 live   r0 the key
 *
 * spread: one loop per bucket rule, chosen once.
 *   r4 key   r5 keys left   r6 counts   r7 lo   r2 multiplier
 *   r0 the key, then twice its bucket   r1 the count
 *
 * quantise: a depth over the cap is rare, so the cap is taken out of line.
 *   r4 key   r5 keys left   r6 low mask   r2 its complement   r7 multiplier   r3 max
 *   r0 the key, then its group   r1 the depth */

	.section .text.sat_paint_keys_range_sh2,"ax",@progbits
	.align 2
	.global _sat_paint_keys_range_sh2
	.type _sat_paint_keys_range_sh2,@function
_sat_paint_keys_range_sh2:
	mov #-1,r1
	mov #0,r2
	tst r5,r5
	bt.s .Lpk_range_done
	mov #0,r3
	.align 2
.Lpk_range_key:
	mov.l @r4+,r0
	cmp/eq #-1,r0
	bt .Lpk_range_next
	cmp/hi r0,r1
	bt .Lpk_range_lo
.Lpk_range_1:
	cmp/hi r2,r0
	bt .Lpk_range_hi
.Lpk_range_2:
	add #1,r3
.Lpk_range_next:
	dt r5
	bf .Lpk_range_key
.Lpk_range_done:
	mov.l r1,@r6
	mov.l r2,@(4,r6)
	rts
	mov r3,r0
.Lpk_range_lo:
	bra .Lpk_range_1
	mov r0,r1
.Lpk_range_hi:
	bra .Lpk_range_2
	mov r0,r2
	.size _sat_paint_keys_range_sh2,.-_sat_paint_keys_range_sh2
	.section .text.sat_paint_keys_spread_sh2,"ax",@progbits
	.align 2
	.global _sat_paint_keys_spread_sh2
	.type _sat_paint_keys_spread_sh2,@function
_sat_paint_keys_spread_sh2:
	mov.l @(4,r4),r5
	mov.l @(8,r4),r6
	mov.l @(12,r4),r7
	mov.l @(16,r4),r2
	tst r5,r5
	bt.s .Lpk_spread_done
	mov.l @r4,r4
	tst r2,r2
	bf .Lpk_scaled_key
	.align 2
.Lpk_plain_key:
	mov.l @r4,r0
	cmp/eq #-1,r0
	bt .Lpk_plain_next
	sub r7,r0
	mov.l r0,@r4
	add r0,r0
	mov.w @(r0,r6),r1
	add #1,r1
	mov.w r1,@(r0,r6)
.Lpk_plain_next:
	dt r5
	bf.s .Lpk_plain_key
	add #4,r4
.Lpk_spread_done:
	rts
	nop
	.align 2
.Lpk_scaled_key:
	mov.l @r4,r0
	cmp/eq #-1,r0
	bt .Lpk_scaled_next
	sub r7,r0
	dmulu.l r2,r0
	sts mach,r0
	mov.l r0,@r4
	add r0,r0
	mov.w @(r0,r6),r1
	add #1,r1
	mov.w r1,@(r0,r6)
.Lpk_scaled_next:
	dt r5
	bf.s .Lpk_scaled_key
	add #4,r4
	rts
	nop
	.size _sat_paint_keys_spread_sh2,.-_sat_paint_keys_spread_sh2
	.text
	.section .text.sat_paint_keys_quantise_sh2,"ax",@progbits
	.align 2
	.global _sat_paint_keys_quantise_sh2
	.type _sat_paint_keys_quantise_sh2,@function
_sat_paint_keys_quantise_sh2:
	mov.l @(4,r4),r5
	mov.l @(8,r4),r6
	mov.l @(12,r4),r7
	mov.l @(16,r4),r3
	tst r5,r5
	bt.s .Lpk_quantise_done
	mov.l @r4,r4
	not r6,r2
	.align 2
.Lpk_quantise_key:
	mov.l @r4,r0
	cmp/eq #-1,r0
	bt .Lpk_quantise_next
	mov r0,r1
	and r6,r1
	dmulu.l r7,r1
	and r2,r0
	sts mach,r1
	cmp/hi r3,r1
	bt .Lpk_quantise_cap
.Lpk_quantise_store:
	or r1,r0
	mov.l r0,@r4
.Lpk_quantise_next:
	dt r5
	bf.s .Lpk_quantise_key
	add #4,r4
.Lpk_quantise_done:
	rts
	nop
.Lpk_quantise_cap:
	bra .Lpk_quantise_store
	mov r3,r1
	.size _sat_paint_keys_quantise_sh2,.-_sat_paint_keys_quantise_sh2
	.text


/* C1 experimental prefix: pairs are [low bucket:high bucket] on the
 * big-endian Saturn. Walk words backward. running never exceeds 65535.
 * r4 pointer, r5 words, r6 running, r7 pair, r0/r1/r2/r3 temporaries. */
	.section .text.sat_paint_prefix_sh2,"ax",@progbits
	.align 2
	.global _sat_paint_prefix_sh2
	.type _sat_paint_prefix_sh2,@function
_sat_paint_prefix_sh2:
	tst r5,r5
	bt .Lpc_prefix_done
	mov r5,r0
	shll2 r0
	add r0,r4
	mov #0,r6
.Lpc_prefix_loop:
	add #-4,r4
	mov.l @r4,r7
	extu.w r7,r0
	mov r6,r2
	add r0,r6
	swap.w r7,r1
	extu.w r1,r1
	mov r6,r3
	add r1,r6
	swap.w r3,r3
	or r2,r3
	mov.l r3,@r4
	dt r5
	bf .Lpc_prefix_loop
.Lpc_prefix_done:
	rts
	nop
	.size _sat_paint_prefix_sh2,.-_sat_paint_prefix_sh2

/* Stable scatter: ascending face index, unsigned halfword cursor.
 * r4 keys, r5 remaining, r6 output, r7 starts, r2 face index.
 * Keep byte/halfword stores separate; arbitrary output alignment for
 * uint8_t, natural 2-byte alignment for uint16_t. No extra scratch. */
	.section .text.sat_paint_place8_sh2,"ax",@progbits
	.align 2
	.global _sat_paint_place8_sh2
	.type _sat_paint_place8_sh2,@function
_sat_paint_place8_sh2:
	tst r5,r5
	bt.s .Lpc_place8_done
	mov #0,r2
.Lpc_place8_loop:
	mov.l @r4+,r0
	cmp/eq #-1,r0
	bt .Lpc_place8_next
	add r0,r0
	mov.w @(r0,r7),r1
	extu.w r1,r1
	mov r1,r3
	add #1,r3
	mov.w r3,@(r0,r7)
	mov r1,r0
	mov.b r2,@(r0,r6)
.Lpc_place8_next:
	dt r5
	bf.s .Lpc_place8_loop
	add #1,r2
.Lpc_place8_done:
	rts
	nop
	.size _sat_paint_place8_sh2,.-_sat_paint_place8_sh2
	.section .text.sat_paint_place16_sh2,"ax",@progbits
	.align 2
	.global _sat_paint_place16_sh2
	.type _sat_paint_place16_sh2,@function
_sat_paint_place16_sh2:
	tst r5,r5
	bt.s .Lpc_place16_done
	mov #0,r2
.Lpc_place16_loop:
	mov.l @r4+,r0
	cmp/eq #-1,r0
	bt .Lpc_place16_next
	add r0,r0
	mov.w @(r0,r7),r1
	extu.w r1,r1
	mov r1,r3
	add #1,r3
	mov.w r3,@(r0,r7)
	mov r1,r0
	add r0,r0
	mov.w r2,@(r0,r6)
.Lpc_place16_next:
	dt r5
	bf.s .Lpc_place16_loop
	add #1,r2
.Lpc_place16_done:
	rts
	nop
	.size _sat_paint_place16_sh2,.-_sat_paint_place16_sh2
	.text
