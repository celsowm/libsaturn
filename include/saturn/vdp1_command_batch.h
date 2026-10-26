#ifndef SATURN_VDP1_COMMAND_BATCH_H
#define SATURN_VDP1_COMMAND_BATCH_H
#include "saturn/vdp1.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Experimental API: build the SDK with LIBSATURN_NATIVE_SCENE_COMMANDS=ON.
 * Work RAM command records, never pointers into in-flight VDP1 VRAM.
 * Retain the descriptor and every worker's ownership through join/cancel.
 * The caller partitions commands into disjoint ranges before dispatch.
 * Workers publish records with pad=1 AFTER all other words are written;
 * their executor must publish/invalidate those ranges before Master links.
 * Only Master reserves, links, finishes or cancels a batch. */
typedef struct __attribute__((aligned(4))) sat_vdp1_native_command {
    uint16_t ctrl,link,pmod,colr,srca,size;
    int16_t xa,ya,xb,yb,xc,yc,xd,yd;
    uint16_t grda,pad;
} sat_vdp1_native_command_t;

typedef struct sat_vdp1_command_batch {
    sat_vdp1_native_command_t* commands;
    sat_vdp1_command_checkpoint_t checkpoint;
    uint16_t capacity,head,tail;
    uint8_t active;
} sat_vdp1_command_batch_t;

/* One batch per frame. Capacity includes unused holes and respects the HUD
 * and END quotas. Ordinary draws may append cold fallback commands while a
 * batch is open; they join the same traversal through link(). No submission
 * or overlay pass is allowed until finish/cancel. */
sat_result_t sat_vdp1_command_batch_reserve(uint16_t capacity,sat_vdp1_command_batch_t* out);
/* Absolute command ID: checkpoint.used + relative prepared slot, or a cold
 * draw's ID appended after the reserved span in this same frame. */
sat_result_t sat_vdp1_command_batch_link(sat_vdp1_command_batch_t* batch,uint16_t command);
sat_result_t sat_vdp1_command_batch_finish(sat_vdp1_command_batch_t* batch);
sat_result_t sat_vdp1_command_batch_cancel(sat_vdp1_command_batch_t* batch);

#ifdef __cplusplus
}
#endif
#endif
