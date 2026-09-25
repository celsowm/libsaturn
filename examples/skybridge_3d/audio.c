/* Skybridge 3D: synthesised sound effects, music loop and game-event cues. */
#include "skybridge.h"

static sat_sound_t g_sounds[SOUNDS];
static sat_voice_t g_music_voice;
static const char* const g_sfx_paths[SOUNDS-1u]={
    "skybridge/sfx/jump", "skybridge/sfx/land", "skybridge/sfx/pickup",
    "skybridge/sfx/fall", "skybridge/sfx/finish"
};
static int8_t g_audio[SOUNDS - 1u][SOUND_LEN] __attribute__((section(".wram_l")));
static int8_t g_music[MUSIC_LEN] __attribute__((section(".wram_l")));
static uint8_t g_audio_ready;

void init_audio(void) {
    uint32_t i;
    uint8_t j;
    const uint8_t tone_step[SOUNDS-1u]={3u,5u,7u,2u,9u};
    sat_sound_play_params_t music_params={76u,0,1u,0u,SB_F(1)};
    sat_example_must(sat_audio_init());
    sat_example_must(sat_audio_set_master_volume(160u));
    for(j=0u;j<SOUNDS-1u;++j) {
        /* The fall cue is the odd one out: noise, not a tone. */
        sat_example_must(j==3u
            ? sat_audio_synth_noise(g_audio[j],SOUND_LEN)
            : sat_audio_synth_blip(g_audio[j],SOUND_LEN,tone_step[j]));
        {
            sat_asset_desc_t asset={0};
            sat_asset_t asset_handle={0};
            asset.logical_path=g_sfx_paths[j];
            asset.kind=SAT_ASSET_SOUND;
            asset.data=g_audio[j];
            asset.size=SOUND_LEN;
            asset.sample_rate=11025u;
            asset.sample_count=SOUND_LEN;
            asset.channels=1u;
            asset.format=SAT_AUDIO_PCM_S8;
            sat_example_must(sat_asset_register(&asset,&asset_handle));
            sat_example_must(sat_sound_load(g_sfx_paths[j],&g_sounds[j]));
        }
        loading_frame("REGISTERING AUDIO",(uint8_t)(82u+(j+1u)*2u));
    }
    {
        /* 8-note, three-second arpeggio, one note per call so the boot panel
         * can report progress between them. */
        static const uint8_t periods[8]={50u,45u,40u,38u,34u,38u,40u,45u};
        const uint32_t note_len=MUSIC_LEN/8u;
        for(i=0u;i<8u;++i) {
            sat_example_must(sat_audio_synth_arpeggio_note(
                &g_music[i*note_len],note_len,periods[i],i*note_len));
            loading_frame("GENERATING MUSIC",
                (uint8_t)(92u+(i+1u)*6u/8u));
        }
    }
    {
        sat_sound_desc_t desc={0};
        desc.samples=g_music;desc.sample_count=MUSIC_LEN;
        desc.sample_rate=11025u;desc.format=SAT_AUDIO_PCM_S8;
        desc.loop=1u;
        sat_example_must(sat_sound_create(&g_sounds[5u],&desc));
        sat_example_must(sat_sound_play(g_sounds[5u],&music_params,&g_music_voice));
    }
    g_audio_ready=1u;
    loading_frame("READY",100u);
}
void sound_event(uint16_t event) {
    uint8_t id;
    sat_sound_play_params_t p={96u,0,8u,0u,SB_F(1)};
    if (!g_audio_ready || !event) return;
    if (event&SB_EVENT_WIN) id=4u;
    else if (event&SB_EVENT_FALL) id=3u;
    else if (event&SB_EVENT_CHECKPOINT) id=2u;
    else if (event&SB_EVENT_PICKUP) id=2u;
    else if (event&SB_EVENT_JUMP) id=0u;
    else if (event&SB_EVENT_LAND) id=1u;
    else if (event&SB_EVENT_WARNING) id=1u;
    else return;
    (void)sat_sound_play(g_sounds[id],&p,0);
}
