; LgiRes install/uninstall support
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
  File ..\lib\Lgi12x64.dll
  File .\x64Release12\LgiRes.exe
  File ..\lib\libpng12x64.dll

  ; Resources
  File .\Resources\lgires.lr8
  File .\Resources\icon64.png
  File .\Code\*.gif

  ; Uninstaller
  WriteUninstaller $INSTDIR\uninstall.exe
  
SectionEnd ; end the section

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Memecode LgiRes"
  CreateShortCut "$SMPROGRAMS\Memecode LgiRes\LgiRes.lnk" "$INSTDIR\LgiRes.exe" "" "$INSTDIR\LgiRes.exe" 0
  CreateShortCut "$SMPROGRAMS\Memecode LgiRes\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
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
  Delete "$SMPROGRAMS\Memecode LgiRes\*.*"
  RMDir /r "$SMPROGRAMS\Memecode LgiRes"

  SetAutoClose true

SectionEnd
