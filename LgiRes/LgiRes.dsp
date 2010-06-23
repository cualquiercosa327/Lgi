# Microsoft Developer Studio Project File - Name="LgiRes" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=LgiRes - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LgiRes.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LgiRes.mak" CFG="LgiRes - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LgiRes - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "LgiRes - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "LgiRes - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /GR /GX /O2 /I "..\include\common" /I "..\include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "LGI_RES" /D "LGI_SHARED" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib winmm.lib imm32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "LgiRes - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /Gm /GR /GX /Zi /Od /I "..\include\common" /I "..\include\win32" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "LGI_RES" /D "LGI_SHARED" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib winmm.lib imm32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "LgiRes - Win32 Release"
# Name "LgiRes - Win32 Debug"
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\do release compress.bat"
# End Source File
# Begin Source File

SOURCE=.\Code\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\Code\Script1.rc
# End Source File
# End Group
# Begin Group "Objects"

# PROP Default_Filter ""
# Begin Group "Dialog"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Code\LgiRes_ControlTree.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\LgiRes_Dialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\LgiRes_Dialog.h
# End Source File
# Begin Source File

SOURCE=.\Code\LgiRes_TableLayout.cpp
# End Source File
# End Group
# Begin Group "Menu"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Code\LgiRes_Menu.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\LgiRes_Menu.h
# End Source File
# End Group
# Begin Group "String"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Code\LgiRes_String.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\LgiRes_String.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Code\LgiResApp.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\LgiResEdit.h
# End Source File
# Begin Source File

SOURCE=.\Code\Search.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\ShowLanguages.cpp
# End Source File
# End Group
# Begin Group "Lgi"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\common\Lgi\GAbout.cpp
# End Source File
# Begin Source File

SOURCE=..\src\common\Lgi\GDocApp.cpp
# End Source File
# Begin Source File

SOURCE=..\include\common\GDocApp.h
# End Source File
# Begin Source File

SOURCE=..\src\common\Text\GDocView.cpp
# End Source File
# Begin Source File

SOURCE=..\include\common\GDocView.h
# End Source File
# Begin Source File

SOURCE=..\src\common\Gdc2\Filters\Gif.cpp
# End Source File
# Begin Source File

SOURCE=..\src\common\Text\GTextView3.cpp
# End Source File
# Begin Source File

SOURCE=..\include\common\GTextView3.h
# End Source File
# Begin Source File

SOURCE=..\src\common\Lgi\LgiMain.cpp
# End Source File
# Begin Source File

SOURCE=..\src\common\Gdc2\Filters\Lzw.cpp
# End Source File
# End Group
# Begin Group "Installer"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Installer.nsi
# End Source File
# End Group
# End Target
# End Project
