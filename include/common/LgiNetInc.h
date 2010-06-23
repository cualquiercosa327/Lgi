


#ifndef __LGI_NET_INC_H
#define __LGI_NET_INC_H

#ifdef LGI_STATIC

	// static linking
	#define LgiNetFunc					extern
	#define LgiNetClass	
	#define LgiNetExtern				extern

#else

	// dynamically linked

	#ifdef _MSC_VER

		#ifdef LGI_LIBRARY
			#define LgiNetFunc			extern "C" __declspec(dllexport)
			#define LgiNetClass			__declspec(dllexport)
			#define LgiNetExtern		extern __declspec(dllexport)
		#else
			#define LgiNetFunc			extern "C" __declspec(dllimport)
			#define LgiNetClass			__declspec(dllimport)
			#define LgiNetExtern		extern __declspec(dllimport)
		#endif

	#else // Unix like OS

		#ifdef LGI_LIBRARY
			#define LgiNetFunc			extern "C"
			#define LgiNetClass	
			#define LgiNetExtern		extern
		#else
			#define LgiNetFunc			extern "C"
			#define LgiNetClass
			#define LgiNetExtern		extern
		#endif


	#endif

#endif

#endif
