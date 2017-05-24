cmake_minimum_required (VERSION 3.5)

set (LGI_VERSION_MAJOR 1)
set (LGI_VERSION_MINOR 0)
add_definitions("/W2 -DWINDOWS -DWIN32 -DLGI_LIBRARY -D_DEBUG -DLGI_RES -D_WINDLL -D_UNICODE -DUNICODE")

include_directories (
	"include/common"
	"include/win32"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../CodeLib/libiconv-1.14/include"
	)

set (Source
	src/win32/Lgi/GGeneral.cpp
	src/common/Lgi/GGuiUtils.cpp
	src/common/Lgi/GObject.cpp
	src/common/Lgi/GOptionsFile.cpp
	src/win32/Lgi/GPrinter.cpp
	src/common/Lgi/GToolTip.cpp
	src/common/Lgi/GTrayIcon.cpp
	src/common/Lgi/Lgi.cpp
	src/win32/Lgi/LgiException.cpp
	src/common/Lgi/LgiMsg.cpp
	src/common/General/GDateTime.cpp
	src/common/General/GExeCheck.cpp
	src/common/General/GFileCommon.cpp
	src/common/Lgi/GLibrary.cpp
	src/common/General/GContainers.cpp
	src/common/Lgi/GMemStream.cpp
	src/win32/Lgi/GDragAndDrop.cpp
	src/win32/General/ShowFileProp_Win.cpp
	src/win32/General/GMem.cpp
	src/win32/General/GFile.cpp
	src/win32/Lgi/GMenu.cpp
	src/common/Lgi/GMutex.cpp
	src/common/Lgi/GProcess.cpp
	src/common/Resource/LgiRes.cpp
	src/common/Resource/Res.cpp
	src/common/Skins/Gel/Gel.cpp
	src/common/Lgi/GStream.cpp
	src/win32/Lgi/GThread.cpp
	src/common/Lgi/GThreadCommon.cpp
	src/common/Lgi/GThreadEvent.cpp
	src/common/Lgi/GVariant.cpp
	src/common/Lgi/GAlert.cpp
	src/win32/Widgets/GDialog.cpp
	src/win32/Lgi/GFileSelect.cpp
	src/common/Lgi/GFindReplace.cpp
	src/common/Lgi/GFontSelect.cpp
	src/common/Lgi/GInput.cpp
	src/common/General/GPassword.cpp
	src/win32/Lgi/GClipBoard.cpp
	src/common/General/GGrowl.cpp
	src/common/Hash/md5/md5.c
	src/common/Hash/sha1/sha1.c
	src/common/Lgi/GMru.cpp
	src/common/INet/Base64.cpp
	src/common/Lgi/LgiRand.cpp
	src/common/Gdc2/8Bit.cpp
	src/common/Gdc2/15Bit.cpp
	src/common/Gdc2/16Bit.cpp
	src/common/Gdc2/24Bit.cpp
	src/common/Gdc2/32Bit.cpp
	src/common/Gdc2/64Bit.cpp
	src/common/Gdc2/Alpha.cpp
	src/common/Gdc2/Tools/GColourReduce.cpp
	src/common/Gdc2/Filters/GFilter.cpp
	src/common/Gdc2/Font/GDisplayString.cpp
	src/common/Gdc2/Font/GFont.cpp
	src/common/Gdc2/Font/GFont_A.cpp
	src/common/Gdc2/Font/GFont_W.cpp
	src/common/Gdc2/Font/GFontCodePages.cpp
	src/common/Gdc2/Font/GFontSystem.cpp
	src/win32/Gdc2/GdiLeak.cpp
	src/common/Gdc2/GRect.cpp
	src/win32/Gdc2/GMemDC.cpp
	src/win32/Gdc2/GPrintDC.cpp
	src/win32/Gdc2/GScreenDC.cpp
	src/common/Gdc2/GSurface.cpp
	src/common/Gdc2/GColour.cpp
	src/win32/Gdc2/Gdc2.cpp
	src/common/Gdc2/GdcCommon.cpp
	src/common/Gdc2/Path/GPath.cpp
	src/win32/Lgi/GApp.cpp
	src/win32/Lgi/GView.cpp
	src/common/Lgi/GViewCommon.cpp
	src/win32/Lgi/GWindow.cpp
	src/common/Lgi/GWindowCommon.cpp
	src/common/INet/GUri.cpp
	src/common/INet/INet.cpp
	src/common/INet/INetTools.cpp
	src/common/INet/MDStringToDigest.cpp
	src/win32/INet/MibAccess.cpp
	src/common/Text/GString.cpp
	src/common/Text/GToken.cpp
	src/common/Text/GUnicode.cpp
	src/common/Text/GUtf8.cpp
	src/common/Text/GXmlTree.cpp
	src/common/Widgets/GItemContainer.cpp
	src/common/Widgets/GList.cpp
	src/common/Widgets/GTree.cpp
	src/common/Lgi/GCss.cpp
	src/common/Lgi/GCssTools.cpp
	src/common/Widgets/GBox.cpp
	src/common/Widgets/GTableLayout.cpp
	src/win32/Widgets/GButton_Win.cpp
	src/win32/Widgets/GCheckBox_Win.cpp
	src/win32/Widgets/GCombo_Win.cpp
	src/win32/Widgets/GEdit_Win.cpp
	src/win32/Widgets/GRadioGroup_Win.cpp
	src/common/Widgets/GBitmap.cpp
	src/common/Text/GDocView.cpp
	src/win32/Lgi/GLayout.cpp
	src/common/Widgets/GPanel.cpp
	src/common/Widgets/GPopup.cpp
	src/win32/Widgets/GProgress.cpp
	src/common/Widgets/GProgressDlg.cpp
	src/win32/Lgi/GScrollBar.cpp
	src/win32/Widgets/GSlider.cpp
	src/common/Widgets/GSplitter.cpp
	src/common/Widgets/GStatusBar.cpp
	src/common/Widgets/GTabView.cpp
	src/common/Widgets/GTextLabel.cpp
	src/common/Text/GTextView3.cpp
	src/common/Widgets/GToolBar.cpp	
	)

add_library(Lgi SHARED
			${Source}
			)

target_link_libraries(Lgi
					ComCtl32.lib
					Ws2_32.lib
					UxTheme.lib
					imm32.lib)

set_target_properties(Lgi PROPERTIES LINKER_LANGUAGE CXX)
