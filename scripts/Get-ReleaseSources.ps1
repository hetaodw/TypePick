# SPDX-License-Identifier: AGPL-3.0-only
param([string]$Destination)
. "$PSScriptRoot/Common.ps1"
if (!$Destination) { $Destination = Join-Path $TypePickRoot 'build/release-sources' }
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$projects = @(
    @{ Name='librime'; Repo='rime/librime'; Ref='33e78140250125871856cdc5b42ddc6a5fcd3cd4' },
    @{ Name='librime/plugins/lua'; Repo='hchunhui/librime-lua'; Ref='ec52e48ea18f11af37717a01c337f853215cf70b' },
    @{ Name='librime/plugins/lua/thirdparty'; Repo='hchunhui/librime-lua'; Ref='fa40fadd8af1e5b1fbd55703ccbd54476956d74c' },
    @{ Name='librime/plugins/octagram'; Repo='lotem/librime-octagram'; Ref='dfcc15115788c828d9dd7b4bff68067d3ce2ffb8' },
    @{ Name='librime/plugins/predict'; Repo='rime/librime-predict'; Ref='920bd41ebf6f9bf6855d14fbe80212e54e749791' }
)
foreach ($item in $projects) {
    $path = Join-Path $Destination $item.Name
    if (!(Test-Path -LiteralPath (Join-Path $path '.git'))) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
        & git -C $path init -q
        Assert-NativeExit 'Initialize dependency source'
        & git -C $path remote add origin "https://github.com/$($item.Repo).git"
        Assert-NativeExit 'Set source remote'
        & git -C $path fetch --depth 1 origin $item.Ref
        Assert-NativeExit 'Fetch exact dependency source'
        & git -C $path checkout --detach FETCH_HEAD
        Assert-NativeExit 'Checkout exact dependency source'
    }
    $head = & git -C $path rev-parse HEAD
    if ($head -ne $item.Ref) { throw "Unexpected source revision: $($item.Name)" }
    & git -C $path submodule update --init --recursive --depth 1
    Assert-NativeExit 'Fetch dependency submodules'
}
# librime's official 1.17.0 Windows build uses Boost 1.89.0 (see its workflow).
$boost = Join-Path $Destination 'boost-1.89.0-b2-nodocs.7z'
if (!(Test-Path -LiteralPath $boost)) {
    Invoke-WebRequest 'https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-b2-nodocs.7z' -OutFile "$boost.part" -TimeoutSec 180
    if ((Get-FileHash -LiteralPath "$boost.part").Hash -ne 'f6e400af4fec3df84b249134690b905d8d2b7f966c3cee8545e4d51b92adb4a7') { throw 'Boost 1.89 source checksum mismatch.' }
    Move-Item -LiteralPath "$boost.part" -Destination $boost
}
if ((Get-FileHash -LiteralPath $boost).Hash -ne 'f6e400af4fec3df84b249134690b905d8d2b7f966c3cee8545e4d51b92adb4a7') { throw 'Boost 1.89 source checksum mismatch.' }
$projects | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Destination 'source-pins.json') -Encoding utf8
Get-FileHash -LiteralPath $boost | Select-Object Hash, Path | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Destination 'boost-source-sha256.json') -Encoding utf8
Write-Output $Destination
