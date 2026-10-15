param(
  [string]$CMake = '',
  [string]$Ninja = '',
  [string]$Toolchain = 'C:/msys64/mingw64/bin',
  [string]$Python = 'python',
  [string]$IconPython = '',
  [int]$Jobs = 6
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
if (!$CMake) {
  $local = Join-Path $root '.tools/cmake-3.31.6-windows-x86_64/bin/cmake.exe'
  $CMake = if (Test-Path -LiteralPath $local) { $local } else { (Get-Command cmake -ErrorAction Stop).Source }
}
if (!$Ninja) {
  $local = Join-Path $root '.tools/ninja/ninja.exe'
  $Ninja = if (Test-Path -LiteralPath $local) { $local } else { (Get-Command ninja -ErrorAction Stop).Source }
}
if (!$IconPython) { $IconPython = $Python }
$build = Join-Path $root 'build-release-windows'
$env:PATH = "$Toolchain;$env:PATH"
$options = @('-S', $root, '-B', $build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
  '-DBUILD_TESTING=ON', "-DCMAKE_MAKE_PROGRAM=$Ninja",
  "-DCMAKE_C_COMPILER=$Toolchain/gcc.exe", "-DCMAKE_CXX_COMPILER=$Toolchain/g++.exe",
  "-DPython3_EXECUTABLE=$Python", "-DACKMAN_ICON_PYTHON=$IconPython", '-DZLIB_USE_STATIC_LIBS=ON',
  "-DZLIB_ROOT=$(Split-Path $Toolchain -Parent)")
$sdl = Join-Path $root '.tools/SDL3-3.4.10'
if (Test-Path -LiteralPath $sdl) { $options += "-DSNESRECOMP_SDL3_SOURCE_DIR=$($sdl.Replace('\','/'))" }
& $CMake @options
if ($LASTEXITCODE) { throw 'Release configuration failed' }
& $CMake --build $build --parallel $Jobs
if ($LASTEXITCODE) { throw 'Release compilation failed' }
$ctest = Join-Path (Split-Path $CMake -Parent) 'ctest.exe'
& $ctest --test-dir $build --output-on-failure
if ($LASTEXITCODE) { throw 'Release tests failed; package not created' }
& $Python "$PSScriptRoot/package_windows_release.py" --build $build --runtime-dir $Toolchain `
  --objdump "$Toolchain/objdump.exe" --output "$root/releases/Go_Go_Ackman_Recompiled_Windows_x64.zip"
if ($LASTEXITCODE) { throw 'Release package validation failed' }
& $Python "$PSScriptRoot/write_release_checksums.py"
if ($LASTEXITCODE) { throw 'Release checksum generation failed' }
