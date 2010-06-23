#include "Lgi.h"
#include "GUtf8.h"

/////////////////////////////////////////////////////////////////////////////
GUtf8Ptr::GUtf8Ptr(char *p)
{
	Ptr = (uint8*)p;
}

GUtf8Ptr::operator uint32()
{
	uint8 *p = Ptr;
	int l = 6;
	return LgiUtf8To32(p, l);
}

void GUtf8Ptr::Add(char16 c)
{
	int l = 6;
	LgiUtf32To8(c, Ptr, l);
}

uint32 GUtf8Ptr::operator++(const int n)
{
	if (IsUtf8_Lead(*Ptr))
	{
		Ptr++;

		while (IsUtf8_Trail(*Ptr))
		{
			Ptr++;
		}
	}
	else
	{
		LgiAssert(!IsUtf8_Trail(*Ptr));

		Ptr++;
	}

	return *this;
}

uint32 GUtf8Ptr::operator--(const int n)
{
	Ptr--;
	if (IsUtf8_Trail(*Ptr))
	{
		Ptr--;

		while (IsUtf8_Trail(*Ptr))
		{
			Ptr--;
		}

		LgiAssert(IsUtf8_Lead(*Ptr));
	}
	else
	{
		LgiAssert((*Ptr & 0x80) == 0);
	}

	return *this;
}

uint32 GUtf8Ptr::operator += (int n)
{
	while (*Ptr AND n-- > 0)
	{
		(*this)++;
	}

	return *this;
}

uint32 GUtf8Ptr::operator-=(int n)
{
	while (*Ptr AND n-- > 0)
	{
		(*this)--;
	}

	return *this;
}

int GUtf8Ptr::GetBytes()
{
	return strlen((char*)Ptr);
}

int GUtf8Ptr::GetChars()
{
	int Count = 0;
	uint8 *p = Ptr;

	while (*p)
	{
		if (IsUtf8_Lead(*p))
		{
			p++;
			while (IsUtf8_Trail(*p))
			{
				p++;
			}
		}
		else
		{
			p++;
		}

		Count++;
	}

	return Count;
}

/////////////////////////////////////////////////////////////////////////////
GUtf8Str::GUtf8Str(char *utf, int bytes, bool Copy)
{
	Own = Copy;
	Ptr = Start = Copy ? (uint8*)NewStr(utf) : (uint8*)utf;
	End = bytes >= 0 ? Start + bytes : 0;
	Cur = Start;
}

GUtf8Str::GUtf8Str(char16 *wide, int chars)
{
	Own = true;
	Start = (uint8*)LgiNewUtf16To8(wide);
	End = chars >= 0 ? Start + chars : 0;
	Cur = Start;
}

GUtf8Str::~GUtf8Str()
{
	Empty();
}

void GUtf8Str::Empty()
{
	if (Own)
	{
		DeleteArray(Start);
	}
	else Start = 0;
	Cur = (char*)0;
	End = 0;
	Own = false;
}

GUtf8Str &GUtf8Str::operator =(char *s)
{
	Empty();
	Start = (uint8*)s;
	Cur = Start;
	End = 0;
	return *this;
}

char16 *GUtf8Str::ToWide()
{
	if (End)
	{
		int Len = End - Ptr;
		if (Len > 0)
		{
			return LgiNewUtf8To16((char*)Ptr, Len);
		}
	}
	else
	{
		return LgiNewUtf8To16((char*)Ptr);
	}

	return 0;
}

bool GUtf8Str::Valid()
{
	if (!Start OR !Ptr)
		return false;

	if (Ptr < Start)
		return false;

	if (End AND Ptr > End)
		return false;

	return true;
}

bool GUtf8Str::IsStart()
{
	return !Ptr OR Ptr == Start;
}

bool GUtf8Str::IsEnd()
{
	if (End)
		return Ptr >= End;

	return !Ptr OR *Ptr == 0;
}

