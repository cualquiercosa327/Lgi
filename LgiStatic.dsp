# Microsoft Developer Studio Project File - Name="LgiStatic" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=LgiStatic - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LgiStatic.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LgiStatic.mak" CFG="LgiStatic - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LgiStatic - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "LgiStatic - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "LgiStatic - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "LgiStatic___Win32_Release"
# PROP BASE Intermediate_Dir "LgiStatic___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "LgiStaticRelease"
# PROP Intermediate_Dir "LgiStaticRelease"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /O1 /I "include/win32" /I "include/common" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "LGI_STATIC" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "LgiStatic - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "LgiStatic___Win32_Debug"
# PROP BASE Intermediate_Dir "LgiStatic___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "LgiStaticDebug"
# PROP Intermediate_Dir "LgiStaticDebug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /Zi /Od /I "include/win32" /I "include/common" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "LGI_STATIC" /D "_WINDOWS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "LgiStatic - Win32 Release"
# Name "LgiStatic - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\common\General\GContainers.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\General\GDateTime.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\General\GFile.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GFontCodePages.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GGeneral.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GLibrary.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\General\GMem.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GProcess.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GSemaphore.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GStream.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Text\GString.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GThread.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Text\GToken.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GVariant.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\Lgi.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\LgiMsg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\LgiRand.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\include\common\GContainers.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GFont.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiClasses.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiCommon.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiInc.h
# End Source File
# End Group
# End Target
# End Project
