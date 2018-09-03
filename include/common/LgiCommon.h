/**
	\file
	\author Matthew Allen
	\date 27/3/2000
	\brief Common LGI include
	Copyright (C) 2000-2004, <a href="mailto:fret@memecode.com">Matthew Allen</a> 
*/

/**
 * \defgroup Base Foundation tools
 * \ingroup Lgi
 */
/**
 * \defgroup Text Text handling
 * \ingroup Lgi
 */

#ifndef _LGI_COMMON_H
#define _LGI_COMMON_H

#if defined LINUX
#include <sys/time.h>
#endif

#include "GMem.h"
#include "GArray.h"
#include "LgiClass.h"
#include "GString.h"
#include "GStringClass.h"

/// Returns the system path specified
/// \ingroup Base
LgiExtern GString LgiGetSystemPath(
	/// Which path to retreive
	LgiSystemPath Which,
	int WordSize = 0
);

/// Returns the mime type of the file
/// \ingroup Mime
LgiExtern GString LgiGetFileMimeType
(
	/// File to find mime type for
	const char *File
);

/// Returns the application associated with the mime type
/// \ingroup Mime
LgiExtern GString LgiGetAppForMimeType
(
	/// Type of the file to find and app for
	const char *Mime
);

/// URL encode a string
LgiExtern GString LgiUrlEncode(const char *s, const char *delim);

/// URL decode a string
LgiExtern GString LgiUrlDecode(const char *s);

/// Gets the current user
LgiExtern GString LgiCurrentUserName();

/// Returns an environment variable.
LgiExtern GString LgiGetEnv(const char *Var);

/// Check for a valid email string
LgiExtern bool IsValidEmail(GString Email);

#ifdef __cplusplus
extern "C"
{
#endif

/////////////////////////////////////////////////////////////
// Externs

// Codepages

/// Converts a buffer of text to a different charset
/// \ingroup Text
/// \returns the bytes written to the location pointed to by 'Out'
LgiFunc ssize_t LgiBufConvertCp(void *Out, const char *OutCp, ssize_t OutLen, const void *&In, const char *InCp, ssize_t &InLen);

/// \brief Converts a string to a new charset
/// \return A dynamically allocate, null terminated string in the new charset
/// \ingroup Text
LgiFunc void *LgiNewConvertCp
(
	/// Output charset
	const char *OutCp,
	/// Input buffer
	const void *In,
	/// The input data's charset
	const char *InCp,
	/// Bytes of valid data in the input
	ssize_t InLen = -1
);

/// Return true if Lgi support the charset
/// \ingroup Text
LgiFunc bool LgiIsCpImplemented(const char *Cp);

/// Converts the ANSI code page to a charset name
/// \ingroup Text
LgiFunc const char *LgiAnsiToLgiCp(int AnsiCodePage = -1);

/// Calculate the number of characters in a string
/// \ingroup Text
LgiFunc int LgiCharLen(const void *Str, const char *Cp, int Bytes = -1);

/// Move a pointer along a utf-8 string by characters
/// \ingroup Text
LgiFunc char *LgiSeekUtf8
(
	/// Pointer to the current character
	const char *Ptr,
	/// The number of characters to move forward or back
	ssize_t D,
	/// The start of the memory buffer if you known
	char *Start = 0
);

/// Return true if the string is valid utf-8
/// \ingroup Text
LgiFunc bool LgiIsUtf8(const char *s, ssize_t len = -1);

/// Converts a string to the native 8bit charset of the OS from utf-8
/// \ingroup Text
LgiFunc char *LgiToNativeCp(const char *In, ssize_t InLen = -1);

/// Converts a string from the native 8bit charset of the OS to utf-8
/// \ingroup Text
LgiFunc char *LgiFromNativeCp(const char *In, ssize_t InLen = -1);

/// Returns the next token in a string, leaving the argument pointing to the end of the token
/// \ingroup Text
LgiFunc char *LgiTokStr(const char *&s);

/// Formats a data size into appropriate units
/// \ingroup Base
LgiFunc void LgiFormatSize
(
	/// Output string
	char *Str,
	/// Output string buffer length
	int SLen,
	/// Input size in bytes
	uint64 Size
);

/// \returns true if the path is a volume root.
LgiFunc bool LgiIsVolumeRoot(const char *Path);

/// Converts a string from URI encoding (ala %20 -> ' ')
/// \returns a dynamically allocated string or NULL on error
/// \ingroup Text
LgiFunc char *LgiDecodeUri
(
	/// The URI
	const char *uri,
	/// The length or -1 if NULL terminated
	int len = -1
);

/// Converts a string to URI encoding (ala %20 -> ' ')
/// \returns a dynamically allocated string or NULL on error
/// \ingroup Text
LgiFunc char *LgiEncodeUri
(
	/// The URI
	const char *uri,
	/// The length or -1 if NULL terminated
	int len = -1
);

// Path
#ifdef COCOA
	LgiExtern GString LgiArgsAppPath;
#endif
	
/// Gets the path and file name of the currently running executable
/// \ingroup Base
LgiFunc bool LgiGetExeFile(char *Dst, int DstSize);

/// Gets the path of the currently running executable
/// \ingroup Base
LgiFunc bool LgiGetExePath(char *Dst, int DstSize);

/// Gets the path of the temporary file directory
/// \ingroup Base
LgiFunc bool LgiGetTempPath(char *Dst, int DstSize);

/// Returns the system path specified
/// \ingroup Base
LgiFunc bool LgiGetSystemPath
(
	/// Which path to retreive
	LgiSystemPath Which,
	/// The buffer to receive the path into 
	char *Dst,
	/// The size of the receive buffer in bytes
	int DstSize
);

/// Finds a file in the applications directory or nearby
/// \ingroup Base
LgiFunc char *LgiFindFile(const char *Name);

/// Returns 0 to end search
/// \ingroup Base
typedef bool (*RecursiveFileSearch_Callback)(void *UserData, char *Path, class GDirectory *Dir);

/// \brief Recursively search for files
/// \return Non zero if something was found
/// \ingroup Base
LgiFunc bool LgiRecursiveFileSearch
(
	/// Start search in this dir
	const char *Root,
	/// Extensions to match
	GArray<const char*> *Ext = NULL,
	/// [optional] Output filenames
	GArray<char*> *Files = NULL,
	/// [optional] Output total size
	uint64 *Size = NULL,
	/// [optional] File count
	uint64 *Count = NULL,
	/// [optional] Callback for match
	RecursiveFileSearch_Callback Callback = NULL,
	/// [options] Callback user data
	void *UserData = NULL
);

// Resources

/// Gets the currently selected language
/// \ingroup Resources
LgiFunc struct GLanguage *LgiGetLanguageId();

// Os version functions

/// Gets the current operating system and optionally it's version.
/// \returns One of the defines starting with #LGI_OS_UNKNOWN in LgiDefs.h
/// \ingroup Base
LgiFunc int LgiGetOs(GArray<int> *Ver = 0);

/// Gets the current operation systems name.
/// \ingroup Base
LgiFunc const char *LgiGetOsName();

// System

/// \brief Opens a file or directory.
///
/// If the input is an executable then it is run. If the input file
/// is a document then an appropriate application is found to open the
/// file and the file is passed to that application. If the input is
/// a directory then the OS's file manager is openned to browse the
/// directory.
///
/// \ingroup Base
LgiFunc bool LgiExecute
(
	/// The file to open
	const char *File,
	/// The arguments to pass to the program
	const char *Arguments="",
	/// The directory to run in
	const char *Dir = 0,
	/// An error message
	GAutoString *ErrorMsg = NULL
);

/// Initializes the random number generator
/// \ingroup Base
LgiFunc void LgiRandomize(uint Seed);

/// Returns a random number between 0 and Max-1
/// \ingroup Base
LgiFunc uint LgiRand(uint Max = 0);

LgiFunc bool _lgi_read_colour_config(const char *Tag, uint32 *c);

/// Plays a sound
/// \ingroup Base
LgiFunc bool LgiPlaySound
(
	/// File name of the sound to play
	const char *FileName,
	/// 0 or SND_ASYNC. If 0 the function blocks till the sound finishes.
	int Flags
);


/**
 * \defgroup Mime Mime handling support.
 * \ingroup Lgi
 */

/// Returns the file extensions associated with the mimetype
/// \ingroup Mime
LgiExtern bool LgiGetMimeTypeExtensions
(
	/// The returned mime type
	const char *Mime,
	/// The extensions
	GArray<GString> &Ext
);

inline bool LgiGetFileMimeType(const char *File, char *MimeType, int BufSize)
{
	GString p = LgiGetFileMimeType(File);
	if (MimeType && p) strcpy_s(MimeType, BufSize, p);
	return p.Length() > 0;
}

inline bool LgiGetAppForMimeType(const char *Mime, char *AppPath, int BufSize)
{
	GString p = LgiGetAppForMimeType(Mime);
	if (AppPath && p) strcpy_s(AppPath, BufSize, p);
	return p.Length() > 0;
}

/// Returns the all applications that can open a given mime type.
/// \ingroup Mime
LgiFunc bool LgiGetAppsForMimeType
(
	/// The type of files to match apps to.
	///
	/// Two special cases exist:
	/// - application/email gets the default email client
	/// - application/browser get the default web browser
	const char *Mime,
	/// The applications that can handle the 
	GArray<GAppInfo*> &Apps,
	/// Limit the length of the results, i.e. stop looking after 'Limit' matches.
	/// -1 means return all matches.
	int Limit = -1
);

/// Gets the current clock in milli-seconds. (1,000th of a second)
/// \ingroup Time
LgiFunc uint64 LgiCurrentTime();

/// Get the current clock in micro-seconds (1,000,000th of a second)
LgiFunc uint64 LgiMicroTime();

// Debug

/// Returns true if the build is for release.
/// \ingroup Base
LgiFunc int LgiIsReleaseBuild();

#if defined WIN32

/// Registers an active x control
LgiFunc bool RegisterActiveXControl(const char *Dll);

enum HWBRK_TYPE
{
	HWBRK_TYPE_CODE,
	HWBRK_TYPE_READWRITE,
	HWBRK_TYPE_WRITE,
};

enum HWBRK_SIZE
{
	HWBRK_SIZE_1,
	HWBRK_SIZE_2,
	HWBRK_SIZE_4,
	HWBRK_SIZE_8,
};

/// Set a hardware breakpoint.
LgiFunc HANDLE SetHardwareBreakpoint
(
	/// Use GetCurrentThread()
	HANDLE hThread,
	/// Type of breakpoint
	HWBRK_TYPE Type,
	/// Size of breakpoint
	HWBRK_SIZE Size,
	/// The pointer to the data to break on
	void *s
);

/// Deletes a hardware breakpoint
LgiFunc bool RemoveHardwareBreakpoint(HANDLE hBrk);

#elif defined LINUX

/// Window managers
enum WindowManager
{
	WM_Unknown,
	WM_Kde,
	WM_Gnome
};

/// Returns the currently running window manager
WindowManager LgiGetWindowManager();

#endif

#ifdef __cplusplus
}
#endif

#endif
