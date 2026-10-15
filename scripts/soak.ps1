param(
  [Parameter(Mandatory=$true)][string]$Exe,
  [Parameter(Mandatory=$true)][string]$Rom,
  [int]$Seconds = 600,
  [string]$Output = 'private/soak.csv'
)
$ErrorActionPreference = 'Stop'
$exePath = (Resolve-Path -LiteralPath $Exe).Path
$romPath = (Resolve-Path -LiteralPath $Rom).Path
if ($romPath.Contains('"')) { throw 'Unsupported quote in path' }
$outPath = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outPath)) | Out-Null
'seconds,private_bytes,working_set,handles,threads' | Set-Content $outPath
$env:SNESRECOMP_RUN_FRAMES = [string][Math]::Ceiling($Seconds * 60.0988)
$proc = Start-Process -FilePath $exePath -ArgumentList @('--no-launcher',('"' + $romPath + '"')) -WorkingDirectory (Split-Path $exePath) -WindowStyle Hidden -PassThru
$watch = [Diagnostics.Stopwatch]::StartNew()
while (!$proc.HasExited -and $watch.Elapsed.TotalSeconds -lt ($Seconds + 60)) {
  $proc.Refresh()
  '{0:F2},{1},{2},{3},{4}' -f $watch.Elapsed.TotalSeconds,$proc.PrivateMemorySize64,$proc.WorkingSet64,$proc.HandleCount,$proc.Threads.Count | Add-Content $outPath
  Start-Sleep -Seconds 1
}
if (!$proc.HasExited) {
  Stop-Process -Id $proc.Id
  throw 'Timed out: process stopped; inspect trace for a guest hang'
}
$proc.WaitForExit()
if ($proc.ExitCode -ne 0) { throw "Process failed: $($proc.ExitCode)" }
Write-Output "Completed: $outPath. Inspect growth after warm-up; exit 0 alone is not a fidelity gate."
