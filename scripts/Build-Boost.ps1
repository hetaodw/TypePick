# SPDX-License-Identifier: AGPL-3.0-only
# x64 static Boost dependencies matching the current MSVC environment.
param([string]$Destination)
. "$PSScriptRoot/Common.ps1"
$null = Enter-TypePickToolchain
if (!$Destination) { $Destination = Join-Path $TypePickRoot 'build/deps/boost' }
$Destination = [IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$archive = Join-Path $Destination 'boost_1_84_0.tar.bz2'
$hash = 'cc4b893acf645c9d4b698e9a0f08ca8846aa5d6c68275c14c3e7949c24109454'
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest 'https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.bz2' -OutFile "$archive.part"
    if ((Get-FileHash -LiteralPath "$archive.part").Hash -ne $hash) { throw 'Boost checksum mismatch.' }
    Move-Item -LiteralPath "$archive.part" -Destination $archive
}
if ((Get-FileHash -LiteralPath $archive).Hash -ne $hash) { throw 'Boost checksum mismatch.' }
$boost = Join-Path $Destination 'boost_1_84_0'
if (!(Test-Path -LiteralPath (Join-Path $boost 'bootstrap.bat'))) {
    & tar -xf $archive -C $Destination
    Assert-NativeExit 'Extract Boost'
}
Push-Location $boost
try {
    & ./bootstrap.bat
    Assert-NativeExit 'Bootstrap Boost'
    & ./b2.exe -j4 --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread toolset=msvc link=static runtime-link=static variant=release architecture=x86 address-model=64 define=BOOST_USE_WINAPI_VERSION=0x0603 stage
    Assert-NativeExit 'Build Boost'
} finally { Pop-Location }
Write-Host "Boost built: $boost"
