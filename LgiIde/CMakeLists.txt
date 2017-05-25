cmake_minimum_required (VERSION 3.5)

SET (CMAKE_OSX_ARCHITECTURES "i386")

project (LgiIde)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()
add_subdirectory(.. LgiDir)

if (APPLE)
	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	SET(GUI_TYPE MACOSX_BUNDLE)
	INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
	FIND_LIBRARY(CARBON_LIBRARY Carbon)
	MARK_AS_ADVANCED (CARBON_LIBRARY)
	SET(EXTRA_LIBS ${CARBON_LIBRARY})
elseif (LINUX)
	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -fPIC -w -fno-inline -fpermissive")
	FIND_PACKAGE(GTK2 COMPONENTS gtk)
elseif (MSVC)
	add_definitions("/W2 -DWINDOWS -DWIN32 -DLGI_LIBRARY -D_DEBUG -DLGI_RES -D_WINDLL -D_UNICODE -DUNICODE")
endif ()

set (CommonSource
	Code/SimpleCppParser.cpp
	Code/WebFldDlg.cpp
	Code/IdeCommon.cpp
	Code/AddFtpFile.cpp
	Code/ProjectNode.cpp
	Code/IdeDoc.cpp
	Code/IdeProject.cpp
	Code/IdeProjectSettings.cpp
	Code/LgiIde.cpp
	Code/LgiUtils.cpp
	Code/MemDumpViewer.cpp
	Code/SpaceTabConv.cpp
	Code/SysCharSupport.cpp
	Code/GDebugContext.cpp
	Code/GDebugger.cpp
	Code/GHistory.cpp
	Code/FindInFiles.cpp
	Code/FindSymbol.cpp
	Code/FtpThread.cpp
	../src/common/INet/IFtp.cpp
	../src/common/Gdc2/Filters/Png.cpp
	../src/common/Lgi/GSubProcess.cpp
	../src/common/Lgi/GMdi.cpp
	../src/common/Lgi/LgiMain.cpp
	../src/common/Widgets/GControlTree.cpp
	../src/common/Coding/GParseCpp.cpp
	../src/common/Coding/GLexCpp.cpp
	)


if (APPLE)
	set(RESOURCE_FILES
		Code/cmds-32px.png
		Code/icons.png
		Code/cmds-16px.png
		Code/LgiIde.lr8
		Mac/Info.plist
		Mac/English.lproj/InfoPlist.strings
		Mac/English.lproj/main.nib)
	set_source_files_properties(${RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

	set(Source ${CommonSource})
	add_executable(LgiIde MACOSX_BUNDLE ${Source} ${RESOURCE_FILES})
	# set(MACOSX_BUNDLE_ICON_FILE ".icns")
	add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"$<TARGET_FILE:Lgi>"
						"LgiIde.app/Contents/MacOS/$<TARGET_FILE_NAME:Lgi>" 
					COMMENT "Copying to output directory")
elseif (LINUX)
	set(Source ${CommonSource})
	add_executable(LgiIde ${Source})
elseif (MSVC)
	set(Source ${CommonSource})
	add_executable(LgiIde ${Source})
endif ()

if (APPLE)
	target_include_directories(LgiIde PUBLIC "../include/mac/carbon" "../include/posix")
	target_link_libraries(LgiIde Lgi ${EXTRA_LIBS})
	set_target_properties(LgiIde PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_FRAMEWORK_IDENTIFIER com.memecode.LgiIde
		RESOURCE "${RESOURCE_FILES}"
		)
elseif (LINUX)
	target_include_directories(LgiIde PUBLIC "../include/linux/Gtk" "../include/linux" ${GTK2_INCLUDE_DIRS})
	target_link_libraries(LgiIde Lgi ${GTK2_LIBRARIES})
elseif (MSVC)
	target_include_directories(LgiIde PUBLIC "../include/win32")
	target_link_libraries(	LgiIde
						Lgi
						ComCtl32.lib
						Ws2_32.lib
						UxTheme.lib
						imm32.lib)
endif ()

target_include_directories (LgiIde PUBLIC
							"../include/common"
							"${CMAKE_CURRENT_SOURCE_DIR}/../../../../CodeLib/libpng"
							"${CMAKE_CURRENT_SOURCE_DIR}/../../../../CodeLib/libiconv-1.14/include")

