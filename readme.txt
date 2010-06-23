	    Lightweight GUI Library
	    -----------------------

The primary aim of LGI is to abstract away the differences between
operating system's and provide a consistant API that applications
can target.

As a secondary goal the library should strive to be compact for easy
distribution with an application without bloating out the download.
Also the API is designed to be as simple as possible for the programmer
to use, without sacrificing functionality on the various platforms
supported.

The library is not intended to be shared between applications
as there is too much change in the API and object size to warrent a
shared library approach. In the future this may change if compatibility
can be improved.

Compiling LGI
-------------

    Open '[Lgi]/include/common/Lgi.h' and check through any compile time
    options there. You may want to switch things on or off to get it to
    compile.

    Win32:
    	Load Lgi/Lgi.dsp into Visual C++ and build it.
    
    Linux:
    	make -f Lgi/Makefile.linux
    		-or-
    	make -f Lgi/LgiIde/Makefile.linux (builds both Lgi and the IDE)
    
    Cygwin:
    	make -f Lgi/Makefile.win32
    		-or-
    	make -f Lgi/LgiIde/Makefile.win32 (builds both Lgi and the IDE)
    
    Mac:
    	Open Lgi.xcode in XCode and run the build command. 

	
	Add build folders so the OS can find the shared libraries:

		* For Windows add these to your path:
			- Lgi/Debug
			- Lgi/Release
			- Lgi/Gel/Debug
			- Lgi/Gel/Release 
		* For Cygwin add these to your path:
			- Lgi/DebugX
			- Lgi/ReleaseX
			- Lgi/Gel/DebugX
			- Lgi/Gel/ReleaseX 
		* On Linux, create symlinks in /usr/lib to the files:
			- Lgi/DebugX/liblgid.so
			- Lgi/ReleaseX/liblgi.so
			- Lgi/Gel/DebugX/liblgiskind.so
			- Lgi/Gel/ReleaseX/liblgiskin.so 

Building Your App
-----------------
    Win32: Add the project Lgi/Lgi.dsp to your workspace. Then in your 
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

    To utilise the built in memory debugging features, encase all C++ memory
    allocation with these macros:

	Obj *o = NEW(Obj(a, b, c)); // instead of "new Obj(a, b, c);"
	DeleteObj(o); // instead of "delete o;"

    And for arrays:

	Obj *o = NEW(Obj[10]);
	DeleteArray(o);

    The List<T> class has these built in:

	List<char> o;
	o.DeleteObjects(); // or o.DeleteArrays(); etc...

    The naming conventions for container methods are:
	- "Delete(...)" removes from the container and frees the object.
	- "Remove(...)" just removes from the container.

Documentation
-------------
    
    See: [Lgi]/docs/html/index.html.

