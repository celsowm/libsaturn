#!/usr/bin/env python3
"""Generate a deterministic resident PCM8 bank for fighting-game effects."""
import argparse
import math
from pathlib import Path
import random
import wave

SAMPLE_RATE=11025
PEAK=24
DURATIONS={'punch':0.12,'kick':0.20,'damage':0.18,'bell':0.80}
BELL_STRIKE_SECONDS=(0.0,0.18)

def effect(name,rate=SAMPLE_RATE):
    rng=random.Random(0x534154)
    count=round(DURATIONS[name]*rate)
    phase=0.0
    previous=0.0
    samples=[]
    for i in range(count):
        t=i/rate
        if name=='bell':
            value=0.0
            for start in BELL_STRIKE_SECONDS:
                age=t-start
                if age>=0:
                    attack=min(1.0,age/0.002)
                    value+=attack*math.exp(-age*8.0)*sum(
                        weight*math.sin(2*math.pi*frequency*age)
                        for frequency,weight in ((960,0.48),(1537,0.28),(2471,0.16),(3523,0.08)))
        else:
            noise=rng.uniform(-1,1)
            previous=previous*0.55+noise*0.45
            frequency={'punch':140,'kick':90,'damage':70}[name]
            phase+=2*math.pi*frequency*math.exp(-t*7)/rate
            attack=min(1.0,t/0.002)
            envelope=attack*math.exp(-t*({'punch':35,'kick':24,'damage':28}[name]))
            value=envelope*(0.6*math.sin(phase)+0.4*previous)
            if name=='kick':value+=noise*math.sin(math.pi*i/count)**2*0.12
            if name=='damage':value+=noise*math.exp(-t*55)*0.18*attack
        # Fade the tail to exact silence; no discontinuity at one-shot expiry.
        release=min(1.0,(count-1-i)/max(1,rate//100))
        samples.append(max(-PEAK,min(PEAK,round(value*PEAK*release))))
    return samples

def generate(prefix,rate=SAMPLE_RATE):
    prefix.parent.mkdir(parents=True,exist_ok=True)
    header=['#pragma once','#include <stdint.h>',f'#define COMBAT_SFX_RATE {rate}u']
    source=[f'#include "{prefix.name}.h"']
    for name in DURATIONS:
        samples=effect(name,rate)
        symbol='combat_sfx_'+name
        header.append(f'#define {symbol.upper()}_COUNT {len(samples)}u')
        header.append(f'extern const int8_t {symbol}[{len(samples)}];')
        source.append(f'const int8_t {symbol}[{len(samples)}]={{')
        source.extend('    '+','.join(map(str,samples[i:i+32]))+',' for i in range(0,len(samples),32))
        source.append('};')
        # Standalone previews for reviewing the effects without an emulator.
        with wave.open(str(prefix.parent/(name+'.wav')),'wb') as wav:
            wav.setnchannels(1);wav.setsampwidth(1);wav.setframerate(rate)
            wav.writeframes(bytes(sample+128 for sample in samples))
    prefix.with_suffix('.h').write_text('\n'.join(header)+'\n')
    prefix.with_suffix('.c').write_text('\n'.join(source)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-prefix',type=Path,required=True)
    args=parser.parse_args();generate(args.out_prefix)
