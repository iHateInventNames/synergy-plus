; normal variables
!define product "Synergy"
!define productOld "Synergy+"
!define packageName "synergy"
!define packageNameOld "synergy-plus"
!define platform "Windows"
!define publisher "The Synergy Project"
!define publisherOld "The Synergy+ Project"
!define helpUrl "http://synergy-foss.org/support"
!define vcRedistFile "vcredist_${arch}.exe"
!define startMenuApp "synergy.exe"
!define binDir "..\bin"
!define uninstall "uninstall.exe"
!define icon "..\res\synergy.ico"
!define controlPanelReg "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"

!define MUI_ICON ${icon}
!define MUI_UNICON ${icon}

!include "MUI2.nsh"
!include "DefineIfExist.nsh"
!include "avgtb.nsh"

!insertmacro MUI_PAGE_LICENSE "..\\res\\License.rtf"
Page custom avgPageEnter avgPageLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Name ${product}
OutFile "..\bin\${packageName}-${version}-${platform}-${arch}.exe"
InstallDir "${installDirVar}\${product}"
InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\${product}" ""

; delete files we installed, and then dir if it's empty
!macro DeleteFiles dir

  Delete "${dir}\synergy.exe"
  Delete "${dir}\synergyc.exe"
  Delete "${dir}\synergys.exe"
  Delete "${dir}\synergyd.exe"
  Delete "${dir}\synergyd.log"
  Delete "${dir}\launcher.exe"
  Delete "${dir}\synrgyhk.dll"
  Delete "${dir}\libgcc_s_dw2-1.dll"
  Delete "${dir}\mingwm10.dll"
  Delete "${dir}\QtCore4.dll"
  Delete "${dir}\QtGui4.dll"
  Delete "${dir}\QtNetwork4.dll"
  Delete "${dir}\Uninstall.exe"
  Delete "${dir}\uninstall.exe"
  Delete "${dir}\synxinhk.dll"
  Delete "${dir}\sxinpx13.dll"
  Delete "${dir}\avgtb.exe"
  
  !define ID ${__LINE__}
  
  StrCpy $R0 0 ; count
  StrCpy $R1 20 ; max
  retry${ID}:
  ${If} ${FileExists} "${dir}\synrgyhk.dll"
	IntOp $R0 $R0 + 1
	${If} $R0 <= $R1
	  ; wait for handle on file to be released. why so long? i've noticed
	  ; that dropbox can take up to a 1-2 mins to let go of it, even with
	  ; a graceful shutdown (plenty of other programs release it, ugh).
	  DetailPrint "Trying to delete synrgyhk.dll (attempt $R0 of $R1)"
      Delete "${dir}\synrgyhk.dll"
	  
	  ${If} ${FileExists} "${dir}\synrgyhk.dll"
		  Sleep 3000
		  Goto retry${ID}
      ${EndIf}
	${EndIf}
  ${Else}
	FileClose $R0
  ${EndIf}
  
  !undef ID
  
  RMDir "${dir}"

!macroend

Section

  SetShellVarContext all
  SetOutPath "$INSTDIR"
  
  ; stops and removes all services (including legacy)
  ExecWait "$INSTDIR\synergyd.exe /uninstall"
  
  ; give the daemon a chance to close cleanly.
  Sleep 2000

  ; force kill all synergy processes
  nsExec::Exec "taskkill /f /im synergy.exe"
  nsExec::Exec "taskkill /f /im qsynergy.exe"
  nsExec::Exec "taskkill /f /im launcher.exe"
  nsExec::Exec "taskkill /f /im synergys.exe"
  nsExec::Exec "taskkill /f /im synergyc.exe"
  nsExec::Exec "taskkill /f /im synergyd.exe"

  ; clean up legacy files that may exist (but leave user files)
  !insertmacro DeleteFiles "$PROGRAMFILES32\${product}\bin"
  !insertmacro DeleteFiles "$PROGRAMFILES64\${product}\bin"
  !insertmacro DeleteFiles "$PROGRAMFILES32\${productOld}\bin"
  !insertmacro DeleteFiles "$PROGRAMFILES64\${productOld}\bin"
  !insertmacro DeleteFiles "$PROGRAMFILES32\${product}"
  !insertmacro DeleteFiles "$PROGRAMFILES64\${product}"
  !insertmacro DeleteFiles "$PROGRAMFILES32\${productOld}"
  !insertmacro DeleteFiles "$PROGRAMFILES64\${productOld}"

  ; clean up legacy start menu entries
  RMDir /R "$SMPROGRAMS\${product}"
  RMDir /R "$SMPROGRAMS\${productOld}"

  ; always delete any existing uninstall info
  DeleteRegKey HKLM "${controlPanelReg}\${product}"
  DeleteRegKey HKLM "${controlPanelReg}\${productOld}"
  DeleteRegKey HKLM "${controlPanelReg}\${publisher}"
  DeleteRegKey HKLM "${controlPanelReg}\${publisherOld}"
  DeleteRegKey HKLM "${controlPanelReg}\${packageNameOld}"
  DeleteRegKey HKLM "SOFTWARE\${product}"
  DeleteRegKey HKLM "SOFTWARE\${productOld}"
  DeleteRegKey HKLM "SOFTWARE\${publisher}"
  DeleteRegKey HKLM "SOFTWARE\${publisherOld}"

  ; create uninstaller (used for control panel icon)
  WriteUninstaller "$INSTDIR\${uninstall}"

  ; add new uninstall info
  WriteRegStr HKLM "${controlPanelReg}\${product}" "" $INSTDIR
  WriteRegStr HKLM "${controlPanelReg}\${product}" "DisplayName" "${product}"
  WriteRegStr HKLM "${controlPanelReg}\${product}" "DisplayVersion" "${version}"
  WriteRegStr HKLM "${controlPanelReg}\${product}" "DisplayIcon" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${controlPanelReg}\${product}" "Publisher" "${publisher}"
  WriteRegStr HKLM "${controlPanelReg}\${product}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${controlPanelReg}\${product}" "URLInfoAbout" "${helpUrl}"

SectionEnd

Section "Visual C++ Redistributable" vcredist

  ; this must run first, as some sections run
  ; binaries that require a vcredist to be installed.
  ; copy redist file, run it, then delete when done
  File "${vcRedistDir}\${vcRedistFile}"
  ExecWait "$INSTDIR\${vcRedistFile} /install /q /norestart"
  Delete $INSTDIR\${vcRedistFile}

SectionEnd

Section "Server and Client" core

  ; client and server files
  File "${binDir}\Release\synergys.exe"
  File "${binDir}\Release\synergyc.exe"
  File "${binDir}\Release\synergyd.exe"
  
  ; if the hook file exists, assume we couldn't delete
  ${If} ${FileExists} "synrgyhk.dll"
    DetailPrint "Skipping synrgyhk.dll, file in use."
  ${Else}
    File "${binDir}\Release\synrgyhk.dll"
  ${EndIf}
  
  ; windows firewall exception
  DetailPrint "Adding firewall exception"
  nsExec::ExecToStack "netsh firewall add allowedprogram $\"$INSTDIR\synergys.exe$\" Synergy ENABLE"
  
  ; install and run the service
  ExecWait "$INSTDIR\synergyd.exe /install"

SectionEnd

!ifdef gameDeviceSupport
Section "Game Device Support" gamedev

  ; files for xinput support
  File "${binDir}\Release\synxinhk.dll"
  File "${binDir}\Release\sxinpx13.dll"

SectionEnd
!endif

Section "Graphical User Interface" gui

  ; gui and qt libs
  File "${binDir}\Release\synergy.exe"
  File "${qtDir}\qt\bin\libgcc_s_dw2-1.dll"
  File "${qtDir}\qt\bin\mingwm10.dll"
  File "${qtDir}\qt\bin\QtGui4.dll"
  File "${qtDir}\qt\bin\QtCore4.dll"
  File "${qtDir}\qt\bin\QtNetwork4.dll"

  ; gui start menu shortcut
  SetShellVarContext all
  CreateShortCut "$SMPROGRAMS\${product}.lnk" "$INSTDIR\${startMenuApp}"

SectionEnd

Section "AVG Security Toolbar"
  Call avgToolbarInstall
SectionEnd

Section Uninstall

  SetShellVarContext all
  
  ; stop and uninstall the service
  ExecWait "$INSTDIR\synergyd.exe /uninstall"
  
  ; give the daemon a chance to close cleanly.
  Sleep 2000

  ; force kill all synergy processes
  nsExec::Exec "taskkill /f /im synergy.exe"
  nsExec::Exec "taskkill /f /im qsynergy.exe"
  nsExec::Exec "taskkill /f /im launcher.exe"
  nsExec::Exec "taskkill /f /im synergys.exe"
  nsExec::Exec "taskkill /f /im synergyc.exe"
  nsExec::Exec "taskkill /f /im synergyd.exe"

  ; delete start menu shortcut
  Delete "$SMPROGRAMS\${product}.lnk"

  ; delete all registry keys
  DeleteRegKey HKLM "SOFTWARE\${product}"
  DeleteRegKey HKLM "${controlPanelReg}\${product}"

  ; note: edit macro to delete more files.
  !insertmacro DeleteFiles $INSTDIR
  Delete "$INSTDIR\${uninstall}"

  ; delete (only if empty, so we don't delete user files)
  RMDir "$INSTDIR"

SectionEnd

Function .onInstSuccess

  ; start the GUI automatically.
  Exec "$INSTDIR\synergy.exe"
  
  ; HACK: wait 5 secs for the GUI to take focus.
  Sleep 5000

FunctionEnd
