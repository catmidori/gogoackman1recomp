param([string]$Rom)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if (!(Test-Path external/snesrecomp/snesrecomp_cli.py)) {
    git clone https://github.com/RetroPortingToolKit/snesrecomp.git external/snesrecomp
    if ($LASTEXITCODE) { throw 'clone failed' }
}
$revision = (Get-Content framework.lock).Trim()
git -C external/snesrecomp checkout --detach $revision
if ($LASTEXITCODE) { throw 'checkout failed' }
if (!(Test-Path rom_identity.txt)) {
    if (!$Rom) { throw 'First setup requires -Rom path' }
    python external/snesrecomp/tools/new_project/probe_rom.py $Rom --json-out private/probe.json --write-seed-cfg recomp/bank00.cfg --write-symbols recomp/symbols.toml
    if ($LASTEXITCODE) { throw 'probe failed' }
    python scripts/prepare.py
    if ($LASTEXITCODE) { throw 'prepare failed' }
}
if ($Rom) {
    python scripts/regen.py --rom $Rom
    if ($LASTEXITCODE) { throw 'generation failed' }
}
