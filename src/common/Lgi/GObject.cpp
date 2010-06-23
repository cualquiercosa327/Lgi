#include "Gdc2.h"
#include "LgiCommon.h"
#include "GString.h"

//////////////////////////////////////////////////////////////////////////
GObject::GObject()
{
	_Name8 = 0;
	_Name16 = 0;
}

GObject::~GObject()
{
	DeleteArray(_Name8);
	DeleteArray(_Name16);
}

char *GObject::Name()
{
	if (!_Name8 AND _Name16)
	{
		_Name8 = LgiNewUtf16To8(_Name16);
	}

	return _Name8;
}

bool GObject::Name(char *n)
{
	if (n == _Name8) return true;

	DeleteArray(_Name8);
	DeleteArray(_Name16);
	_Name8 = NewStr(n);

	return _Name8 != 0;
}

char16 *GObject::NameW()
{
	if (!_Name16 AND _Name8)
	{
		_Name16 = LgiNewUtf8To16(_Name8);
	}

	return _Name16;
}

bool GObject::NameW(char16 *n)
{
	if (n == _Name16) return true;

	DeleteArray(_Name8);
	DeleteArray(_Name16);
	_Name16 = NewStrW(n);

	return _Name16 != 0;
}

