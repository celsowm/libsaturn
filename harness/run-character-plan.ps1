[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Bios,
    [ValidateSet('all','viewer','arena','collision','stress')][string]$Scenario='all'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cases=if($Scenario -eq 'all'){@('viewer','arena','collision','stress')}else{@($Scenario)}
Push-Location $root
try {
    foreach($case in $cases) {
        $example=@{viewer='skeletal_animation_3d';arena='fighter_arena_3d';collision='fighter_arena_3d';stress='skinning_stress_3d'}[$case]
        & .\build-example.ps1 $example
        if($LASTEXITCODE){throw "Build failed: $example"}
        $prefix="harness/build/character_$case"; $map=$prefix+'.map'
        Copy-Item -LiteralPath ('build/'+$example+'.map') -Destination $map
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
                    '720 UP','780 NONE','800 Z','840 NONE','960 START','1000 NONE')
                $shots=@("199:${prefix}_guard.png","299:${prefix}_punch.png","519:${prefix}_kick.png",
                    "669:${prefix}_jump.png","819:${prefix}_collision.png","1149:${prefix}_restart.png")
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
            collision {
                $frames=750
                $pad=@('0 NONE','100 RIGHT','600 NONE')
                $shots=@("450:${prefix}_blocked.png","749:${prefix}_final.png")
            }
        }
        $pad|Set-Content -LiteralPath ($prefix+'.pad')
        $arguments=@{Example=$example;Bios=$Bios;Frames=$frames;PadScript=$prefix+'.pad';
            Screenshot=$shots;DumpWramHigh=$prefix+'_high.bin';Out=$prefix+'.json'}
        if($dumps.Count){$arguments.DumpMem=$dumps}
        & .\harness\run-harness.ps1 @arguments 2>&1 | Tee-Object -FilePath ($prefix+'_run.log')
        if($LASTEXITCODE){throw "Emulation failed: $example"}
        $analysis=@('--example',$example,'--map',$map,'--high',($prefix+'_high.bin'),
                    '--probe',($prefix+'.json'),'--output',($prefix+'_analysis.json'))
        if($case -eq 'stress'){$analysis+=@('--low',($prefix+'_low.bin'))}
        if($case -eq 'collision'){$analysis+='--body-collision'}
        & python harness/tests/character_plan.py @analysis
        if($LASTEXITCODE){throw "Character validation failed: $example"}
        if($case -eq 'viewer') {
            & python -c "from PIL import Image; from pathlib import Path; a=Image.open(Path('$prefix'+'_paused_a.png')).crop((130,60,175,185)); b=Image.open(Path('$prefix'+'_paused_b.png')).crop((130,60,175,185)); assert a.tobytes()==b.tobytes(), 'Paused scene changed'"
            if($LASTEXITCODE){throw 'Paused scene pixel comparison failed'}
        }
    }
} finally { Pop-Location }
