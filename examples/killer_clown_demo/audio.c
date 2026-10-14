/* Resident samples; the SCSP 68000 performs every effect's scheduled key-on. */
#include "audio.h"
#include "saturn/audio.h"
#include "saturn/sound_driver.h"
#include "example_util.h"
#include "combat_sfx.h"
enum { EFFECT_VOLUME=208, PAN_PER_UNIT=4, DRIVER_LEAD_TICKS=1 };
static sat_sound_t sounds[CLOWN_SOUND_COUNT];
volatile clown_audio_metrics_t g_clown_audio;
void clown_audio_init(void) {
    static const int8_t* const samples[]={combat_sfx_punch,combat_sfx_kick,combat_sfx_damage,combat_sfx_bell};
    static const uint32_t counts[]={COMBAT_SFX_PUNCH_COUNT,COMBAT_SFX_KICK_COUNT,COMBAT_SFX_DAMAGE_COUNT,COMBAT_SFX_BELL_COUNT};
    sat_example_must(sat_audio_init());
    sat_example_must(sat_sound_driver_start());
    for(unsigned i=0;i<CLOWN_SOUND_COUNT;++i) {
        sat_sound_desc_t desc={0};desc.samples=samples[i];desc.sample_count=counts[i];
        desc.sample_rate=COMBAT_SFX_RATE;desc.format=SAT_AUDIO_PCM_S8;
        sat_example_must(sat_sound_create(&sounds[i],&desc));
    }
}
void clown_audio_play(unsigned effect,sat_fx16_t x) {
    if(effect>=CLOWN_SOUND_COUNT)return;
    sat_sound_play_params_t params={0};params.volume=EFFECT_VOLUME;
    int pan=x*PAN_PER_UNIT/SAT_FX16_ONE;
    if(pan<SAT_AUDIO_PAN_LEFT)pan=SAT_AUDIO_PAN_LEFT;
    if(pan>SAT_AUDIO_PAN_RIGHT)pan=SAT_AUDIO_PAN_RIGHT;
    params.pan=(int16_t)pan;params.priority=(uint16_t)(effect+1);
    params.pitch=SAT_FX16_ONE;params.flags=SAT_SOUND_PLAY_AT_TICK;
    params.start_tick=sat_sound_driver_tick()+DRIVER_LEAD_TICKS;
    sat_result_t status=sat_sound_play(sounds[effect],&params,NULL);
    if(status==SAT_OK)++g_clown_audio.effects[effect];
    else ++g_clown_audio.failures;
}
void clown_audio_update(void) {
    sat_example_must(sat_audio_update());
    sat_sound_driver_info_t info;
    sat_example_must(sat_sound_driver_info(&info));
    g_clown_audio.running=info.running;g_clown_audio.executed=info.executed;
    g_clown_audio.queued=info.queued;g_clown_audio.max_lateness=info.max_lateness_ticks;
    g_clown_audio.tick=info.tick;
    sat_audio_stats_t stats;sat_example_must(sat_audio_get_stats(&stats));
    g_clown_audio.active_voices=stats.active_voices;
    if(stats.active_voices>g_clown_audio.peak_voices)g_clown_audio.peak_voices=stats.active_voices;
}
