# SPDX-License-Identifier: AGPL-3.0-only
param([string]$Destination)
. "$PSScriptRoot/Common.ps1"
if (!$Destination) { $Destination = Join-Path $TypePickRoot 'build/weasel' }
$Destination = [IO.Path]::GetFullPath($Destination)
$upstream = Join-Path $TypePickRoot 'upstream/weasel'
$pin = 'd73f6295e8252ed2f7b9c12bae32e9001b1afdaa'
if (!(Test-Path -LiteralPath (Join-Path $upstream 'weasel.sln'))) {
    & git -C $TypePickRoot submodule update --init upstream/weasel
    Assert-NativeExit 'Initialize Weasel submodule'
}
$revision = & git -C $upstream rev-parse HEAD
Assert-NativeExit 'Read upstream revision'
if ($revision -ne $pin) { throw "Expected Weasel $pin; refusing to apply patch to a different revision." }
$patch = Join-Path $TypePickRoot 'patches/weasel-typepick.patch'
$hash = (Get-FileHash -LiteralPath $patch).Hash
$marker = Join-Path $Destination '.typepick-prepared.json'
if (Test-Path -LiteralPath $marker) {
    $prepared = Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
    if ($prepared.upstream -ne $pin -or $prepared.patch -ne $hash) {
        throw 'Prepared tree uses a different patch. Select a new -Destination to preserve existing work.'
    }
} else {
    if ((Test-Path -LiteralPath $Destination) -and @(Get-ChildItem -LiteralPath $Destination -Force).Count) {
        throw 'Destination is not empty. Select a new build directory.'
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $archive = Join-Path $Destination 'upstream.tar'
    & git -C $upstream archive --format=tar "--output=$archive" $pin
    Assert-NativeExit 'Archive pinned Weasel'
    & tar -xf $archive -C $Destination
    Assert-NativeExit 'Extract Weasel'
    & git -C $Destination init -q
    Assert-NativeExit 'Initialize scratch repository'
    & git -C $Destination apply --check $patch
    Assert-NativeExit 'Check integration patch'
    & git -C $Destination apply $patch
    Assert-NativeExit 'Apply integration patch'
    @{ upstream = $pin; patch = $hash } | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding utf8
}
$overlay = Join-Path $Destination 'typepick'
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
foreach ($name in @('native', 'third_party', 'config')) {
    Copy-Item -LiteralPath (Join-Path $TypePickRoot $name) -Destination $overlay -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $TypePickRoot 'LICENSE') -Destination (Join-Path $overlay 'LICENSE') -Force
Write-Output $Destination
