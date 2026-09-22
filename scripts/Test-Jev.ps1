# SPDX-License-Identifier: AGPL-3.0-only
# Opt-in live requests with synthetic text only. Never evaluate dotenv as code.
param([string]$EnvFile, [ValidateRange(100,3000)][int]$TimeoutMs = 3000, [string]$BuildDirectory, [string]$RimeDll)
. "$PSScriptRoot/Common.ps1"
if (!$EnvFile) { $EnvFile = Join-Path $TypePickRoot '.env' }
if (!$BuildDirectory) { $BuildDirectory = Join-Path $TypePickRoot 'build/core-native' }
if (!$RimeDll) { $RimeDll = Join-Path $TypePickRoot 'build/deps/rime-1.17.0-x64/dist/lib/rime.dll' }
$probe = Join-Path $BuildDirectory 'TypePickProbe.exe'
if (!(Test-Path -LiteralPath $probe) -or !(Test-Path -LiteralPath $RimeDll)) { throw 'Run Build-Core.ps1 and Test-Rime.ps1 first.' }
$rawKey = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $EnvFile).Path).Trim()
if ($rawKey -notmatch '^(?:key|TYPESAFE_API_KEY)\s*=\s*([^\r\n]+)$') {
    throw 'Expected one dotenv line: key="..." or TYPESAFE_API_KEY="...". Value omitted.'
}
$secret = $Matches[1].Trim()
$Matches.Clear()
if ($secret.StartsWith('"') -or $secret.StartsWith("'")) {
    if ($secret.Length -lt 2 -or $secret[$secret.Length-1] -ne $secret[0]) { throw 'Unmatched dotenv quotes. Value omitted.' }
    $secret = $secret.Substring(1, $secret.Length-2)
}
if ([string]::IsNullOrWhiteSpace($secret) -or $secret.Length -ge 4096) { throw 'Empty or oversized API key. Value omitted.' }
$previous = [Environment]::GetEnvironmentVariable('TYPESAFE_API_KEY', 'Process')
$scratch = Join-Path $TypePickRoot ('build/live-probes/' + [guid]::NewGuid().ToString('N'))
$results = @()
try {
    [Environment]::SetEnvironmentVariable('TYPESAFE_API_KEY', $secret, 'Process')
    foreach ($case in @(@{Name='research'; Context='这个问题需要进一步'}, @{Name='shop'; Context='这家商店主要卖'})) {
        $raw = & $probe --rime $RimeDll --data (Join-Path $TypePickRoot 'data') --user (Join-Path $scratch $case.Name) --live --input yanjiu --context $case.Context --timeout-ms $TimeoutMs
        Assert-NativeExit "Live probe $($case.Name)"
        $result = ($raw -join "`n") | ConvertFrom-Json
        $results += $result
        Write-Host "$($case.Name): $($result.status), confidence=$($result.confidence), elapsed=$($result.elapsed_ms)ms"
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $scratch 'results.json') -Encoding utf8
    Write-Host "Live evidence: $scratch/results.json (synthetic text, no credentials)"
    if (@($results | Where-Object { $_.status -notin @('recommended','uncertain','abstained') }).Count) {
        throw 'One or more live requests failed or exceeded the time limit; see status fields.'
    }
} finally {
    [Environment]::SetEnvironmentVariable('TYPESAFE_API_KEY', $previous, 'Process')
    $secret = $null
    $rawKey = $null
}
