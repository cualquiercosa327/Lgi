# Microsoft Developer Studio Project File - Name="LgiIde" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=LgiIde - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LgiIde.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LgiIde.mak" CFG="LgiIde - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LgiIde - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "LgiIde - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "LgiIde - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /GR /GX /O1 /I "..\include\common" /I "..\include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib imm32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "LgiIde - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GR /GX /Zi /Od /I "..\include\common" /I "..\include\win32" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib imm32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "LgiIde - Win32 Release"
# Name "LgiIde - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "LexCpp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\common\Coding\GLexCpp.cpp
# End Source File
# Begin Source File

SOURCE=..\include\common\GLexCpp.h
# End Source File
# End Group
# Begin Group "Ftp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Code\FtpThread.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\FtpThread.h
# End Source File
# Begin Source File

SOURCE=..\src\common\INet\IFtp.cpp
# End Source File
# Begin Source File

SOURCE=..\include\common\IFtp.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Code\FindInFiles.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\FindInFiles.h
# End Source File
# Begin Source File

SOURCE=.\Code\GHistory.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\GHistory.h
# End Source File
# Begin Source File

SOURCE=.\Code\IdeDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\IdeDoc.h
# End Source File
# Begin Source File

SOURCE=.\Code\IdeProject.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\IdeProject.h
# End Source File
# Begin Source File

SOURCE=.\Code\LgiIde.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\LgiIde.h
# End Source File
# Begin Source File

SOURCE=.\Code\LgiUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\MemDumpViewer.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\SpaceTabConv.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\SpaceTabConv.h
# End Source File
# Begin Source File

SOURCE=.\Code\SysCharSupport.cpp
# End Source File
# End Group
# Begin Group "Lgi"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\src\common\Text\GDocView.cpp
# End Source File
# Begin Source File

SOURCE=..\src\common\Lgi\GMdi.cpp
# End Source File
# Begin Source File

SOURCE=..\include\common\GMdi.h
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

SOURCE=..\src\common\Gdc2\Filters\Png.cpp
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Code\resource.h
# End Source File
# Begin Source File

SOURCE=.\Code\Script1.rc
# End Source File
# End Group
# End Target
# End Project
