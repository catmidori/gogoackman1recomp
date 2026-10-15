$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$root = (Get-Location).Path.Replace('\','/')
$cmake = "$root/.tools/cmake-3.31.6-windows-x86_64/bin/cmake.exe"
if (!(Test-Path $cmake)) { throw 'Local tools absent; follow README MSVC/MinGW instructions.' }
& $cmake -S . -B build-ninja -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$root/.tools/ninja/ninja.exe" `
  -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe `
  -DPython3_EXECUTABLE=C:/msys64/mingw64/bin/python.exe `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  "-DSNESRECOMP_SDL3_SOURCE_DIR=$root/.tools/SDL3-3.4.10"
if ($LASTEXITCODE) { throw 'configure failed' }
& $cmake --build build-ninja --parallel 6
if ($LASTEXITCODE) { throw 'build failed' }
& "$root/.tools/cmake-3.31.6-windows-x86_64/bin/ctest.exe" --test-dir build-ninja --output-on-failure
if ($LASTEXITCODE) { throw 'tests failed' }
Copy-Item config.ini build-ninja/config.ini
