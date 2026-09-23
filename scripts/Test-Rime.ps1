# SPDX-License-Identifier: AGPL-3.0-only
param([string]$BuildDirectory, [string]$RimeDll)
. "$PSScriptRoot/Common.ps1"
if (!$BuildDirectory) { $BuildDirectory = Join-Path $TypePickRoot 'build/core-native' }
if (!$RimeDll) { $RimeDll = Join-Path (& "$PSScriptRoot/Get-Rime.ps1") 'lib/rime.dll' }
$probe = Join-Path $BuildDirectory 'TypePickProbe.exe'
if (!(Test-Path -LiteralPath $probe)) { throw 'Run Build-Core.ps1 first.' }
$scratch = Join-Path $TypePickRoot ('build/probes/' + [guid]::NewGuid().ToString('N'))
$cases = @(
    @{ Name = 'research'; Context = '这个问题需要进一步'; Expected = '研究' },
    @{ Name = 'shop'; Context = '这家商店主要卖'; Expected = '烟酒' },
    @{ Name = 'edit-preedit'; Context = '这家商店主要卖'; Expected = '烟酒'; Edit = $true },
    @{ Name = 'focus'; Context = '这个问题需要进一步'; Reject = 'focus' },
    @{ Name = 'input'; Context = '这个问题需要进一步'; Reject = 'input' },
    @{ Name = 'app'; Context = '这个问题需要进一步'; Reject = 'app' },
    @{ Name = 'local-features'; Context = ''; Local = $true }
)
$results = @()
foreach ($case in $cases) {
    $probeArgs = @('--rime', $RimeDll, '--data', (Join-Path $TypePickRoot 'data'), '--user', (Join-Path $scratch $case.Name), '--bridge-smoke', '--context', $case.Context)
    if ($case.ContainsKey('Reject')) { $probeArgs += @('--reject', $case.Reject) }
    if ($case.ContainsKey('Edit')) { $probeArgs += '--edit-preedit' }
    if ($case.ContainsKey('Local')) { $probeArgs = @($probeArgs | Where-Object { $_ -ne '--bridge-smoke' }); $probeArgs += '--local-smoke' }
    $raw = & $probe @probeArgs
    Assert-NativeExit "Rime probe $($case.Name)"
    $result = ($raw -join "`n") | ConvertFrom-Json
    if ($case.ContainsKey('Expected') -and $result.commit -ne $case.Expected) { throw 'Unexpected Rime commit.' }
    if ($case.ContainsKey('Reject') -and $result.rejected -ne $case.Reject) { throw 'Stale result was not rejected.' }
    $results += $result
    Write-Host "PASS: $($case.Name)"
}
$results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $scratch 'results.json') -Encoding utf8
Write-Host "$($cases.Count) real-librime bridge tests passed. Evidence: $scratch/results.json"
