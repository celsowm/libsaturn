#ifndef SATURN_CD_MOUNT_H
#define SATURN_CD_MOUNT_H

#include "saturn/cd.h"
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One-call CD bring-up: init the block, optionally set a progress service
 * (the audio-CD sector-decoded hook; pass NULL to skip), bind a device, and
 * mount CDFS. Mirrors the four-line chain every example that boots from
 * disc open-codes. Each output pointer may be NULL to skip that step:
 * a NULL `block` skips init, set_progress_service and bind; a NULL `device`
 * skips bind and mount; a NULL `volume` skips the CDFS mount.
 *
 * `sector_count` is the disc image size in 2048-byte sectors, forwarded to
 * sat_cd_block_bind_device. Some tests pass 0u to size the device lazily
 * from the first read; both the bind step and sat_cd_device_init accept
 * that. The first failing call's status is returned. */
sat_result_t sat_cd_mount_default(
    sat_cd_block_t* block,
    sat_cd_block_progress_fn progress, void* progress_context,
    sat_cd_device_t* device, uint32_t sector_count,
    sat_cdfs_volume_t* volume
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CD_MOUNT_H */
