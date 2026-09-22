# SPDX-License-Identifier: AGPL-3.0-only
param([string]$BuildDirectory, [string]$CMakePath)
. "$PSScriptRoot/Common.ps1"
$vsPath = Enter-TypePickToolchain
if (!$BuildDirectory) { $BuildDirectory = Join-Path $TypePickRoot 'build/core-native' }
if (!$CMakePath) { $CMakePath = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
$ninja = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
if (!(Test-Path -LiteralPath $CMakePath) -or !(Test-Path -LiteralPath $ninja)) { throw 'Install the Visual Studio C++ CMake tools component.' }
& $CMakePath -S $TypePickRoot -B $BuildDirectory -G Ninja '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_CXX_COMPILER=cl' "-DCMAKE_MAKE_PROGRAM=$ninja"
Assert-NativeExit 'CMake configure'
& $CMakePath --build $BuildDirectory --parallel
Assert-NativeExit 'CMake build'
& (Join-Path (Split-Path $CMakePath) 'ctest.exe') --test-dir $BuildDirectory --output-on-failure
Assert-NativeExit 'Core tests'
Write-Host "Built and tested: $BuildDirectory"
