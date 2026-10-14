#include "diagnostics.h"
#include "saturn/cdfs.h"
#include "saturn/time.h"
#include "saturn/input.h"
#include "src/core/startup/boot_trace.h"
#include "fixture.h"

volatile optical_results_t g_optical_results = {.magic=0x4f455431u,.version=1u,.profile=OET_FULL,.build_id=OET_BUILD_ID};
static const char* const names[OET_STEP_COUNT]={"CD STATUS / INIT","ISO / FIRST FILE",
    "READ 1 SECTOR","READ 2 SECTORS","READ 32 / CRC","30 SECOND READ TEST"};
const char* optical_step_name(unsigned step) { return step<OET_STEP_COUNT?names[step]:"IDLE"; }

#if OET_FULL
static sat_cd_block_t block;
static sat_cd_device_t device;
static sat_cdfs_volume_t volume;
static sat_cdfs_file_t fixture_file;
static uint8_t buffer[OET_FIXTURE_BYTES] __attribute__((aligned(4)));
static uint32_t clock_ms(void* context) { (void)context;return sat_time_ms(); }
static uint32_t crc32(const uint8_t* data,uint32_t size) {
    uint32_t crc=0xffffffffu;
    for(uint32_t i=0;i<size;++i) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static uint8_t expected_byte(uint32_t i) {
    static const uint8_t signature[]="LIBSATURN ODE V1";
    return i<16?signature[i]:(uint8_t)(i*37+(i>>8)*11+0x5a);
}
static sat_result_t ensure_block(void) {
    if(block.initialized)return SAT_OK;
    if(!sat_time_frequency_hz())return SAT_ERR_UNSUPPORTED;
    const sat_cd_block_config_t timing={clock_ms,0,OET_COMMAND_TIMEOUT_MS,OET_OPERATION_TIMEOUT_MS};
    return sat_cd_block_init_ex(&block,&timing);
}
static sat_result_t ensure_iso(void) {
    sat_result_t status=ensure_block();if(status!=SAT_OK)return status;
    if(volume.mounted)return SAT_OK;
    status=sat_cd_block_bind_device(&block,&device,0);if(status!=SAT_OK)return status;
    status=sat_cdfs_mount(&volume,&device);if(status!=SAT_OK)return status;
    sat_cdfs_file_t application;
    status=sat_cdfs_lookup(&volume,"0.BIN",&application);if(status!=SAT_OK)return status;
    status=sat_cdfs_lookup(&volume,"ODETEST.BIN",&fixture_file);if(status!=SAT_OK)return status;
    if(fixture_file.size!=OET_FIXTURE_BYTES || fixture_file.extent_lba<=application.extent_lba)
        return SAT_ERR_IO;
    return SAT_OK;
}
static sat_result_t read_batch(unsigned sectors) {
    sat_result_t status=ensure_iso();if(status!=SAT_OK)return status;
    status=sat_cd_block_read_sectors(&block,fixture_file.extent_lba,sectors,buffer);
    if(status!=SAT_OK)return status;
    const uint32_t bytes=sectors*SAT_CD_SECTOR_BYTES;
    for(uint32_t i=0;i<bytes;++i)if(buffer[i]!=expected_byte(i))return SAT_ERR_IO;
    g_optical_results.crc=crc32(buffer,bytes);
    if(sectors==OET_FIXTURE_SECTORS && g_optical_results.crc!=OET_FIXTURE_CRC)return SAT_ERR_IO;
    return SAT_OK;
}
static void capture(sat_result_t status) {
    sat_cd_block_diagnostics_t d;
    (void)sat_cd_block_get_diagnostics(&block,&d);
    g_optical_results.lba=d.lba;g_optical_results.requested=d.bytes_requested;
    g_optical_results.transferred=d.bytes_transferred;g_optical_results.elapsed_ms=d.elapsed_ms;
    if(status==SAT_ERR_TIMEOUT)++g_optical_results.timeouts;
    g_optical_results.hirq=d.hirq;
    g_optical_results.cr1=d.response[0];g_optical_results.cr2=d.response[1];
    g_optical_results.cr3=d.response[2];g_optical_results.cr4=d.response[3];
    g_optical_results.wait_mask=d.wait_mask;g_optical_results.response_valid=d.response_valid;
    g_optical_results.result=(uint32_t)status;
}
#endif

sat_result_t optical_run_step(unsigned step) {
    if(step>=OET_STEP_COUNT)return SAT_ERR_INVALID_ARG;
    g_optical_results.current=step;
    g_optical_results.cancelled=0u;
    sat_result_t status=SAT_ERR_UNSUPPORTED;
#if OET_FULL
    unsigned stage=step==OET_STEP_STATUS?SAT_BOOT_CD:step==OET_STEP_ISO?SAT_BOOT_ISO:
        step==OET_STEP_STREAM?SAT_BOOT_STREAM:SAT_BOOT_SECTOR;
    sat_boot_mark(stage,0,SAT_OK);
    if(step==OET_STEP_STATUS) {
        status=ensure_block();
        if(status==SAT_OK) {
            const uint16_t cmd[4]={0,0,0,0};uint16_t response[4];
            status=sat_cd_block_command(&block,cmd,response);
        }
    } else if(step==OET_STEP_ISO)status=ensure_iso();
    else if(step<OET_STEP_STREAM)status=read_batch(step==OET_STEP_READ1?1u:step==OET_STEP_READ2?2u:OET_FIXTURE_SECTORS);
    else {
        const uint32_t start=sat_time_ms();
        g_optical_results.sustained_bytes=0;
        do {
            status=read_batch(OET_FIXTURE_SECTORS);
            if(status!=SAT_OK)break;
            g_optical_results.sustained_bytes+=OET_FIXTURE_BYTES;
            /* No rendering/audio dependencies inside the CD transport. */
            sat_pad_state_t pad={0};
            if(sat_pad_poll(&pad)==SAT_OK && (pad.held&SAT_PAD_B)) {
                g_optical_results.cancelled=1u;status=SAT_ERR_BUSY;break;
            }
        } while(sat_time_ms()-start<OET_SUSTAINED_DURATION_MS);
        g_optical_results.sustained_ms=sat_time_ms()-start;
    }
    capture(status);
    sat_boot_mark(stage,status==SAT_OK,status);
#else
    g_optical_results.result=(uint32_t)status;
#endif
    if(status==SAT_OK) { ++g_optical_results.passes;g_optical_results.completed=step; }
    else ++g_optical_results.failures;
    g_optical_results.current=0xffffffffu;
    return status;
}
