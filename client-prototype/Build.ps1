$ErrorActionPreference = 'Stop'
& python (Join-Path $PSScriptRoot 'tools\generate_signatures.py')
if ($LASTEXITCODE -ne 0) { throw 'Signature generation failed' }
& cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build') -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& cmake --build (Join-Path $PSScriptRoot 'build')
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& ctest --test-dir (Join-Path $PSScriptRoot 'build') --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
