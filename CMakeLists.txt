cmake_minimum_required (VERSION 3.5)

SET (CMAKE_OSX_ARCHITECTURES "i386")

project (Lgi)
set (LGI_VERSION_MAJOR 1)
set (LGI_VERSION_MINOR 0)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

if (APPLE)
	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	#SET(GUI_TYPE MACOSX_BUNDLE)
	INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
	FIND_LIBRARY(CARBON_LIBRARY Carbon)
	FIND_LIBRARY(COCOA_LIBRARY Cocoa)
	MARK_AS_ADVANCED (CARBON_LIBRARY COCOA_LIBRARY)
	SET(EXTRA_LIBS ${CARBON_LIBRARY} ${COCOA_LIBRARY})
elseif (LINUX)
	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -fPIC -w -fno-inline -fpermissive")
	FIND_PACKAGE(GTK2 COMPONENTS gtk)
elseif (MSVC)
	add_definitions("/W2 -DWINDOWS -DWIN32 -DLGI_LIBRARY -D_DEBUG -DLGI_RES -D_WINDLL -D_UNICODE -DUNICODE")
endif ()

set (CommonSource
	src/common/Gdc2/15Bit.cpp
	src/common/Gdc2/16Bit.cpp
	src/common/Gdc2/24Bit.cpp
	src/common/Gdc2/32Bit.cpp
	src/common/Gdc2/64Bit.cpp
	src/common/Gdc2/8Bit.cpp
	src/common/Gdc2/Alpha.cpp
	src/common/Gdc2/Filters/GFilter.cpp
	src/common/Gdc2/Font/GDisplayString.cpp
	src/common/Gdc2/Font/GFont.cpp
	src/common/Gdc2/Font/GFont_A.cpp
	src/common/Gdc2/Font/GFont_W.cpp
	src/common/Gdc2/Font/GFontCodePages.cpp
	src/common/Gdc2/Font/GFontSystem.cpp
	src/common/Gdc2/GColour.cpp
	src/common/Gdc2/GdcCommon.cpp
	src/common/Gdc2/GRect.cpp
	src/common/Gdc2/GSurface.cpp
	src/common/Gdc2/Path/GPath.cpp
	src/common/Gdc2/Tools/GColourReduce.cpp
	src/common/General/GContainers.cpp
	src/common/General/GDateTime.cpp
	src/common/General/GExeCheck.cpp
	src/common/General/GFileCommon.cpp
	src/common/General/GGrowl.cpp
	src/common/General/GPassword.cpp
	src/common/Hash/md5/md5.c
	src/common/Hash/sha1/sha1.c
	src/common/INet/Base64.cpp
	src/common/INet/GUri.cpp
	src/common/INet/INet.cpp
	src/common/INet/INetTools.cpp
	src/common/INet/MDStringToDigest.cpp
	src/common/Lgi/GAlert.cpp
	src/common/Lgi/GCss.cpp
	src/common/Lgi/GCssTools.cpp
	src/common/Lgi/GFindReplace.cpp
	src/common/Lgi/GFontSelect.cpp
	src/common/Lgi/GGuiUtils.cpp
	src/common/Lgi/GInput.cpp
	src/common/Lgi/GLibrary.cpp
	src/common/Lgi/GMemStream.cpp
	src/common/Lgi/GMru.cpp
	src/common/Lgi/GMutex.cpp
	src/common/Lgi/GObject.cpp
	src/common/Lgi/GOptionsFile.cpp
	src/common/Lgi/GProcess.cpp
	src/common/Lgi/GStream.cpp
	src/common/Lgi/GThreadCommon.cpp
	src/common/Lgi/GThreadEvent.cpp
	src/common/Lgi/GToolTip.cpp
	src/common/Lgi/GTrayIcon.cpp
	src/common/Lgi/GVariant.cpp
	src/common/Lgi/GViewCommon.cpp
	src/common/Lgi/GWindowCommon.cpp
	src/common/Lgi/Lgi.cpp
	src/common/Lgi/LgiMsg.cpp
	src/common/Lgi/LgiRand.cpp
	src/common/Resource/LgiRes.cpp
	src/common/Resource/Res.cpp
	src/common/Skins/Gel/Gel.cpp
	src/common/Text/GDocView.cpp
	src/common/Text/GString.cpp
	src/common/Text/GTextView3.cpp
	src/common/Text/GToken.cpp
	src/common/Text/GUnicode.cpp
	src/common/Text/GUtf8.cpp
	src/common/Text/GXmlTree.cpp
	src/common/Widgets/GBitmap.cpp
	src/common/Widgets/GBox.cpp
	src/common/Widgets/GItemContainer.cpp
	src/common/Widgets/GList.cpp
	src/common/Widgets/GPanel.cpp
	src/common/Widgets/GPopup.cpp
	src/common/Widgets/GProgressDlg.cpp
	src/common/Widgets/GSplitter.cpp
	src/common/Widgets/GStatusBar.cpp
	src/common/Widgets/GTableLayout.cpp
	src/common/Widgets/GTabView.cpp
	src/common/Widgets/GTextLabel.cpp
	src/common/Widgets/GToolBar.cpp	
	src/common/Widgets/GTree.cpp
	)

set (WindowsSource
	src/win32/Gdc2/Gdc2.cpp
	src/win32/Gdc2/GdiLeak.cpp
	src/win32/Gdc2/GMemDC.cpp
	src/win32/Gdc2/GPrintDC.cpp
	src/win32/Gdc2/GScreenDC.cpp
	src/win32/General/GFile.cpp
	src/win32/General/GMem.cpp
	src/win32/General/ShowFileProp_Win.cpp
	src/win32/INet/MibAccess.cpp
	src/win32/Lgi/GApp.cpp
	src/win32/Lgi/GClipBoard.cpp
	src/win32/Lgi/GDragAndDrop.cpp
	src/win32/Lgi/GFileSelect.cpp
	src/win32/Lgi/GGeneral.cpp
	src/win32/Lgi/GLayout.cpp
	src/win32/Lgi/GMenu.cpp
	src/win32/Lgi/GPrinter.cpp
	src/win32/Lgi/GScrollBar.cpp
	src/win32/Lgi/GThread.cpp
	src/win32/Lgi/GView.cpp
	src/win32/Lgi/GWindow.cpp
	src/win32/Lgi/LgiException.cpp
	src/win32/Widgets/GButton_Win.cpp
	src/win32/Widgets/GCheckBox_Win.cpp
	src/win32/Widgets/GCombo_Win.cpp
	src/win32/Widgets/GDialog.cpp
	src/win32/Widgets/GEdit_Win.cpp
	src/win32/Widgets/GProgress.cpp
	src/win32/Widgets/GRadioGroup_Win.cpp
	src/win32/Widgets/GSlider.cpp
	)


set (LinuxSource
	src/linux/Gtk/Gdc2.cpp
	src/linux/Gtk/GMemDC.cpp
	src/linux/Gtk/GPrintDC.cpp
	src/linux/Gtk/GScreenDC.cpp
	src/linux/General/GFile.cpp
	src/linux/General/GMem.cpp
	src/linux/General/ShowFileProp_Linux.cpp
	src/linux/Lgi/GApp.cpp
	src/linux/Lgi/GClipBoard.cpp
	src/linux/Lgi/GDragAndDrop.cpp
	src/linux/Lgi/GGeneral.cpp
	src/linux/Lgi/GLayout.cpp
	src/linux/Lgi/GMenuGtk.cpp
	src/linux/Lgi/GPrinter.cpp
	src/linux/Lgi/GThread.cpp
	src/linux/Lgi/GView.cpp
	src/linux/Lgi/GWidgets.cpp
	src/linux/Lgi/GWindow.cpp
	)

set (MacSource
	src/common/Widgets/GCheckBox.cpp
	src/common/Widgets/GCombo.cpp
	src/common/Widgets/GScrollBar.cpp
	src/common/Widgets/GButton.cpp
	src/common/Widgets/GRadioGroup.cpp
	src/common/Widgets/GEdit.cpp
	src/common/Lgi/GLayout.cpp
	src/common/Widgets/GProgress.cpp
	src/mac/cocoa/General/ShowFileProp_Mac.mm
	src/mac/carbon/Gdc2/Gdc2.cpp
	src/mac/carbon/Gdc2/GMemDC.cpp
	src/mac/carbon/Gdc2/GPrintDC.cpp
	src/mac/carbon/Gdc2/GScreenDC.cpp
	src/mac/carbon/General/GFile.cpp
	src/mac/carbon/General/GMem.cpp
	src/mac/carbon/Lgi/GApp.cpp
	src/mac/carbon/Lgi/GClipBoard.cpp
	src/mac/carbon/Lgi/GDragAndDrop.cpp
	src/mac/carbon/Lgi/GGeneral.cpp
	src/mac/carbon/Lgi/GFileSelect.cpp
	src/mac/carbon/Lgi/GMenu.cpp
	src/mac/carbon/Lgi/GPrinter.cpp
	src/mac/carbon/Lgi/GThread.cpp
	src/mac/carbon/Lgi/GView.cpp
	src/mac/carbon/Lgi/GWidgets.cpp
	src/mac/carbon/Lgi/GWindow.cpp
	)

if (APPLE)
	set(Source ${CommonSource} ${MacSource})
elseif (LINUX)
	set(Source ${CommonSource} ${LinuxSource})
elseif (MSVC)
	set(Source ${CommonSource} ${WindowsSource})
endif ()

add_library(Lgi SHARED ${Source})

if (APPLE)
	target_include_directories(Lgi PUBLIC "include/mac/carbon" "include/posix")
	target_link_libraries(Lgi ${EXTRA_LIBS})
elseif (LINUX)
	target_include_directories(Lgi PUBLIC "include/linux/Gtk" "include/linux" ${GTK2_INCLUDE_DIRS})
	target_link_libraries(Lgi ${GTK2_LIBRARIES})
elseif (MSVC)
	target_include_directories(Lgi PUBLIC "include/win32")
	target_link_libraries(	Lgi
						ComCtl32.lib
						Ws2_32.lib
						UxTheme.lib
						imm32.lib)
endif ()

target_include_directories (Lgi PUBLIC
							"include/common"
							"${CMAKE_CURRENT_SOURCE_DIR}/../../../CodeLib/libiconv-1.14/include")



set_target_properties(Lgi PROPERTIES LINKER_LANGUAGE CXX)
