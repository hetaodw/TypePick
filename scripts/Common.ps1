# SPDX-License-Identifier: AGPL-3.0-only
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$TypePickRoot = Split-Path $PSScriptRoot -Parent
function Assert-NativeExit([string]$Action) {
    if ($LASTEXITCODE -ne 0) { throw "$Action failed (exit $LASTEXITCODE)" }
}
function Enter-TypePickToolchain {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ Build Tools and Windows SDK.' }
    $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vsPath) { throw 'Visual Studio C++ toolchain was not found.' }
    $launch = Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1'
    $launchArgs = @{ Arch = 'amd64'; HostArch = 'amd64'; SkipAutomaticLocation = $true }
    # NoLogo was added after VS2022; keep the same script usable on both versions.
    if ((Get-Command $launch).Parameters.ContainsKey('NoLogo')) { $launchArgs.NoLogo = $true }
    & $launch @launchArgs | Out-Null
    return $vsPath
}
