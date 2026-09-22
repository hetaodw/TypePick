# SPDX-License-Identifier: AGPL-3.0-only
param([Parameter(Mandatory)][string]$BoostRoot, [string]$PlatformToolset = 'v143', [string]$Destination, [string]$RimeDistribution)
. "$PSScriptRoot/Common.ps1"
$vsPath = Enter-TypePickToolchain
if (!(Test-Path -LiteralPath (Join-Path $env:VCToolsInstallDir 'atlmfc/include/atlbase.h'))) {
    throw 'Full Weasel build requires the Visual Studio C++ ATL component. Core/probe builds do not require ATL.'
}
$BoostRoot = (Resolve-Path -LiteralPath $BoostRoot).Path
# GitHub cache archives do not retain all Boost header directory junctions.
# Recreate the aggregate include tree from the cached source modules every run.
Push-Location $BoostRoot
try {
    & ./b2.exe -a headers
    Assert-NativeExit 'Restore Boost header junctions'
} finally { Pop-Location }
if (!(Test-Path -LiteralPath (Join-Path $BoostRoot 'boost/version.hpp')) -or !(Test-Path -LiteralPath (Join-Path $BoostRoot 'stage/lib'))) {
    throw 'Boost headers and prebuilt x64 static libraries are required. See docs/development.md.'
}
$tree = & "$PSScriptRoot/Prepare-Weasel.ps1" -Destination $Destination
if (!$RimeDistribution) { $RimeDistribution = & "$PSScriptRoot/Get-Rime.ps1" }
$RimeDistribution = (Resolve-Path -LiteralPath $RimeDistribution).Path
New-Item -ItemType Directory -Force -Path (Join-Path $tree 'librime/include'), (Join-Path $tree 'librime/build/lib/Release') | Out-Null
Copy-Item -Path (Join-Path $RimeDistribution 'include/*') -Destination (Join-Path $tree 'librime/include') -Force
Copy-Item -LiteralPath (Join-Path $RimeDistribution 'lib/rime.lib') -Destination (Join-Path $tree 'librime/build/lib/Release/rime.lib') -Force
Copy-Item -LiteralPath (Join-Path $RimeDistribution 'lib/rime.dll') -Destination (Join-Path $tree 'output/rime.dll') -Force
$props = Get-Content -LiteralPath (Join-Path $tree 'weasel.props.template') -Raw
$replacements = @{ BOOST_ROOT = [Security.SecurityElement]::Escape($BoostRoot); PLATFORM_TOOLSET = $PlatformToolset; VERSION_MAJOR = '0'; VERSION_MINOR = '17'; VERSION_PATCH = '4'; PRODUCT_VERSION = '0.17.4.0'; FILE_VERSION = '0.17.4.0' }
foreach ($key in $replacements.Keys) { $props = $props.Replace('$' + $key, $replacements[$key]) }
Set-Content -LiteralPath (Join-Path $tree 'weasel.props') -Value $props -Encoding utf8
$msbuild = Join-Path $vsPath 'MSBuild/Current/Bin/MSBuild.exe'
& $msbuild (Join-Path $tree 'weasel.sln') /m /nologo '/t:WeaselServer;WeaselTSF;WeaselDeployer' '/p:Configuration=Release' '/p:Platform=x64' "/p:PlatformToolset=$PlatformToolset" /v:minimal
Assert-NativeExit 'Patched Weasel x64 build'
Write-Host "Built x64 development binaries in $tree/output. No input method was installed or replaced."
