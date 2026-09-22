# SPDX-License-Identifier: AGPL-3.0-only
param()
. "$PSScriptRoot/Common.ps1"
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Run installed-system tests only on disposable GitHub CI runners.' }
$install = Join-Path $env:ProgramW6432 'TypePick'
$clsid = 'HKLM:\Software\Classes\CLSID\{603238AF-C1E4-5691-8B93-AE392BD3BCD1}'
$original = 'HKLM:\Software\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}'
$originalPresent = Test-Path $original
$setup = Start-Process -FilePath "$TypePickRoot/dist/TypePick-Setup-0.1.0-x64.exe" -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
if ($setup.ExitCode -ne 0 -or !(Test-Path $clsid)) { throw 'Installer or TSF registration failed.' }
$server = $null
try {
    $user = Join-Path $env:APPDATA 'TypePick'
    New-Item -ItemType Directory -Force -Path $user | Out-Null
    '{"enabled":true,"mode":"demo","debounce_ms":50,"timeout_ms":1500}' | Set-Content -LiteralPath "$user/typepick.json" -Encoding utf8NoBOM
    $selfTest = Start-Process -FilePath "$install/TypePick.exe" -ArgumentList '--self-test' -WindowStyle Hidden -PassThru -Wait
    if ($selfTest.ExitCode -ne 0) { throw 'Credential Manager self-test failed.' }
    $server = Start-Process -FilePath "$install/TypePickServer.exe" -WindowStyle Hidden -PassThru
    $test = Start-Process -FilePath (Get-Command python).Source -ArgumentList "`"$PSScriptRoot/test_installed_ipc.py`"" -WindowStyle Hidden -PassThru
    if (!$test.WaitForExit(120000)) { Stop-Process -Id $test.Id; throw 'Installed IPC test timed out.' }
    if ($test.ExitCode -ne 0) { throw 'Installed IPC test failed.' }
} finally {
    if ($server -and !$server.HasExited) {
        $quit = Start-Process -FilePath "$install/TypePickServer.exe" -ArgumentList '/q' -WindowStyle Hidden -PassThru
        if (!$quit.WaitForExit(10000)) { Stop-Process -Id $quit.Id }
        if (!$server.WaitForExit(10000)) { Stop-Process -Id $server.Id }
    }
    # _?= executes the uninstaller in place so this process can wait for completion.
    $uninstall = Start-Process -FilePath "$install/Uninstall.exe" -ArgumentList "/S _?=$install" -WindowStyle Hidden -PassThru -Wait
    if ($uninstall.ExitCode -ne 0 -or (Test-Path $clsid)) { throw 'Uninstall did not unregister TypePick.' }
    if ((Test-Path $original) -ne $originalPresent) { throw 'Original Weasel registration was changed.' }
}
Write-Host 'PASS: install, independent TSF registration, real server Pinyin, demo Tab, app restriction, uninstall.'
