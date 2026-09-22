; SPDX-License-Identifier: AGPL-3.0-only
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
Name "TypePick 0.1.0"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\TypePick"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
VIProductVersion "0.1.0.0"
VIAddVersionKey "ProductName" "TypePick"
VIAddVersionKey "FileDescription" "TypePick 拼音输入法安装程序"
VIAddVersionKey "FileVersion" "0.1.0.0"
VIAddVersionKey "LegalCopyright" "TypePick contributors; upstream notices included"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\LICENSE.txt"
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "TypePick 安装完成。请从桌面打开 TypePick 设置，点击保存并启动，再按 Win + 空格切换输入法。如果列表尚未更新，请注销后重新登录。"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "此安装包只支持 Windows x64。"
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
FunctionEnd

Section "TypePick" SEC_MAIN
  ; Fixed install path keeps registration and uninstall scoped to this product.
  IfFileExists "$INSTDIR\TypePickServer.exe" 0 +2
    ExecWait '"$INSTDIR\TypePickServer.exe" /q'
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*"
  WriteRegStr HKLM "Software\TypePick" "WeaselRoot" "$INSTDIR"
  WriteRegStr HKLM "Software\TypePick" "InstallDir" "$INSTDIR"
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\TypePickTSF.dll"' $0
  ${EnableX64FSRedirection}
  ${If} $0 != 0
    ${DisableX64FSRedirection}
    ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$INSTDIR\TypePickTSF.dll"'
    ${EnableX64FSRedirection}
    DeleteRegKey HKLM "Software\TypePick"
    MessageBox MB_ICONSTOP "输入法注册失败（错误 $0）。安装未完成。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "TypePickServer" '$\"$INSTDIR\TypePickServer.exe$\"'
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "DisplayName" "TypePick"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "DisplayVersion" "0.1.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "Publisher" "TypePick contributors"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\TypePick"
  CreateShortcut "$SMPROGRAMS\TypePick\TypePick 设置.lnk" "$INSTDIR\TypePick.exe"
  CreateShortcut "$SMPROGRAMS\TypePick\使用说明.lnk" "$INSTDIR\使用说明.txt"
  CreateShortcut "$DESKTOP\TypePick 设置.lnk" "$INSTDIR\TypePick.exe"
SectionEnd

Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  ExecWait '"$INSTDIR\TypePickServer.exe" /q'
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$INSTDIR\TypePickTSF.dll"' $0
  ${EnableX64FSRedirection}
  ${If} $0 != 0
    MessageBox MB_ICONSTOP "输入法注销失败（错误 $0），请重试。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "TypePickServer"
  DeleteRegKey HKLM "Software\TypePick"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TypePick"
  Delete "$DESKTOP\TypePick 设置.lnk"
  Delete "$SMPROGRAMS\TypePick\TypePick 设置.lnk"
  Delete "$SMPROGRAMS\TypePick\使用说明.lnk"
  RMDir "$SMPROGRAMS\TypePick"
  ; Delete only files recorded in this build; personal user data is preserved.
  !include "${UNINSTALL_FILES}"
  Delete /REBOOTOK "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
