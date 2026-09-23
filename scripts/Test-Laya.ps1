# SPDX-License-Identifier: AGPL-3.0-only
param([ValidateRange(1,65535)][int]$Port = 18765)
. "$PSScriptRoot/Common.ps1"
$probe = Join-Path $TypePickRoot 'build/core-native/TypePickProbe.exe'
if (!(Test-Path -LiteralPath $probe)) { throw 'Run Build-Core.ps1 first.' }
$rime = Join-Path (& "$PSScriptRoot/Get-Rime.ps1") 'lib/rime.dll'
$scratch = Join-Path $TypePickRoot ('build/probes/laya-' + [guid]::NewGuid().ToString('N'))
$results = @()
foreach ($case in @(
    @{ Context = '这家便利店主要卖'; Expected = '烟酒' },
    @{ Context = '这个问题需要进一步'; Expected = '研究' }
)) {
    $raw = & $probe --rime $rime --data (Join-Path $TypePickRoot 'data') --user $scratch --local --port $Port --show-all-confidences --timeout-ms 1500 --context $case.Context
    Assert-NativeExit 'Local Laya probe'
    $result = ($raw -join "`n") | ConvertFrom-Json
    $results += $result
}
$results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $scratch 'results.json') -Encoding utf8
for ($i = 0; $i -lt $results.Count; $i++) {
    $expected = @('烟酒', '研究')[$i]
    if ($results[$i].status -ne 'recommended' -or $results[$i].commit -ne $expected) {
        throw "Laya did not select the expected word. Evidence: $scratch/results.json"
    }
}
Write-Host "PASS: real local Laya selection and librime commit. Evidence: $scratch/results.json"
