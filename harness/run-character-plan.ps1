[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Bios,
    [ValidateSet('all','viewer','arena','rate','collision','stress','prepare','materials')][string]$Scenario='all',
    [switch]$Pal,
    [switch]$AnalyzeOnly
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cases=if($Scenario -eq 'all'){@('viewer','arena','collision','stress','materials')}else{@($Scenario)}
$oldVerify=$env:CHARACTER_DEMO_VERIFY_GEOMETRY
Push-Location $root
try {
    foreach($case in $cases) {
        $env:CHARACTER_DEMO_VERIFY_GEOMETRY=if($case -eq 'materials'){'1'}else{$oldVerify}
        $example=@{viewer='skeletal_animation_3d';arena='fighter_arena_3d';rate='fighter_arena_3d';collision='fighter_arena_3d';stress='skinning_stress_3d';prepare='skinning_stress_3d';materials='skinning_stress_3d'}[$case]
        $suffix=if($Pal){'_pal'}else{''}
        $fieldHz=if($Pal){50}else{60}
        $prefix="harness/build/character_$case$suffix"; $map=$prefix+'.map'
        if(-not $AnalyzeOnly) {
            & .\build-example.ps1 $example
            if($LASTEXITCODE){throw "Build failed: $example"}
            Copy-Item -LiteralPath ('build/'+$example+'.map') -Destination $map
        }
        $pad=@('0 NONE');$frames=0;$shots=@();$dumps=@()
        switch($case) {
            viewer {
                $frames=1860
                $pad=@('0 NONE','120 A','160 NONE','240 A','280 NONE','360 A','400 NONE',
                    '480 A','520 NONE','600 A','640 NONE','720 START','760 NONE',
                    '880 START','920 NONE','960 C','1000 NONE','1040 Y','1080 NONE',
                    '1120 R','1160 NONE','1200 Z','1240 NONE','1280 RIGHT','1360 NONE',
                    '1400 A','1440 NONE','1520 A','1560 NONE','1640 A','1680 NONE')
                $shots=@("389:${prefix}_jump.png","509:${prefix}_punch.png","629:${prefix}_kick.png",
                    "800:${prefix}_paused_a.png","840:${prefix}_paused_b.png","1009:${prefix}_rgb.png",
                    "1459:${prefix}_guard.png","1579:${prefix}_hit.png","1739:${prefix}_knockdown.png",
                    "1859:${prefix}_final.png")
            }
            arena {
                $frames=1150
                $pad=@('0 NONE','100 B','200 NONE','220 A','420 C','600 NONE','640 X','680 NONE',
                    '720 UP','780 NONE','800 Z','840 NONE','860 Y','900 NONE',
                    '920 Y','940 NONE','960 START','1000 NONE')
                $shots=@("199:${prefix}_guard.png","299:${prefix}_punch.png","519:${prefix}_kick.png",
                    "669:${prefix}_jump.png","819:${prefix}_collision.png",
                    "879:${prefix}_low.png","959:${prefix}_high.png","1149:${prefix}_restart.png")
                $dumps=@("0x00200000:1048576:${prefix}_low.bin")
            }
            stress {
                $frames=3100
                $pad=@('0 NONE')
                foreach($field in @(180,380,580,980,1180,1380,1580,1980,2180,2380,2580)) {
                    $pad+=@("$field A",("{0} NONE" -f ($field+40)))
                }
                foreach($field in @(780,1780)){$pad+=@("$field Z",("{0} NONE" -f ($field+40)))}
                $pad+=@('2800 B','2840 NONE','2900 B','2940 NONE','3000 X','3040 NONE')
                $pad=$pad|Sort-Object { [int]($_.Split(' ')[0]) }
                $shots=@("699:${prefix}_eight_auto.png","1699:${prefix}_eight_master.png",
                    "2699:${prefix}_eight_slave.png","3099:${prefix}_complex.png")
                $dumps=@("0x00200000:1048576:${prefix}_low.bin")
            }
            rate {
                $frames=1200
                $pad=@('0 NONE','40 Y','80 NONE','100 B','200 NONE','220 A','420 C','600 NONE',
                    '640 X','680 NONE','720 UP','780 NONE','960 START','1000 NONE')
                $shots=@("299:${prefix}_low_punch.png","699:${prefix}_low_jump.png","1199:${prefix}_final.png")
                $dumps=@("0x00200000:1048576:${prefix}_low.bin")
            }
            collision {
                $frames=750
                $pad=@('0 NONE','100 RIGHT','600 NONE')
                $shots=@("450:${prefix}_blocked.png","749:${prefix}_final.png")
            }
            materials {
                $frames=800
                $pad=@('0 NONE','100 A','140 NONE','220 C','260 NONE','340 Z','380 NONE',
                    '460 R','560 NONE','600 Z','640 NONE','680 X','720 NONE')
                $shots=@("299:${prefix}_rgb_auto.png","759:${prefix}_rgb_slave.png")
            }
            prepare {
                $frames=1400
                $pad=@('0 NONE','100 A','140 NONE','500 Z','540 NONE','900 Z','940 NONE')
                $shots=@("399:${prefix}_two_auto.png")
                $dumps=@("0x00200000:1048576:${prefix}_low.bin")
            }
        }
        if(-not $AnalyzeOnly){$pad|Set-Content -LiteralPath ($prefix+'.pad')}
        $arguments=@{Example=$example;Bios=$Bios;Frames=$frames;PadScript=$prefix+'.pad';
            Screenshot=$shots;DumpWramHigh=$prefix+'_high.bin';Out=$prefix+'.json'}
        if($dumps.Count){$arguments.DumpMem=$dumps}
        if($Pal){$arguments.Pal=$true}
        if(-not $AnalyzeOnly) {
            & .\harness\run-harness.ps1 @arguments 2>&1 | Tee-Object -FilePath ($prefix+'_run.log')
            if($LASTEXITCODE){throw "Emulation failed: $example"}
        } else {Write-Host "[character-plan] Analyzing saved capture: $prefix"}
        $analysis=@('--example',$example,'--map',$map,'--high',($prefix+'_high.bin'),
                    '--probe',($prefix+'.json'),'--output',($prefix+'_analysis.json'),'--field-hz',$fieldHz)
        if($case -in @('stress','arena','rate')){$analysis+=@('--low',($prefix+'_low.bin'))}
        if($case -eq 'rate'){$analysis+=@('--expected-lods','2')}
        if($case -eq 'collision'){$analysis+='--body-collision'}
        if($case -eq 'prepare') {
            & python harness/tests/character_preparation.py $prefix --field-hz $fieldHz
        } else {
            & python harness/tests/character_plan.py @analysis
        }
        if($LASTEXITCODE){throw "Character validation failed: $example"}
        if($case -eq 'rate') {
            # Two display fields per submission: 30/s NTSC, 25/s PAL.
            $targetFieldsPerSubmission=2
            $rateTolerance=1
            $minimumRate=$fieldHz/$targetFieldsPerSubmission-$rateTolerance
            & python -c "import json; from pathlib import Path; d=json.loads(Path('$prefix'+'_analysis.json').read_text()); r=next(w for w in d['workloads'] if w['lod']==2); assert r['samples']>300; assert r['submitted_lists_per_second']>=$minimumRate"
            if($LASTEXITCODE){throw 'Sustained LOW arena submission target failed'}
        }
        if($case -eq 'materials') {
            & python -c "import json; from pathlib import Path; d=json.loads(Path('$prefix'+'_analysis.json').read_text()); assert d['geometry']['verified']>20; assert d['geometry']['errors']==0"
            if($LASTEXITCODE){throw 'Material geometry equivalence failed'}
            $env:CHARACTER_DEMO_VERIFY_GEOMETRY=$oldVerify
            # Leave the ordinary runnable artifact with the caller's settings.
            if(-not $AnalyzeOnly) {
                & .\build-example.ps1 $example
                if($LASTEXITCODE){throw 'Restoring the standard material build failed'}
            }
        }
        if($case -eq 'viewer') {
            & python -c "from PIL import Image; from pathlib import Path; a=Image.open(Path('$prefix'+'_paused_a.png')).crop((130,60,175,185)); b=Image.open(Path('$prefix'+'_paused_b.png')).crop((130,60,175,185)); assert a.tobytes()==b.tobytes(), 'Paused scene changed'"
            if($LASTEXITCODE){throw 'Paused scene pixel comparison failed'}
        }
    }
} finally { $env:CHARACTER_DEMO_VERIFY_GEOMETRY=$oldVerify; Pop-Location }
