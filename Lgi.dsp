# Microsoft Developer Studio Project File - Name="Lgi" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Lgi - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Lgi.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Lgi.mak" CFG="Lgi - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Lgi - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Lgi - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Lgi - Win32 Release"

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
F90=fl32.exe
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /GR /Zi /O1 /I "include\common" /I "include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "LGI_RES" /D "LGI_LIBRARY" /FD /c
# SUBTRACT CPP /WX /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 oleaut32.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib comctl32.lib ole32.lib Ws2_32.lib Version.lib /nologo /subsystem:windows /dll /debug /machine:I386 /OPT:REF
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
OutDir=.\Release
TargetPath=.\Release\Lgi.dll
TargetName=Lgi
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) .\lib	copy $(OutDir)\$(TargetName).lib .\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Lgi - Win32 Debug"

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
F90=fl32.exe
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GR /Zi /Od /I "include\common" /I "include\win32" /D "LGI_LIBRARY" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "LGI_RES" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib comctl32.lib ole32.lib Ws2_32.lib Version.lib /nologo /subsystem:windows /dll /incremental:no /debug /machine:I386 /out:"Debug/Lgid.dll"
# SUBTRACT LINK32 /profile
# Begin Special Build Tool
OutDir=.\Debug
TargetPath=.\Debug\Lgid.dll
TargetName=Lgid
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) .\lib	copy $(OutDir)\$(TargetName).lib .\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "Lgi - Win32 Release"
# Name "Lgi - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Core"

# PROP Default_Filter ""
# Begin Group "Libraries"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GLibrary.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GLibrary.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GLibraryUtils.h
# End Source File
# End Group
# Begin Group "Semaphores"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GSemaphore.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GSemaphore.h
# End Source File
# End Group
# Begin Group "Drag and Drop"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\win32\GCom.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GDragAndDrop.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GDragAndDrop.h
# End Source File
# End Group
# Begin Group "Menus"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\win32\Lgi\GMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GMenu.h
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Resource\LgiRes.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiRes.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Resource\Res.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\Res.h
# End Source File
# End Group
# Begin Group "Threads"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\win32\Lgi\GThread.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GThread.h
# End Source File
# Begin Source File

SOURCE=.\src\common\General\GThreadCommon.cpp
# End Source File
# End Group
# Begin Group "Variant"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GVariant.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GVariant.h
# End Source File
# End Group
# Begin Group "Stream"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\common\GDom.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GMemStream.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GStream.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GStream.h
# End Source File
# End Group
# Begin Group "Memory Subsystem"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\common\GArray.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GAutoPtr.h
# End Source File
# Begin Source File

SOURCE=.\src\common\General\GContainers.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GContainers.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GHashTable.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\General\GMem.cpp

!IF  "$(CFG)" == "Lgi - Win32 Release"

# ADD CPP /GX

!ELSEIF  "$(CFG)" == "Lgi - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\include\common\GMem.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GRefCount.h
# End Source File
# End Group
# Begin Group "File"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\General\GExeCheck.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\General\GFile.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GFile.h
# End Source File
# End Group
# Begin Group "Process"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GProcess.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GProcess.h
# End Source File
# End Group
# Begin Group "DateTime"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\General\GDateTime.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GDateTime.h
# End Source File
# End Group
# Begin Group "Crash Handler"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\win32\GSymLookup.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\LgiException.cpp
# End Source File
# End Group
# Begin Group "Skin"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Skins\Gel\Gel.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Path\GPath.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\win32\Lgi\GApp.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GGeneral.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GGuiUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GObject.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GPrinter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GToolTip.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GTrayIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GView.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GViewCommon.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GViewPriv.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\Lgi.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\LgiMsg.cpp
# End Source File
# End Group
# Begin Group "Widgets"

# PROP Default_Filter ""
# Begin Group "Css"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GCss.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GCss.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\common\Widgets\GBitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GButton.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GButton.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GCheckBox.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GCheckBox.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GCombo.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GCombo.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Widgets\GEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GEdit.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GLayout.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GList.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GList.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GListItemCheckBox.h
# End Source File
# Begin Source File

SOURCE=.\include\common\GListItemRadioBtn.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GPanel.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GPopup.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GPopup.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Widgets\GProgress.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GProgress.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GProgressDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GRadioGroup.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GRadioGroup.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GScrollBar.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GScrollBar.h
# End Source File
# Begin Source File

SOURCE=.\src\win32\Widgets\GSlider.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GSlider.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GSplitter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GStatusBar.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GTableLayout.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GTableLayout.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GTabView.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GTabView.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GTextLabel.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GTextLabel.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GToolBar.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GToolBar.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Widgets\GTree.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GTree.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiWidgets.h
# End Source File
# End Group
# Begin Group "General"

# PROP Default_Filter ""
# Begin Group "Mru"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GMru.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GMru.h
# End Source File
# End Group
# Begin Group "Clipboard"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\win32\Lgi\GClipBoard.cpp
# End Source File
# End Group
# Begin Group "Md5"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\General\md5.c
# End Source File
# Begin Source File

SOURCE=.\include\common\md5.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\common\INet\Base64.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GOptionsFile.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GOptionsFile.h
# End Source File
# Begin Source File

SOURCE=.\src\common\General\GPassword.cpp
# End Source File
# Begin Source File

SOURCE=..\_Common\Include\GPassword.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\LgiRand.cpp
# End Source File
# End Group
# Begin Group "Text"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Text\GString.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GString.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Text\GToken.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GToken.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Text\GUtf8.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GUtf8.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Text\GXmlTree.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GXmlTree.h
# End Source File
# End Group
# Begin Group "Graphics"

# PROP Default_Filter ""
# Begin Group "_Core"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Gdc2\GColourReduce.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Gdc2\Gdc2.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\Gdc2.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\GFuncs.cpp
# End Source File
# End Group
# Begin Group "Font"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GDisplayString.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GDisplayString.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GFont.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GFont.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GFont_A.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GFont_W.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GFontCodePages.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Font\GFontSystem.cpp
# End Source File
# End Group
# Begin Group "Applicators"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Gdc2\15Bit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\16Bit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\24Bit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\32Bit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\8Bit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\Alpha.cpp
# End Source File
# End Group
# Begin Group "Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Gdc2\Filters\GFilter.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GFilter.h
# End Source File
# End Group
# Begin Group "Rect/Region"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Gdc2\GRect.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GRect.h
# End Source File
# End Group
# Begin Group "Surfaces"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\win32\Gdc2\GMemDC.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Gdc2\GPrintDC.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Gdc2\GScreenDC.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Gdc2\GSurface.cpp
# End Source File
# End Group
# Begin Group "Gdi Leak"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\win32\Gdc2\GdiLeak.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GdiLeak.h
# End Source File
# End Group
# End Group
# Begin Group "Dialogs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\Lgi\GAlert.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Widgets\GDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\Lgi\GFileSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GFileSelect.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GFindReplace.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GFontSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\include\common\GFontSelect.h
# End Source File
# Begin Source File

SOURCE=.\src\common\Lgi\GInput.cpp
# End Source File
# End Group
# End Group
# Begin Group "LgiNet"

# PROP Default_Filter ""
# Begin Group "Source"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "NtlmAuth"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\common\INet\AuthNtlm\Smbdes.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\INet\AuthNtlm\Smbencrypt.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\INet\AuthNtlm\Smbmd4.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\INet\AuthNtlm\Smbutil.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\common\INet\INet.cpp
# End Source File
# Begin Source File

SOURCE=.\src\common\INet\INetTools.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\INet\MibAccess.cpp
# End Source File
# End Group
# Begin Group "Header"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\include\common\INet.h
# End Source File
# Begin Source File

SOURCE=.\include\common\INetTools.h
# End Source File
# End Group
# Begin Group "Mime Message"

# PROP Default_Filter ""
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\include\common\Lgi.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiClass.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiClasses.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiCommon.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiDefs.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiInc.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiInterfaces.h
# End Source File
# Begin Source File

SOURCE=.\include\common\LgiMsgs.h
# End Source File
# Begin Source File

SOURCE=.\include\win32\LgiOsClasses.h
# End Source File
# Begin Source File

SOURCE=.\include\win32\LgiOsDefs.h
# End Source File
# End Group
# End Target
# End Project
