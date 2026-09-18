/* cd_block_probe - bounded BIOS-initialized CD Block read acceptance probe. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/cd_block.h"
#include "saturn/color.h"
#include "saturn/core.h"

static uint16_t color_for_status(sat_result_t status) {
    if (status == SAT_ERR_TIMEOUT) return SAT_COLOR_YELLOW;
    if (status == SAT_ERR_BUSY) return SAT_COLOR_MAGENTA;
    return SAT_COLOR_RED;
}

int main(void) {
    const sat_video_config_t video = {320u, 224u, 1u, 0u};
    sat_cd_block_t block;
    uint8_t sector[SAT_CD_SECTOR_BYTES];
    sat_result_t status = sat_init(&video);
    uint16_t failure_color = SAT_COLOR_MAGENTA;
    if (status == SAT_OK) {
        status = sat_cd_block_init(&block, SAT_CD_BLOCK_DEFAULT_TIMEOUT);
        if (status != SAT_OK) failure_color = color_for_status(status);
    }
    if (status == SAT_OK) {
        status = sat_cd_block_read_sectors(&block, 16u, 1u, sector);
        if (status != SAT_OK) failure_color = color_for_status(status);
    }
    if (status == SAT_OK &&
        (sector[1] != 'C' || sector[2] != 'D' || sector[3] != '0' ||
         sector[4] != '0' || sector[5] != '1')) {
        status = SAT_ERR_IO;
        failure_color = SAT_COLOR_MAGENTA;
    }

    for (uint16_t frame = 0u; frame < 120u; ++frame) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad) != SAT_OK) break;
        (void)sat_set_clear_color(status == SAT_OK ? SAT_COLOR_GREEN : failure_color);
        (void)sat_app_frame_end();
    }
    return status == SAT_OK ? 0 : 1;
}
