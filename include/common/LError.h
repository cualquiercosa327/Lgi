//
//  LCancel.h
//  Lgi
//
//  Created by Matthew Allen on 19/10/2017.
//
//

#ifndef _LERROR_H_
#define _LERROR_H_

#ifdef POSIX
#include "errno.h"
#endif

LgiFunc const char *GetErrorName(int e);

// Some cross platform error symbols (by no means a complete list)
enum LErrorCodes
{
#if defined(WINDOWS)
	LErrorNone			= ERROR_SUCCESS,
	LErrorAccessDenied	= ERROR_ACCESS_DENIED,
	LErrorNoMem			= ERROR_NOT_ENOUGH_MEMORY,
	LErrorInvalidParam	= ERROR_INVALID_PARAMETER,
	LErrorTooManyFiles	= ERROR_NO_MORE_FILES,
	LErrorFileExists	= ERROR_FILE_EXISTS,
	LErrorPathNotFound	= ERROR_PATH_NOT_FOUND,
#elif defined(POSIX)
	LErrorNone = 0,
	LErrorAccessDenied	= EACCES,
	LErrorNoMem			= ENOMEM,
	LErrorInvalidParam	= EINVAL,
	LErrorTooManyFiles	= EMFILE,
	LErrorFileExists	= EEXIST,
	LErrorPathNotFound	= ENOTDIR,
#else
	#warning "Impl me."
#endif
};

class LgiClass LError
{
	// This error code is defined by the operating system
	int Code;
	GString Msg;
	
public:
	LError(int code = 0, const char *msg = NULL)
	{
		Set(code, msg);
	}

	LError &operator =(int code)
	{
		Code = code;
		return *this;
	}

	void Set(int code, const char *msg = NULL)
	{
		Code = code;
		Msg = msg;
	}

	int GetCode() { return Code; }

	GString GetMsg()
	{
		if (!Msg)
			Msg = GetErrorName(Code);
		return Msg;
	}
};


#endif /* _LERROR_H_ */
