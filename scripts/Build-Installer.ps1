# SPDX-License-Identifier: AGPL-3.0-only
param([string]$WeaselTree, [string]$BoostRoot, [string]$SourceTree)
. "$PSScriptRoot/Common.ps1"
if (!$WeaselTree) { $WeaselTree = Join-Path $TypePickRoot 'build/weasel' }
if (!$BoostRoot) { $BoostRoot = Join-Path $TypePickRoot 'build/deps/boost/boost-1.84.0' }
if (!$SourceTree) { $SourceTree = Join-Path $TypePickRoot 'build/release-sources' }
$stage = Join-Path $TypePickRoot 'build/package'
$dist = Join-Path $TypePickRoot 'dist'
New-Item -ItemType Directory -Force -Path $stage, $dist, "$stage/licenses" | Out-Null
foreach ($pair in @(@('WeaselServer.exe','TypePickServer.exe'), @('WeaselDeployer.exe','TypePickDeployer.exe'), @('weaselx64.dll','TypePickTSF.dll'), @('rime.dll','rime.dll'))) {
    Copy-Item -LiteralPath (Join-Path $WeaselTree ('output/' + $pair[0])) -Destination (Join-Path $stage $pair[1]) -Force
}
Copy-Item -LiteralPath "$TypePickRoot/build/core-native/TypePick.exe" -Destination $stage -Force
Copy-Item -LiteralPath "$TypePickRoot/packaging/data" -Destination $stage -Recurse -Force
Copy-Item -LiteralPath "$TypePickRoot/packaging/start_service.bat", "$TypePickRoot/packaging/使用说明.txt" -Destination $stage -Force
Copy-Item -LiteralPath "$TypePickRoot/LICENSE" -Destination "$stage/LICENSE.txt" -Force
Copy-Item -LiteralPath "$TypePickRoot/THIRD_PARTY_NOTICES.md" -Destination "$stage/licenses/THIRD_PARTY_NOTICES.md" -Force
Copy-Item -LiteralPath "$TypePickRoot/upstream/weasel/LICENSE.txt" -Destination "$stage/licenses/Weasel-GPL-3.0.txt" -Force
Copy-Item -LiteralPath "$BoostRoot/LICENSE_1_0.txt" -Destination "$stage/licenses/Boost-1.0.txt" -Force
Copy-Item -LiteralPath "$TypePickRoot/third_party/nlohmann/LICENSE.MIT" -Destination "$stage/licenses/nlohmann-MIT.txt" -Force
Copy-Item -LiteralPath "$TypePickRoot/third_party/pinyin-simp/LICENSE" -Destination "$stage/licenses/pinyin-simp-Apache-2.0.txt" -Force
Copy-Item -LiteralPath "$TypePickRoot/third_party/pinyin-simp/AUTHORS" -Destination "$stage/licenses/pinyin-simp-AUTHORS.txt" -Force
Copy-Item -LiteralPath "$TypePickRoot/third_party/wtl/MS-PL.txt" -Destination "$stage/licenses/WTL-MS-PL.txt" -Force
& python "$PSScriptRoot/package_sources.py" --stage $stage --sources $SourceTree --boost $BoostRoot
Assert-NativeExit 'Package corresponding source and dependency notices'
$uninstall = Join-Path $TypePickRoot 'build/uninstall-files.nsh'
$lines = @()
foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($stage, $file.FullName)
    $lines += 'Delete /REBOOTOK "$INSTDIR\' + $relative + '"'
}
foreach ($dir in Get-ChildItem -LiteralPath $stage -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending) {
    $lines += 'RMDir "$INSTDIR\' + [IO.Path]::GetRelativePath($stage, $dir.FullName) + '"'
}
$lines | Set-Content -LiteralPath $uninstall -Encoding utf8
$nsis = Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe'
if (!(Test-Path -LiteralPath $nsis)) { $nsis = (Get-Command makensis.exe -ErrorAction Stop).Source }
$exe = Join-Path $dist 'TypePick-Setup-0.1.0-x64.exe'
& $nsis /V3 "/DSTAGE=$stage" "/DOUTPUT=$exe" "/DUNINSTALL_FILES=$uninstall" "$TypePickRoot/packaging/typepick.nsi"
Assert-NativeExit 'Compile NSIS installer'
Copy-Item -LiteralPath "$TypePickRoot/packaging/使用说明.txt" -Destination $dist -Force
Get-ChildItem -LiteralPath $dist -File | Where-Object Name -ne 'SHA256SUMS.txt' | ForEach-Object {
    (Get-FileHash -LiteralPath $_.FullName).Hash.ToLower() + '  ' + $_.Name
} | Set-Content -LiteralPath "$dist/SHA256SUMS.txt" -Encoding utf8
Write-Host "Installer ready: $exe"
