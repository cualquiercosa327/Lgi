; LgiRes install/uninstall support

!system "mkdir lgires-setup"
!system "del .\lgires-setup\*.*"

!system "copy ..\lib\Lgi8.dll lgires-setup"
!system "copy .\Win32Release\*.exe lgires-setup"

!system '"c:\Program Files\Upx\upx.exe" -9 .\lgires-setup\*.exe'
!system '"c:\Program Files\Upx\upx.exe" -9 .\lgires-setup\*.dll'

;--------------------------------
SetCompressor lzma

; The name of the installer
Name "Memecode LgiRes"

; The file to write
OutFile "lgires-win32-v#.#.exe"

; The default installation directory
InstallDir $PROGRAMFILES\Memecode\LgiRes

;--------------------------------

; Pages

Page directory
Page components
Page instfiles

;--------------------------------

; The stuff to install
Section ""

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Program files
  File .\lgires-setup\*.exe
  File .\lgires-setup\*.dll

  ; Resources
  File .\Code\lgires.lr8
  File .\Code\*.gif

  ; Uninstaller
  WriteUninstaller $INSTDIR\uninstall.exe
  
SectionEnd ; end the section

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\LgiRes"
  CreateShortCut "$SMPROGRAMS\LgiRes\LgiRes.lnk" "$INSTDIR\LgiRes.exe" "" "$INSTDIR\LgiRes.exe" 0
  CreateShortCut "$SMPROGRAMS\LgiRes\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd

Section "Run LgiRes"

  ExecShell "open" "$INSTDIR\LgiRes.exe"
  SetAutoClose true
  
SectionEnd

UninstPage components
UninstPage instfiles

Section "un.Program and Start Menu Items"

  Delete $INSTDIR\*.*
  RMDir /r $INSTDIR
  Delete $SMPROGRAMS\LgiRes\*.*
  RMDir /r $SMPROGRAMS\LgiRes

  SetAutoClose true

SectionEnd

; Clean up temp files
!system "del /Q .\lgires-setup\*.*"
!system "rmdir lgires-setup"

