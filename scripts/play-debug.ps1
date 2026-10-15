param(
  [string]$Rom = 'R:/ChatGPT/Go Go Ackman (Japan).sfc',
  [string]$PcmPrefix = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'build-ninja/AckmanRecomp.exe'
if (!(Test-Path -LiteralPath $Rom)) { throw "ROM not found: $Rom" }
if (!(Test-Path -LiteralPath $exe)) { throw 'Run scripts/build-local.ps1 first.' }
$logs = Join-Path $root ('private/gameplay-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$env:ACKMAN_WIDE_EXPERIMENTAL = '1'
$env:ACKMAN_WIDE_FORCE = '1'
$env:ACKMAN_DEBUG_KEYS = '1'
$env:ACKMAN_VIDEO_TRACE = Join-Path $logs 'video.csv'
$env:ACKMAN_TIMING_TRACE = Join-Path $logs 'timing.csv'
$env:ACKMAN_AUDIO_TRACE = Join-Path $logs 'audio.csv'
$env:ACKMAN_TRACE = Join-Path $logs 'frames.csv'
$env:SNESRECOMP_HOST_PROFILE = '1'
$env:SNESRECOMP_AUDIO_STATS = Join-Path $logs 'audio-buffer.txt'
$env:ACKMAN_HOST_STALL_TRACE = '1'
Remove-Item Env:SNESRECOMP_RUN_FRAMES -ErrorAction SilentlyContinue
if ($PcmPrefix) {
  $env:ACKMAN_MSU_EXPERIMENTAL = '1'
  $env:ACKMAN_MSU_PREFIX = $PcmPrefix
} else {
  $env:ACKMAN_MSU_EXPERIMENTAL = '0'
}
Copy-Item -LiteralPath (Join-Path $root 'config-debug.ini') -Destination (Join-Path $root 'build-ninja/config.ini')
Write-Host "Logs: $logs"
Write-Host "F5: next available PCM; F6: PCM fade. Native SPC is unchanged by debug playback."
Write-Host "Monitor from another terminal: Get-Content -Wait '$logs/console.log'"
$game = Start-Process -FilePath $exe -WorkingDirectory (Join-Path $root 'build-ninja') `
  -ArgumentList @('--no-launcher', ('"' + (Resolve-Path -LiteralPath $Rom).Path + '"')) `
  -RedirectStandardError (Join-Path $logs 'console.log') `
  -RedirectStandardOutput (Join-Path $logs 'stdout.log') -PassThru -Wait
Write-Host "Exit: $($game.ExitCode). Logs: $logs"
