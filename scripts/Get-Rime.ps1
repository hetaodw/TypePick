# SPDX-License-Identifier: AGPL-3.0-only
param([string]$Destination)
. "$PSScriptRoot/Common.ps1"
if (!$Destination) { $Destination = Join-Path $TypePickRoot 'build/deps/rime-1.17.0-x64' }
$Destination = [IO.Path]::GetFullPath($Destination)
$expected = '7478c7caa4ff6b37de86daba1f7ce4a994a4f5ba24872a820fb2b3a9b01fed15'
$url = 'https://github.com/rime/librime/releases/download/1.17.0/rime-33e7814-Windows-msvc-x64.7z'
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$archive = Join-Path $Destination 'rime.7z'
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri $url -OutFile "$archive.part"
    if ((Get-FileHash -LiteralPath "$archive.part" -Algorithm SHA256).Hash -ne $expected) { throw 'Rime archive checksum mismatch.' }
    Move-Item -LiteralPath "$archive.part" -Destination $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Cached Rime archive checksum mismatch.' }
& tar -xf $archive -C $Destination
Assert-NativeExit 'Extract librime'
Write-Output (Join-Path $Destination 'dist')
