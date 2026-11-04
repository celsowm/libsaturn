/* The ADX inner loop in SH-2 assembly: 32 samples of one channel (adx.hpp, decode_block_to).
 *
 * GCC builds the C loop with MUL.L, reloads its constants and spills the history to the stack: about 38 instructions
 * a sample. Here the coefficients, the history, the floor bias and the clamp bounds stay in registers, the products
 * are MULS.W (16 x 16, the history is clamped to 16 bits), and the clamps fall through when the sample is in range:
 * 23 instructions a sample. The arithmetic is the C loop's, sample for sample:
 *   sample = clamp(steps[n] + ((c0 * p1 + c1 * p2 + 2^31) >>> 12) - 2^19), p2 = p1, p1 = sample.
 *
 * The frame's scale is read here too and the 16 steps (each nibble's delta times the scale) built unrolled on this
 * function's stack: the caller has already rejected the end marker (scale bit 15).
 *
 * void sat_adx_decode_channel_s16be(const uint8_t* frame, const int16_t* coefficients, int16_t* history, int16_t* dest)
 *   frame: the 18-byte frame, big-endian scale first; coefficients: {c0, c1}; history: {p1, p2}, written back;
 *   dest: 32 S16BE samples, 2-byte aligned. */
#if defined(__sh__)
__asm__(
    "    .text\n"
    "    .align 2\n"
    "    .global _sat_adx_decode_channel_s16be\n"
    "_sat_adx_decode_channel_s16be:\n"
    "    mov.l   r8,@-r15\n"
    "    mov.l   r9,@-r15\n"
    "    mov.l   r10,@-r15\n"
    "    mov.l   r11,@-r15\n"
    "    mov.l   r12,@-r15\n"
    "    mov.l   r13,@-r15\n"
    "    mov.l   r14,@-r15\n"
    "    sts.l   macl,@-r15\n"
    "    mov.l   r6,@-r15\n"
    "    mov.w   @r5,r8\n"            /* c0 */
    "    mov.w   @(2,r5),r0\n"
    "    mov     r0,r9\n"             /* c1 */
    "    mov.w   @r6,r10\n"           /* p1 */
    "    mov.w   @(2,r6),r0\n"
    "    mov     r0,r11\n"            /* p2 */
    "    mov.b   @r4+,r0\n"
    "    mov.b   @r4+,r1\n"
    "    extu.b  r0,r0\n"
    "    shll8   r0\n"
    "    extu.b  r1,r1\n"
    "    or      r1,r0\n"             /* scale */
    "    add     #-64,r15\n"          /* steps[16]: n = 0..7 is n * scale, 8..15 is (n - 16) * scale */
    "    mov     #0,r1\n"
    "    mov.l   r1,@r15\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(4,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(8,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(12,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(16,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(20,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(24,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(28,r15)\n"
    "    add     r0,r1\n"             /* 8 * scale */
    "    neg     r1,r1\n"
    "    mov.l   r1,@(32,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(36,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(40,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(44,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(48,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(52,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(56,r15)\n"
    "    add     r0,r1\n"
    "    mov.l   r1,@(60,r15)\n"
    "    mov     r15,r5\n"
    "    mov.l   .Ladx_bias,r12\n"    /* 2^31 */
    "    mov.l   .Ladx_unbias,r13\n"  /* -2^19 */
    "    mov.w   .Ladx_max,r14\n"     /* 32767 */
    "    mov.w   .Ladx_min,r6\n"      /* -32768 */
    "    mov     #16,r3\n"
    ".Ladx_loop:\n"
    "    mov.b   @r4+,r0\n"
    "    muls.w  r8,r10\n"
    "    mov     r0,r2\n"
    "    and     #0xF0,r0\n"
    "    shlr2   r0\n"
    "    mov.l   @(r0,r5),r1\n"       /* steps[high nibble] */
    "    sts     macl,r0\n"
    "    muls.w  r9,r11\n"
    "    mov     r10,r11\n"           /* p2 = p1 */
    "    add     r12,r0\n"
    "    sts     macl,r10\n"
    "    add     r10,r0\n"
    "    shlr8   r0\n"
    "    shlr2   r0\n"
    "    shlr2   r0\n"
    "    add     r13,r0\n"
    "    add     r1,r0\n"
    "    cmp/gt  r14,r0\n"
    "    bt      .Ladx_high1\n"
    "    cmp/ge  r6,r0\n"
    "    bf      .Ladx_low1\n"
    ".Ladx_done1:\n"
    "    mov     r0,r10\n"            /* p1 = sample */
    "    mov.w   r0,@r7\n"
    "    mov     r2,r0\n"
    "    and     #15,r0\n"
    "    muls.w  r8,r10\n"
    "    shll2   r0\n"
    "    mov.l   @(r0,r5),r1\n"       /* steps[low nibble] */
    "    sts     macl,r0\n"
    "    muls.w  r9,r11\n"
    "    mov     r10,r11\n"
    "    add     r12,r0\n"
    "    sts     macl,r10\n"
    "    add     r10,r0\n"
    "    shlr8   r0\n"
    "    shlr2   r0\n"
    "    shlr2   r0\n"
    "    add     r13,r0\n"
    "    add     r1,r0\n"
    "    cmp/gt  r14,r0\n"
    "    bt      .Ladx_high2\n"
    "    cmp/ge  r6,r0\n"
    "    bf      .Ladx_low2\n"
    ".Ladx_done2:\n"
    "    mov     r0,r10\n"
    "    mov.w   r0,@(2,r7)\n"
    "    dt      r3\n"
    "    bf.s    .Ladx_loop\n"
    "    add     #4,r7\n"
    "    add     #64,r15\n"
    "    mov.l   @r15+,r1\n"          /* history */
    "    mov.w   r10,@r1\n"
    "    mov     r11,r0\n"
    "    mov.w   r0,@(2,r1)\n"
    "    lds.l   @r15+,macl\n"
    "    mov.l   @r15+,r14\n"
    "    mov.l   @r15+,r13\n"
    "    mov.l   @r15+,r12\n"
    "    mov.l   @r15+,r11\n"
    "    mov.l   @r15+,r10\n"
    "    mov.l   @r15+,r9\n"
    "    rts\n"
    "    mov.l   @r15+,r8\n"
    ".Ladx_high1:\n"
    "    bra     .Ladx_done1\n"
    "    mov     r14,r0\n"
    ".Ladx_low1:\n"
    "    bra     .Ladx_done1\n"
    "    mov     r6,r0\n"
    ".Ladx_high2:\n"
    "    bra     .Ladx_done2\n"
    "    mov     r14,r0\n"
    ".Ladx_low2:\n"
    "    bra     .Ladx_done2\n"
    "    mov     r6,r0\n"
    "    .align 1\n"
    ".Ladx_max:\n"
    "    .short  32767\n"
    ".Ladx_min:\n"
    "    .short  -32768\n"
    "    .align 2\n"
    ".Ladx_bias:\n"
    "    .long   0x80000000\n"
    ".Ladx_unbias:\n"
    "    .long   0xFFF80000\n");
#endif
