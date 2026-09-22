# SPDX-License-Identifier: AGPL-3.0-only
# x64 static Boost dependencies matching the current MSVC environment.
param([string]$Destination)
. "$PSScriptRoot/Common.ps1"
$null = Enter-TypePickToolchain
if (!$Destination) { $Destination = Join-Path $TypePickRoot 'build/deps/boost' }
$Destination = [IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$archive = Join-Path $Destination 'boost-1.84.0.7z'
$hash = 'ce132f85fc706aa8b5c7e6e52a8338de33a688e92e7c8fada3713194b109232e'
$ProgressPreference = 'SilentlyContinue'
if (!(Test-Path -LiteralPath $archive)) {
    Write-Host 'Downloading Boost 1.84.0 from the official GitHub release...'
    Invoke-WebRequest 'https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.7z' -OutFile "$archive.part" -TimeoutSec 180
    if ((Get-FileHash -LiteralPath "$archive.part").Hash -ne $hash) { throw 'Boost checksum mismatch.' }
    Move-Item -LiteralPath "$archive.part" -Destination $archive
}
if ((Get-FileHash -LiteralPath $archive).Hash -ne $hash) { throw 'Boost checksum mismatch.' }
$boost = Join-Path $Destination 'boost-1.84.0'
if (!(Test-Path -LiteralPath (Join-Path $boost 'bootstrap.bat'))) {
    Write-Host 'Extracting verified Boost archive...'
    $sevenZip = Join-Path $TypePickRoot 'upstream/weasel/output/7z.exe'
    if (!(Test-Path -LiteralPath $sevenZip)) { throw 'Initialize the Weasel submodule first.' }
    & $sevenZip x -y "-o$Destination" $archive | Out-Null
    Assert-NativeExit 'Extract Boost'
}
Push-Location $boost
try {
    Write-Host 'Bootstrapping Boost.Build...'
    & ./bootstrap.bat
    Assert-NativeExit 'Bootstrap Boost'
    # GitHub publishes the modular source tree; create the aggregate boost/ headers.
    & ./b2.exe headers
    Assert-NativeExit 'Prepare Boost headers'
    Write-Host 'Compiling x64 static Boost libraries...'
    & ./b2.exe -j4 --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread toolset=msvc link=static runtime-link=static variant=release architecture=x86 address-model=64 define=BOOST_USE_WINAPI_VERSION=0x0603 stage
    Assert-NativeExit 'Build Boost'
} finally { Pop-Location }
Write-Host "Boost built: $boost"
