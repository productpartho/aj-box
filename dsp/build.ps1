# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Builds the AJ Box desktop engine against FM1 Open's own DSP sources.
#   ./dsp/build.ps1            build renderer and tests
#   ./dsp/build.ps1 -Assets    also regenerate assets/samples/*.c from the placeholder generator
#   ./dsp/build.ps1 -Test      build, then run the tests
#   ./dsp/build.ps1 -Render    build, run tests, render every page's pairs, mood tours, control demos
#                              and extras into renders/
# Needs clang for Windows (LLVM-MinGW). Set AJB_CC to override the compiler path.
[CmdletBinding()]
param([switch]$Assets, [switch]$Test, [switch]$Render)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fw = Join-Path $root 'firmware'
$out = Join-Path $root 'dsp/build'
[IO.Directory]::CreateDirectory($out) | Out-Null

$cc = $env:AJB_CC
if (!$cc) {
    $found = Get-Command clang -ErrorAction SilentlyContinue
    if ($found) { $cc = $found.Source }
    else {
        $pkg = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Microsoft/WinGet/Packages') -Directory -Filter 'MartinStorsjo.LLVM-MinGW*' -ErrorAction SilentlyContinue |
            ForEach-Object { Get-ChildItem $_.FullName -Directory } | Select-Object -First 1
        if ($pkg) { $cc = Join-Path $pkg.FullName 'bin/clang.exe' }
    }
}
if (!$cc -or !(Test-Path -LiteralPath $cc)) { throw 'clang not found: winget install MartinStorsjo.LLVM-MinGW.UCRT, or set AJB_CC.' }
if (!(Test-Path -LiteralPath (Join-Path $fw 'third_party/synth/drum808.c'))) { throw 'firmware/ submodule is missing: git submodule update --init' }

function Run($program, [string[]]$arguments) {
    # Compiler warnings go to stderr; let them through and judge by exit code.
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $program @arguments 2>&1 | ForEach-Object { "$_" }; $result = $LASTEXITCODE } finally { $ErrorActionPreference = $old }
    if ($result -ne 0) { throw "$([IO.Path]::GetFileName($program)) failed ($result)" }
}

# Same language flags as FM1 Open's browser build (tools/Build-Web.ps1).
$flags = @('-std=c11','-O2','-fwrapv','-fno-fast-math','-ffp-contract=off','-DFM1_HOST_TEST','-DFM1_OPEN_WEB',
    '-Wall','-Wextra','-Wno-unused-function','-ffunction-sections','-fdata-sections')
foreach ($dir in @('dsp/include','dsp/src','dsp/host')) { $flags += '-I' + (Join-Path $root $dir) }
foreach ($dir in @('web/synth','third_party/synth','sdk/include','app/synth','app')) { $flags += '-I' + (Join-Path $fw $dir) }
$link = @('-Wl,--gc-sections','-lm')

# FM1 Open engine code, compiled unchanged from the submodule.
$firmware = @('third_party/synth/drum808.c','web/synth/dx7core.c','web/synth/dx7env.c','web/synth/dx7lfo.c',
    'web/synth/dx7math.c','web/synth/dx7note.c','web/synth/dx7pitchenv.c','web/synth/dx7synth.c',
    'web/synth/fixed.c','web/synth/adsr.c','web/synth/mixer.c','web/engine/effects.c','app/fm_factory.c',
    'third_party/synth/pluck.c','web/synth/psg.c','third_party/synth/emu2149.c','third_party/synth/bass303.c',
    'web/synth/sea_decode.c') | ForEach-Object { Join-Path $fw $_ }
$engine = @('dsp/src/engine.c','dsp/src/effects.c','dsp/src/sampler.c','dsp/src/sea5.c','dsp/src/fm1_glue.c') | ForEach-Object { Join-Path $root $_ }
$host_ = @(Join-Path $root 'dsp/host/wav.c')

$samples = Join-Path $root 'assets/samples'
if ($Assets -or !(Test-Path -LiteralPath (Join-Path $samples 'musicbox_placeholder.c'))) {
    [IO.Directory]::CreateDirectory($samples) | Out-Null
    $tools = Join-Path $out 'tools'; [IO.Directory]::CreateDirectory($tools) | Out-Null
    Write-Host 'Build sample tools'
    Run $cc ($flags + @((Join-Path $root 'tools/make_placeholders.c')) + $host_ + @('-o',(Join-Path $tools 'make_placeholders.exe')) + $link)
    Run $cc ($flags + @((Join-Path $root 'tools/sample_encode.c'),(Join-Path $root 'dsp/src/sea5.c')) + $host_ + @('-o',(Join-Path $tools 'sample_encode.exe')) + $link)
    $wavs = Join-Path $out 'placeholders'; [IO.Directory]::CreateDirectory($wavs) | Out-Null
    Run (Join-Path $tools 'make_placeholders.exe') @($wavs)
    # file name, label, root MIDI note, slices (0 = a pitched note)
    $sounds = @(
        @('musicbox','Music Box','72','0'), @('kalimba','Kalimba','72','0'),
        @('la','Her La','72','0'), @('cat','Cat Meow','72','0'),
        @('pots','Pots and Pans','60','16'), @('dhol','Dhol','60','16'),
        @('beatbox','Beatbox','60','16'), @('rainstick','Rain Stick','60','16'))
    foreach ($s in $sounds) {
        Run (Join-Path $tools 'sample_encode.exe') @((Join-Path $wavs "$($s[0])_placeholder.wav"),"ajb_sample_$($s[0])","$($s[1]) (placeholder)",$s[2],$s[3],(Join-Path $samples "$($s[0])_placeholder.c"))
    }
}
$library = @((Join-Path $root 'assets/loops.c')) + @(Get-ChildItem -LiteralPath $samples -Filter '*.c' | Sort-Object Name | ForEach-Object { $_.FullName })

Write-Host 'Build ajbox_render'
Run $cc ($flags + $firmware + $engine + $library + $host_ + @((Join-Path $root 'dsp/render/main.c'),'-o',(Join-Path $out 'ajbox_render.exe')) + $link)
Write-Host 'Build ajbox_tests'
Run $cc ($flags + $firmware + $engine + $library + $host_ + @((Join-Path $root 'dsp/tests/test_main.c'),(Join-Path $root 'dsp/tests/test_s2.c'),(Join-Path $root 'dsp/tests/test_library.c'),'-o',(Join-Path $out 'ajbox_tests.exe')) + $link)

Run (Join-Path $out 'ajbox_render.exe') @('report')
if ($Test -or $Render) { Run (Join-Path $out 'ajbox_tests.exe') @() }
if ($Render) {
    $renders = Join-Path $root 'renders'
    [IO.Directory]::CreateDirectory((Join-Path $renders 'extras')) | Out-Null
    # renders/ holds only generated WAVs; clear them so renamed loops leave no stale pairs.
    Get-ChildItem -LiteralPath $renders -Filter '*.wav' -Recurse | Remove-Item
    Run (Join-Path $out 'ajbox_render.exe') @('pairs',$renders)
    Run (Join-Path $out 'ajbox_render.exe') @('extras',(Join-Path $renders 'extras'))
    [IO.Directory]::CreateDirectory((Join-Path $renders 'moods')) | Out-Null
    Run (Join-Path $out 'ajbox_render.exe') @('moods',(Join-Path $renders 'moods'))
    [IO.Directory]::CreateDirectory((Join-Path $renders 'demos')) | Out-Null
    Run (Join-Path $out 'ajbox_render.exe') @('demos',(Join-Path $renders 'demos'))
}
