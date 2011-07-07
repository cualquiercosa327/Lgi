#ifndef __LGI_INC_H
#define __LGI_INC_H

#define LGI_EXCEPTIONS				0

#ifdef _MSC_VER
#pragma warning(disable:4250) // I do not care for this warning at all
#endif

#ifdef LGI_STATIC

	// static linking
	#define LgiFunc					extern
	#define LgiClass	
	#define LgiExtern				extern

#else

	// dynamically linked

	#if defined(WIN32) || defined(_WIN64)

		#ifdef LGI_LIBRARY
			
			#ifdef __cplusplus
			#define LgiFunc			extern "C" __declspec(dllexport)
			#else
			#define LgiFunc			__declspec(dllexport)
			#endif

			#define LgiClass		__declspec(dllexport)
			#define LgiExtern		extern __declspec(dllexport)
			#define LgiTemplate

		#else

			#ifdef __cplusplus
			#define LgiFunc			extern "C" __declspec(dllimport)
			#else
			#define LgiFunc			__declspec(dllimport)
			#endif

			#define LgiClass		__declspec(dllimport)
			#define LgiExtern		extern __declspec(dllimport)
			#define LgiTemplate		extern

		#endif

	#else // Unix like OS

		#ifdef __cplusplus
			#define LgiFunc			extern "C"
		#else
			#define LgiFunc			extern
		#endif
		
		#if __GNUC__ >= 4
			#if LGI_LIBRARY
				#define LgiClass		__attribute__((visibility("default")))
				#define LgiExtern		extern
			#else
				#define LgiClass		__attribute__((visibility("default")))
				#define LgiExtern		extern
			#endif
		#else
			#define LgiClass
			#define LgiExtern		extern
		#endif

	#endif

#endif

#endif
