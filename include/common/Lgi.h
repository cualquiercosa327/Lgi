/**
	\file
	\author Matthew Allen
	\date 24/9/1999
	\brief Global LGI include.
	Copyright (C) 1999-2004, Matthew Allen
*/

/**
	\mainpage Lightweight GUI Library
	\section intro Introduction
	This library is aimed at producing small, quality applications that
	run on multiple platforms.\n
	\n
	The way this is acheived is by implementing a common API that is simple to use
	and allows access to the features of the supported platforms in a (preferably)
	highest common denominator fashion. Features should be synthesised when not 
	present if practical.\n

	\section setup Setup
	Initially you should configure the required and recomended libraries that Lgi
	uses.
	<ul>
		<li> <a href="http://www.gnu.org/software/libiconv">iconv</a>
		<li> <a href="http://freshmeat.net/projects/libjpeg/">libjpeg</a>
		<li> <a href="http://www.libpng.org/pub/png/libpng.html">libpng</a>
		<li> <a href="http://www.gzip.org/zlib/">zlib</a>
		<li> <a href="http://www.memecode.com/libsharedmime.php">libsharedmime</a>
	</ul>
	If you don't have any of these libraries then you should set the appropriate define
	in Lgi.h to '0'\n

	\section vc Visual C++ Project Setup
	If your building on Win32 with Visual C++ then you'll need to make some edits to the
	default project settings to make linking against the library work. Heres a list of 
	the things you need to change from the defaults:
	<ul>
		<li> Project Settings:
		<ul>
			<li> C++ Tab
			<ul>
				<li> General Section:
				<ul>
					<li> Warning level: <b>1</b>
					<li> [Optional] Release Mode - Optimizations: <b>Smaller</b>
					<li> [Optional] Debug Mode - Debug Info: <b>Program Database</b>
				</ul>
				<li> C++ Language:
				<ul>
					<li> [Optional] RTTI: <b>On</b>
				</ul>
				<li> Code Generation:
				<ul>
					<li> Release Mode - Use runtime library: <b>Multi-threaded DLL</b>
					<li> Debug Mode - Use runtime library: <b>Debug Multi-threaded DLL</b>
				</ul>
				<li> Preprocessor:
				<ul>
					<li> [Optional] Additional Include Directories: <b>..\\lgi\\include\\common,..\\lgi\\include\\win32</b> or
						whatever the paths to the Lgi include directories are. Alternatively you can add the include
						paths to Visual C++'s Tools -> Options -> Directories window.
				</ul>
			</ul>
			<li> Link
			<ul>
				<li> General
				<ul>
					<li> [Optional, if using GTextView3] Additional Libraries: <b>imm32.lib</b>
				</ul>
			</ul>
		</ul>
	</ul>
	
	\section start Where To Start
	Firstly include the file LgiMain.cpp into your project, which will handle some of
	the setup for and then call LgiMain(). In your own source code implement LgiMain()
	like this:
	\code
	int LgiMain(OsAppArguments &Args)
	{
		GApp *App = new GApp("application/x-MyProgram", Args);
		if (App && App->IsOk())
		{
			App->AppWnd = new MyWindow;
			App->Run();
			delete App;
		}

		return 0;
	}
	\endcode
	Where 'MyWindow' is a class that inherits from GWindow. This class should initialize 
	the user interface for your application and then make itself visible. This is typically
	done like this:
	\code
	class MyWindow : public GWindow
	{
	public:
		MyWindow()
		{
			GRect r(0, 0, 700, 600);
			SetPos(r);
			MoveToCenter();
			SetQuitOnClose(true);
			Name("My Application");
			
			#ifdef WIN32
			CreateClass("MyApp");
			#endif
			
			if (Attach(0))
			{
				// Put code to create a menu, toolbar and splitter here

				Visible(true);
			}
		}
	};
	\endcode
	This will put a simple empty window on the screen. The sorts of classes you'll be using to
	fill in the contents of the window will be:
	<ul>
		<li> GMenu
		<li> GSplitter
		<li> GToolBar
		<li> GStatusBar
		<li> GLayout
		<li> GButton
		<li> GList
		<li> GTree
		<li> etc...
	</ul>
	A GWindow will layout controls automatically according to the order you attach (GView::Attach) them.
	The first window gets to lay itself out anywhere in the client area, and once it's positioned itself,
	the second window gets the area left over. And so on until the window is filled. There are a number of
	helper functions for layout:
	<ul>
		<li> GView::FindLargest
		<li> GView::FindSmallestFit
		<li> GView::FindLargestEdge
	</ul>
	Usually you attach a GToolBar first, as it will automatically slide in below the menu (if any). Then a
	GStatusBar, if your going to need one. And finally a GSplitter or GList to fill out the rest of the space.
	The standard method of attaching a view to a GWindow in the constructor of your window:
	\code
	GSplitter *s = new GSplitter;
	if (s)
	{
		s->Value(200);
		s->Attach(this);
		s->SetViewA(new MyChildWindow1, false);
		s->SetViewB(new MyChildWindow2, false);
	}
	\endcode
	Where MyChildWindow1 and MyChildWindow2 are GView based classes.
	
	\section dialogs Dialogs
	At some point you may want to put a dialog on the screen to get input from the user or provide the user
	with information. There are several pre-rolled dialogs, as well as support for custom dialogs. Firstly the
	pre-rolled dialogs are:
	<ul>
		<li> ::LgiMsg
		<li> GAlert
		<li> GInput
		<li> GFileSelect
		<li> GFontSelect
		<li> GPrinter::Browse
		<li> GProgressDlg
		<li> GFindDlg
		<li> GReplaceDlg
		<li> GAbout
	</ul>
	To create your own custom dialog you derive from:
	<ul>
		<li> GDialog
	</ul>
	Throughout Lgi applications windows can be constructed from a variety of Controls (or Widgets, or Views)
	that form the common methods for interacting with users and displaying information. Lgi has a rich set
	of bundled controls as well as support for writing your own. Here is an index of the built in controls:
	<ul>
		<li> GButton (Push button)
		<li> GEdit (Editbox for text entry)
		<li> GText (Static label)
		<li> GCheckBox (Independant boolean)
		<li> GCombo (Select one from many)
		<li> GSlider (Select a position)
		<li> GBitmap (Display an image)
		<li> GProgress (Show a progress)
		<li> GList (List containing GListItem)
		<li> GTree (Heirarchy of GTreeItem)
		<li> GRadioGroup (One of many selection using GRadioButton)
		<li> GTabView (Containing GTabPage)
		<li> GHtml
		<li> GScrollBar
	</ul>
	However it's fairly easy to write custom controls by inheriting from GLayout (or GView for the hardcore) and implementing 
	the required functionality by override various events like GView::OnPaint and GView::OnMouseClick.
*/

#ifndef __LGI_H
#define __LGI_H

/// Library version
#define LGI_VER								"3.2.1"

/// \brief Define as '1' if Iconv is available else as '0'
/// \sa http://www.gnu.org/software/libiconv
#ifdef MAC
#define HAS_ICONV							1
#else
#define HAS_ICONV							1
#endif
/// \brief Define as '1' if libjpeg is available else as '0'
/// \sa http://freshmeat.net/projects/libjpeg/?topic_id=105%2C809
#define HAS_LIBJPEG							1
/// \brief Define as '1' if libpng and zlib are available else as '0'
/// \sa http://www.libpng.org/pub/png/libpng.html
/// and http://www.gzip.org/zlib
#define HAS_LIBPNG_ZLIB						1
/// \brief Define as '1' if libsharedmime is available else as '0'
/// \sa http://www.memecode.com/libsharedmime.php
#define HAS_SHARED_MIME						0
#define HAS_LIBPNG_ZLIB						1
#define HAS_FILE_CMD						0
#define HAS_SHARED_OBJECT_SKIN				0
#define HAS_LIBGSASL						0

// My includes
#include "LgiInc.h"							// Xp

#include "LgiOsDefs.h"						// Platform specific 
#include "LgiMsgs.h"						// Xp
#include "LgiDefs.h"						// Xp
#include "GMem.h"                           // Platform specific 
#include "GString.h"						// Xp
#include "LgiClass.h"						// Xp
#include "Progress.h"						// Xp
#include "GFile.h"							// Platform specific
#include "Gdc2.h"							// Platform specific

#include "LgiRes.h"							// Xp
#include "LgiClasses.h" 					// Xp
#include "LgiCommon.h"						// Xp

#ifdef SKIN_MAGIC
#include "SkinMagicLib.h"
#endif

#endif
