#include "Gdc2.h"
#include "LgiCommon.h"
#include "GString.h"

//////////////////////////////////////////////////////////////////////////
GBase::GBase()
{
	_Name8 = 0;
	_Name16 = 0;
}

GBase::~GBase()
{
	DeleteArray(_Name8);
	DeleteArray(_Name16);
}

char *GBase::Name()
{
	if (!_Name8 && _Name16)
	{
		_Name8 = LgiNewUtf16To8(_Name16);
	}

	return _Name8;
}

bool GBase::Name(const char *n)
{
	if (n == _Name8) return true;

	DeleteArray(_Name8);
	DeleteArray(_Name16);
	_Name8 = NewStr(n);

	return _Name8 != 0;
}

char16 *GBase::NameW()
{
	if (!_Name16 && _Name8)
	{
		_Name16 = LgiNewUtf8To16(_Name8);
	}

	return _Name16;
}

bool GBase::NameW(const char16 *n)
{
	if (n == _Name16) return true;

	DeleteArray(_Name8);
	DeleteArray(_Name16);
	_Name16 = NewStrW(n);

	return _Name16 != 0;
}

