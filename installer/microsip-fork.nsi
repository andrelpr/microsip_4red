; [fork] Instalador do MicroSIP personalizado
;
; Compilar (feito pelo GitHub Actions):
;   makensis /DVERSION=3.22.16-fork.1 /DSTAGE=..\stage [/DPROV_URL=https://...] microsip-fork.nsi
;
; Uso:
;   MicroSIP-Setup.exe                         instalacao normal (com telas)
;   MicroSIP-Setup.exe /S                      instalacao silenciosa (implantacao em massa / GPO)
;   MicroSIP-Setup.exe /S /URL=https://prov.exemplo.com.br/microsip/v1/acme
;                                              grava a URL de provisionamento para todos os usuarios
;
; STAGE deve conter: MicroSIP.exe, langpack_portuguesebr.txt, *.wav, lame_enc.dll, License.txt

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

!ifndef VERSION
  !define VERSION "dev"
!endif
!ifndef STAGE
  !define STAGE "..\stage"
!endif

!define APPNAME "MicroSIP"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

Name "${APPNAME}"
OutFile "MicroSIP-Setup-${VERSION}.exe"
InstallDir "$PROGRAMFILES\${APPNAME}"
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "${APPNAME} ${VERSION}"

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "FileDescription" "Instalador ${APPNAME}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "GPL v2 - baseado no MicroSIP (www.microsip.org)"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE "${STAGE}\License.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\MicroSIP.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "PortugueseBR"

Var ProvUrl

Function .onInit
  ; URL de provisionamento: parametro /URL= tem prioridade sobre a embutida na compilacao
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/URL=" $ProvUrl
  ${If} ${Errors}
    StrCpy $ProvUrl ""
!ifdef PROV_URL
    StrCpy $ProvUrl "${PROV_URL}"
!endif
  ${EndIf}
FunctionEnd

!macro CloseMicroSIP
  ; fecha o MicroSIP em execucao (primeiro de forma educada, depois forcado)
  nsExec::Exec 'taskkill /IM MicroSIP.exe'
  Sleep 1500
  nsExec::Exec 'taskkill /F /IM MicroSIP.exe'
  Sleep 500
!macroend

Section "MicroSIP" SecMain
  SectionIn RO
  !insertmacro CloseMicroSIP
  SetShellVarContext all
  SetOutPath "$INSTDIR"

  ; remove idiomas de instalacoes anteriores: o MicroSIP usa o primeiro langpack_*.txt que encontrar
  Delete "$INSTDIR\langpack_*.txt"

  File "${STAGE}\MicroSIP.exe"
  File "${STAGE}\langpack_portuguesebr.txt"
  File "${STAGE}\License.txt"
  File /nonfatal "${STAGE}\*.wav"
  File /nonfatal "${STAGE}\lame_enc.dll"

  ${If} $ProvUrl != ""
    WriteINIStr "$INSTDIR\provisioning.ini" "Provisioning" "url" "$ProvUrl"
  ${EndIf}

  ; atalhos
  CreateShortCut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\MicroSIP.exe"
  CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\MicroSIP.exe"

  ; iniciar com o Windows: o proprio MicroSIP le esta chave e cria a entrada do usuario
  WriteRegDWORD HKLM "Software\${APPNAME}" "RunAtSystemStartup" 1
  WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"

  ; desinstalador
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\MicroSIP.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Uninstall"
  !insertmacro CloseMicroSIP
  SetShellVarContext all
  Delete "$INSTDIR\MicroSIP.exe"
  Delete "$INSTDIR\langpack_*.txt"
  Delete "$INSTDIR\License.txt"
  Delete "$INSTDIR\*.wav"
  Delete "$INSTDIR\lame_enc.dll"
  Delete "$INSTDIR\provisioning.ini"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${APPNAME}.lnk"
  Delete "$DESKTOP\${APPNAME}.lnk"
  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "Software\${APPNAME}"
  ; as configuracoes do usuario (%APPDATA%\MicroSIP) sao mantidas
SectionEnd
