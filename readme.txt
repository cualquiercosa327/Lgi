	    Lightweight GUI Library
	    -----------------------

The primary aim of LGI is to abstract away the differences between
operating system's and provide a consistent API that applications
can target.

As a secondary goal the library should strive to be compact for easy
distribution with an application without bloating out the download.
Also the API is designed to be as simple as possible for the programmer
to use, without sacrificing functionality on the various platforms
supported.

The library is not intended to be shared between applications
as there is too much change in the API and object size to warrant a
shared library approach. In the future this may change if compatibility
can be improved.

Compiling LGI
-------------

    Open '[lgi]/include/common/Lgi.h' and check through any compile time
    options there. You may want to switch things on or off to get it to
    compile.

    Win32:
    	Load  [lgi]/Lgi_vc9.sln into Visual C++ 2008 and build it.
    
    Linux:
    	make -f [lgi]/Makefile.linux
    		-or-
    	make -f [lgi]/LgiIde/Makefile.linux (builds both Lgi and the IDE)
    
    Cygwin:
    	make -f [lgi]/Makefile.win32
    		-or-
    	make -f [lgi]/LgiIde/Makefile.win32 (builds both Lgi and the IDE)
    
    Mac:
    	Open [lgi]/src/mac/carbon/Lgi.xcode in XCode and run the build command. 

	
	Add build folders so the OS can find the shared libraries:

		* For Windows add these to your path:
			- [lgi]/lib
		* For Cygwin add these to your path:
			- [lgi]/DebugX
			- [lgi]/ReleaseX
		* On Linux, create symlinks in /usr/lib to the files:
			- [lgi]/DebugX/liblgid.so
			- [lgi]/ReleaseX/liblgi.so

Building Your App
-----------------
    Win32: Add the project [lgi]/Lgi_vc9.vxproj to your workspace. Then in your 
    new project you'll need to set these settings:
	    * C/C++ tab
			- C++ Language
			    - RTTI on.
			- Code Generation
			    - [Debug] Run time library: Debug Multithreaded DLL
			    - [Release] Run time library: Multithreaded DLL
			- Preprocessor
			    - Define: LGI_STATIC (if your using LgiStatic version)
			    - Include path: [Lgi]/include/common
			    - Include path: [Lgi]/include/win32
		* Link tab
			- Object/library modules:
				- Add 'imm32.lib' (if you use GTextView3.cpp)

    Linux/Cygwin:
    	* Add to your library string:
    		- [Debug] '-L[Lgi]/DebugX -llgid'
    		- [Release] '-L[Lgi]/ReleaseX -llgi'
    	* Add to your compile flags:
    		- '-i[Lgi]/include/common -i[Lgi]/include/linux/X'

	Mac:
		* Include the lgi.framework into your app
		* Add the these folders to your include path:
			- [Lgi]/include/common
			- [Lgi]/include/mac

Usage
-----
    In most cases just:

	#include "Lgi.h"

    But you can also include "Gdc2.h" for just graphics support without all
    the GUI stuff.

    The naming conventions for container methods are:
	- "Delete(...)" removes from the container and frees the object.
	- "Remove(...)" just removes from the container.

Documentation
-------------
    
    See: [Lgi]/docs/html/index.html.

	Which you may have to generate with doxygen if you are building from the
	repository.

