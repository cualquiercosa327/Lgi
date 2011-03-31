/**
	\file
	\author Matthew Allen
	\date 27/3/2000
	\brief Common LGI include
	Copyright (C) 2000-2004, <a href="mailto:fret@memecode.com">Matthew Allen</a> 
*/

#ifndef _LGI_COMMON_H
#define _LGI_COMMON_H

#if defined LINUX
#include <sys/time.h>
#endif

#include "GArray.h"
#include "LgiClass.h"

#ifdef __cplusplus
extern "C"
{
#endif

/////////////////////////////////////////////////////////////
// Externs

// Codepages

/// Converts a buffer of text to a different charset
LgiFunc int LgiBufConvertCp(void *Out, char *OutCp, int OutLen, void *&In, char *InCp, int &InLen);
/// \brief Converts a string to a new charset
/// \return A dynamically allocate, null terminated string in the new charset
LgiFunc void *LgiNewConvertCp
(
	/// Output charset
	char *OutCp,
	/// Input buffer
	void *In,
	/// The input data's charset
	char *InCp,
	/// Bytes of valid data in the input
	int InLen = -1
);
/// Converts a utf-8 string into a wide character string
LgiFunc char16 *LgiNewUtf8To16(char *In, int InLen = -1);
/// Converts a wide character string into a utf-8 string
LgiFunc char *LgiNewUtf16To8
(
	/// Input string
	char16 *In,
	/// Number of bytes in the input or -1 for NULL terminated
	int InLen = -1
);
/// Return true if Lgi support the charset
LgiFunc bool LgiIsCpImplemented(char *Cp);
/// Converts the ANSI code page to a charset name
LgiFunc char *LgiAnsiToLgiCp(int AnsiCodePage = -1);
/// Calculate the byte length of a string
LgiFunc int LgiByteLen(void *Str, char *Cp);
/// Calculate the number of characters in a string
LgiFunc int LgiCharLen(void *Str, char *Cp, int Bytes = -1);
/// Move a pointer along a utf-8 string by characters
LgiFunc char *LgiSeekUtf8
(
	/// Pointer to the current character
	char *Ptr,
	/// The number of characters to move forward or back
	int D,
	/// The start of the memory buffer if you known
	char *Start = 0
);
/// Return true if the string is valid utf-8
LgiFunc bool LgiIsUtf8(char *s, int len = -1);
/// Converts a string to the native 8bit charset of the OS from utf-8
LgiFunc char *LgiToNativeCp(char *In, int InLen = -1);
/// Converts a string from the native 8bit charset of the OS to utf-8
LgiFunc char *LgiFromNativeCp(char *In, int InLen = -1);
/// Returns the next token in a string, leaving the argument pointing to the end of the token
LgiFunc char *LgiTokStr(const char *&s);
/// Formats a data size into appropriate units
LgiFunc void LgiFormatSize
(
	/// Output string
	char *Str,
	/// Input size in bytes
	uint64 Size
);
/// Converts a string from URI encoding (ala %20 -> ' ')
/// \returns a dynamically allocated string or NULL on error
LgiFunc char *LgiDecodeUri
(
	/// The URI
	char *uri,
	/// The length or -1 if NULL terminated
	int len = -1
);
/// Converts a string to URI encoding (ala %20 -> ' ')
/// \returns a dynamically allocated string or NULL on error
LgiFunc char *LgiEncodeUri
(
	/// The URI
	char *uri,
	/// The length or -1 if NULL terminated
	int len = -1
);

// Path

/// Gets the path and file name of the currently running executable
LgiFunc bool LgiGetExeFile(char *Dst, int DstSize);
/// Gets the path of the currently running executable
LgiFunc bool LgiGetExePath(char *Dst, int DstSize);
/// Gets the path of the temporary file directory
LgiFunc bool LgiGetTempPath(char *Dst, int DstSize);

/// Returns the system path specified
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
LgiFunc char *LgiFindFile(char *Name);

/// Returns 0 to end search
typedef bool (*RecursiveFileSearch_Callback)(void *UserData, char *Path, class GDirectory *Dir);
/// \brief Recursively search for files
/// \return Non zero if something was found
LgiFunc bool LgiRecursiveFileSearch
(
	/// Start search in this dir
	char *Root,
	/// Extensions to match
	GArray<char*> *Ext = 0,
	/// [optional] Output filenames
	GArray<char*> *Files = 0,
	/// [optional] Output total size
	uint64 *Size = 0,
	/// [optional] File count
	uint64 *Count = 0,
	/// [optional] Callback for match
	RecursiveFileSearch_Callback Callback = 0,
	/// [options] Callback user data
	void *UserData = 0
);

// Resources

/// Gets the currently selected language
LgiFunc struct GLanguage *LgiGetLanguageId();
/// Loads a string from the resource file
LgiFunc char *LgiLoadString(int Res, char *Default = 0);

// Os version functions

/// Gets the current operating system and optionally it's version.
/// \returns One of the defines starting with #LGI_OS_UNKNOWN in LgiDefs.h
LgiFunc int LgiGetOs(GArray<int> *Ver = 0);
/// Gets the current operation systems name.
LgiFunc const char *LgiGetOsName();

// System

/// \brief Opens a file or directory.
///
/// If the input is an executable then it is run. If the input file
/// is a document then an appropriate application is found to open the
/// file and the file is passed to that application. If the input is
/// a directory then the OS's file manager is openned to browse the
/// directory.
LgiFunc bool LgiExecute
(
	/// The file to open
	const char *File,
	/// The arguments to pass to the program
	const char *Arguments="",
	/// The directory to run in
	const char *Dir=0
);

/// Initializes the random number generator
LgiFunc void LgiRandomize(uint Seed);

/// Returns a random number between 0 and Max-1
LgiFunc uint LgiRand(uint Max = 0);

LgiFunc bool _lgi_read_colour_config(char *Tag, uint32 *c);

/// Plays a sound
LgiFunc bool LgiPlaySound
(
	/// File name of the sound to play
	char *FileName,
	/// 0 or SND_ASYNC. If 0 the function blocks till the sound finishes.
	int Flags
);

/// Returns the mime type of the file
LgiFunc bool LgiGetMimeTypeExtensions
(
	/// The returned mime type
	char *Mime,
	/// The extensions
	GArray<char*> &Ext
);

/// Returns the mime type of the file
LgiFunc bool LgiGetFileMimeType
(
	/// File to file type of
	char *File,
	/// Pointer to buffer to receive mime-type
	char *MimeType,
	/// Buffer length
	int BufLen
);

/// Returns the application associated with the mime type
LgiFunc bool LgiGetAppForMimeType
(
	/// Type of the file to find and app for
	char *Mime,
	/// Path to the executable of the app that can handle the file type.
	char *AppPath,
	/// Size of the 'AppPath' buffer
	int BufSize
);

/// Returns the all applications that can open a given mime type.
LgiFunc bool LgiGetAppsForMimeType
(
	/// The type of files to match apps to.
	///
	/// Two special cases exist:
	/// - application/email gets the default email client
	/// - application/browser get the default web browser
	char *Mime,
	/// The applications that can handle the 
	GArray<GAppInfo*> &Apps,
	/// Limit the length of the results, i.e. stop looking after 'Limit' matches.
	/// -1 means return all matches.
	int Limit = -1
);

/// Gets the current clock in milli-seconds.
LgiFunc uint64 LgiCurrentTime();

// Debug

/// Returns true if the build is for release.
LgiFunc int LgiIsReleaseBuild();

#if defined WIN32

/// Registers an active x control
LgiFunc bool RegisterActiveXControl(char *Dll);

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
