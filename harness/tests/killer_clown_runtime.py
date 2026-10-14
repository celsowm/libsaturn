"""Validate combat, two controllers, dynamic fence coverage and stock RAM."""
import argparse
import json
import struct
import array
import sys
import wave
from pathlib import Path
from PIL import Image, ImageChops
from character_plan import symbol, HIGH_BASE
from clown_profile import geometry_metrics, lighting_metrics
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from generate_combat_audio import effect, DURATIONS


def analyze(prefix,smoke=False):
    path = lambda suffix: Path(str(prefix) + suffix)
    mapping = path('.map').read_text()
    high = path('_high.bin').read_bytes()
    geometry = geometry_metrics(mapping, high)
    lighting_jobs = lighting_metrics(mapping, high)
    assert lighting_jobs['updates'] > 20, lighting_jobs
    def words(name, count=1):
        return struct.unpack_from('>' + str(count) + 'I', high, symbol(mapping, name) - HIGH_BASE)
    panic = words('g_sat_last_panic', 3)
    assert not any(panic), panic
    names = ('frames simulation_ticks hits blocks knockdowns body_overlaps '
             'stage_faces actor_faces commands draw_errors slave_jobs round_resets '
             'two_player paused submissions_tenths pose_changes fence_mode '
             'fence_mesh_frames fence_hidden_frames fence_solid_frames second_pad_buttons '
             'draw_timeouts late_swaps ui_state menu_selection menu_visits options_visits '
             'controls_visits ground_updates lights_enabled light_updates').split()
    metrics = dict(zip(names, words('g_clown_metrics', len(names))))
    assert metrics['frames'] > (10 if smoke else 150) and metrics['simulation_ticks'] > (200 if smoke else 1200), metrics
    assert metrics['draw_errors'] == 0 and metrics['body_overlaps'] == 0, metrics
    assert metrics['draw_timeouts'] == 0, metrics
    assert metrics['hits'] > 0 and metrics['blocks'] > 0, metrics
    assert metrics['slave_jobs'] > (10 if smoke else 100), metrics
    assert metrics['paused'] == 0 and metrics['ui_state']==1, metrics
    assert 0 < metrics['commands'] < 2048, metrics
    assert metrics['fence_mode'] == 0, metrics
    if smoke:
        assert metrics['fence_mesh_frames']==0 and metrics['fence_hidden_frames']>0, metrics
    else:
        assert metrics['pose_changes']>10 and metrics['round_resets']==3 and metrics['two_player']==1, metrics
        assert all(metrics[name]>0 for name in ('fence_mesh_frames','fence_hidden_frames','fence_solid_frames')), metrics
        assert metrics['second_pad_buttons']&((1<<14)|(1<<4)|(1<<5))==((1<<14)|(1<<4)|(1<<5)), metrics
        assert metrics['menu_visits']>=3 and metrics['options_visits']==1 and metrics['controls_visits']==1, metrics
    assert metrics['ground_updates']>(10 if smoke else 100) and metrics['light_updates']>(10 if smoke else 100), metrics
    assert metrics['lights_enabled']==1, metrics
    light_names=('updates visible_red visible_blue max_visible_red max_visible_blue '
                 'red_strength blue_strength cone_faces pool_faces phase').split()
    lighting=dict(zip(light_names,words('g_clown_light_metrics',len(light_names))))
    assert lighting['max_visible_red']>10 and lighting['max_visible_blue']>10, lighting
    assert lighting['red_strength']>0 and lighting['blue_strength']>0, lighting
    assert 8<=lighting['cone_faces']<=32 and lighting['pool_faces']==2, lighting
    regs=path('_vdp2.bin').read_bytes()
    assert struct.unpack_from('>H',regs,0x20)[0]&0x11==0x11, 'NBG0/RBG0 disabled'
    assert struct.unpack_from('>H',regs,0xec)[0]&0x140==0x140, 'sprite additive calculation disabled'
    commands=path('_vdp1.bin').read_bytes()
    shaded=0;cutout=0;pools=0;beams=0;red_peak=0;blue_peak=0
    for offset in range(0,len(commands),32):
        command=struct.unpack_from('>16H',commands,offset)
        if command[0]&0x8000:break
        if command[0]&15==2 and command[2]&63==12:
            assert command[14]>=65536//8, command
            corners=struct.unpack_from('>4H',commands,command[14]*8)
            red_peak=max(red_peak,max(color&31 for color in corners))
            blue_peak=max(blue_peak,max((color>>10)&31 for color in corners))
            shaded+=1
        if command[0]&15==2 and command[2]&0x1ff==0x88:
            assert command[14]==0, command
            width=((command[5]>>8)&63)*8;height=command[5]&255
            assert (width,height)==(64,64), command
            pixels=commands[command[4]*8:command[4]*8+width*height//2]
            assert any(value==0 for value in pixels) and any(value!=0 for value in pixels)
            lut=struct.unpack_from('>16H',commands,command[3]*8)
            assert lut[0]==0 and lut[1]&0x8000
            cutout+=1
        if command[0]&15==2 and command[2]&0x38==0x20 and command[3]&0x7800==0x4000:
            assert command[2]&0x138==0x20, command
            width=((command[5]>>8)&63)*8;height=command[5]&255
            pixels=commands[command[4]*8:command[4]*8+width*height]
            if max(pixels)>=32:pools+=1
            else:
                assert sum(value!=0 for value in pixels)>len(pixels)*3//4, 'dithered beam texture'
                beams+=1
    assert shaded>100, 'missing LUT4 RGB CLUT Gouraud fighter commands'
    assert red_peak>20 and blue_peak>20, 'spotlight corrections never reached fighter command tables'
    assert cutout>0, 'missing non-mesh LUT4 cutout fence commands'
    assert pools==2, 'missing VDP2-blended ground pools'
    assert beams>=8, 'missing continuous additive beams'
    audio_names=('punch kick damage bell failures running executed queued max_lateness '
                 'tick active_voices peak_voices').split()
    audio=dict(zip(audio_names,words('g_clown_audio',len(audio_names))))
    assert all(audio[name]>0 for name in DURATIONS), audio
    assert audio['bell']>=(1 if smoke else 2) and audio['failures']==0 and audio['running']==1, audio
    assert audio['max_lateness']<=2 and 0<audio['peak_voices']<=8, audio
    sound_ram=path('_sound.bin').read_bytes()
    executed=struct.unpack_from('>H',sound_ram,0x814)[0]
    assert executed==sum(audio[name] for name in DURATIONS), audio
    sample_addresses={}
    for name in DURATIONS:
        offset=sound_ram.find(bytes(value&255 for value in effect(name)))
        assert offset>=0x3000, (name,offset)
        sample_addresses[name]=offset
    pcm=array.array('h');pcm.frombytes(path('.raw').read_bytes())
    if sys.byteorder!='little':pcm.byteswap()
    peak=max(map(abs,pcm))
    assert 100<peak<32767 and sum(value!=0 for value in pcm)>1000, peak
    with wave.open(str(path('.wav')),'wb') as wav:
        wav.setnchannels(2);wav.setsampwidth(2);wav.setframerate(44100)
        wav.writeframes(path('.raw').read_bytes())
    probe = json.loads(path('.json').read_text())
    assert probe['ram_cartridge']['type'] == 'none'
    distinct=None
    if not smoke:
        paused_a = Image.open(path('_paused_a.png')).convert('RGB')
        paused_b = Image.open(path('_paused_b.png')).convert('RGB')
        area = (0, 55, paused_a.width, 185)
        assert ImageChops.difference(paused_a.crop(area), paused_b.crop(area)).getbbox() is None
        fence_area=(0,80,paused_a.width,185)
        hashes = {Image.open(path('_' + mode + '.png')).convert('RGB').crop(fence_area).tobytes()
                  for mode in ('solid', 'cutaway', 'auto')}
        assert len(hashes)==3, 'fence modes do not produce distinct captures'
        distinct=len(hashes)
    result = {'panic': list(panic), 'metrics': metrics, 'distinct_fence_images':distinct,
              'lut4_gouraud_commands': shaded, 'cutout_fence_commands':cutout, 'audio':audio,
              'lighting':lighting,'geometry_execution':geometry,'lighting_execution':lighting_jobs,'blended_floor_pools':pools,'additive_beam_commands':beams,
              'fighter_gouraud_red_peak':red_peak,'fighter_gouraud_blue_peak':blue_peak,
              'sound_ram_samples':sample_addresses,'captured_audio_peak':peak,
              'driver_executed_events':executed}
    path('_analysis.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--smoke',action='store_true',help='Validate the default fence, lighting and audio in a short CPU match.')
    args=parser.parse_args();analyze(args.prefix,args.smoke)
