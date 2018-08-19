; lgi install/uninstall support

;!execute '"C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE" Lgi.dsp /make "Lgi - ALL"'
;!execute '"C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE" Gel\Gel.dsp /make "Gel - ALL"'

;--------------------------------
SetCompressor lzma

; The name of the installer
Name "Memecode Lgi SDK"

; The file to write
OutFile "lgi-v200.exe"

; The default installation directory
InstallDir $PROGRAMFILES\Memecode\Lgi

;--------------------------------

; Pages

Page directory
Page components
Page instfiles

;--------------------------------

; The stuff to install
Section ""

  ; Binaries
  SetOutPath $INSTDIR\libs
  ;File .\Debug\Lgid.dll
  ;File .\Debug\Lgid.lib
  ;File .\Release\Lgi.dll
  ;File .\Release\Lgi.lib
  ;File .\Gel\Debug\Lgiskind.dll
  ;File .\Gel\Release\Lgiskin.dll
  
  ; Headers
  SetOutPath $INSTDIR\include
  File /r include\*.h

  ; Source
  SetOutPath $INSTDIR\src
  File /r /x Mfs /x Zip32 /x .svn /x Ttf src\*.cpp
  File /r /x Mfs /x Zip32 /x .svn /x Ttf src\*.h

  ; Projects
  SetOutPath $INSTDIR
  File Readme.txt
  File license.txt
  ; Win32
  File Lgi.dsp
  File Lgi.dsw
  File Lgi.sln
  File Lgi.vcproj
  ; Linux
  File Makefile.*
  File LgiLinux.xml
  ; Mac
  File /r /x .svn English.lproj
  File /r /x .svn Lgi.xcode
  File /r /x .svn Lgi.xcodeproj

  ; Skin plugin
  SetOutPath $INSTDIR\Gel
  File Gel\Gel.ds*
  File Gel\Makefile.*

  ; LgiRes
  SetOutPath $INSTDIR\LgiRes
  File /r /x .svn LgiRes\Code
  File /r /x .svn /x Release /x Debug /x *.ncb /x *.opt /x *.plg LgiRes\LrStrip
  File LgiRes\LgiRes.ds*
  File LgiRes\LgiRes.xml
  File LgiRes\Makefile

  ; LgiIde
  SetOutPath $INSTDIR\LgiIde
  File /r /x .svn LgiIde\Code
  File LgiIde\LgiIde.ds*
  File LgiIde\LgiIde.xml
  File LgiIde\Makefile.*

  ; Help
  SetOutPath $INSTDIR\docs
  File /r docs\html\*.*

  ; Uninstaller
  WriteUninstaller $INSTDIR\uninstall.exe
  
SectionEnd ; end the section

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Memecode Lgi SDK"
  CreateShortCut "$SMPROGRAMS\Memecode Lgi SDK\Folder.lnk" "$INSTDIR"
  CreateShortCut "$SMPROGRAMS\Memecode Lgi SDK\Help.lnk" "$INSTDIR\docs\index.html" "" "$WINDIR\WINHLP32.EXE" 1
  CreateShortCut "$SMPROGRAMS\Memecode Lgi SDK\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd

Section "Open Folder"

  ExecShell "open" "$INSTDIR"
  SetAutoClose true
  
SectionEnd

UninstPage components
UninstPage instfiles

Section "un.Program and Start Menu Items"

  Delete $INSTDIR\*.*
  RMDir /r $INSTDIR
  Delete "$SMPROGRAMS\Memecode Lgi SDK\*.*"
  RMDir /r "$SMPROGRAMS\Memecode Lgi SDK"

  SetAutoClose true

SectionEnd


