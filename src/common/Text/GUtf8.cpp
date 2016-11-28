#include "Lgi.h"
#include "GUtf8.h"

/////////////////////////////////////////////////////////////////////////////
GUtf8Ptr::GUtf8Ptr(void *p)
{
	Ptr = (uint8*)p;
}

GUtf8Ptr::operator int32()
{
	uint8 *p = Ptr;
	int l = 6;
	return LgiUtf8To32(p, l);
}

void GUtf8Ptr::Add(wchar_t c)
{
	int l = 6;
	LgiUtf32To8(c, Ptr, l);
}

int32 GUtf8Ptr::operator++(const int n)
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
	    if (IsUtf8_Trail(*Ptr))
	    {
	        static bool Warn = true;
	        if (Warn)
	        {
	            Warn = false;
		        LgiAssert(!"Invalid UTF");
		    }
		}

		Ptr++;
	}

	return *this;
}

int32 GUtf8Ptr::operator--(const int n)
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

int32 GUtf8Ptr::operator += (int n)
{
	while (*Ptr && n-- > 0)
	{
		(*this)++;
	}

	return *this;
}

int32 GUtf8Ptr::operator-=(int n)
{
	while (*Ptr && n-- > 0)
	{
		(*this)--;
	}

	return *this;
}

int GUtf8Ptr::GetBytes()
{
	return (int)strlen((char*)Ptr);
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

GUtf8Str::GUtf8Str(wchar_t *wide, int chars)
{
	Own = true;
	Start = (uint8*)WideToUtf8(wide);
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

wchar_t *GUtf8Str::ToWide()
{
	if (End)
	{
		size_t Len = End - Ptr;
		if (Len > 0)
		{
			return Utf8ToWide((char*)Ptr, (int)Len);
		}
	}
	else
	{
		return Utf8ToWide((char*)Ptr);
	}

	return 0;
}

bool GUtf8Str::Valid()
{
	if (!Start || !Ptr)
		return false;

	if (Ptr < Start)
		return false;

	if (End && Ptr > End)
		return false;

	return true;
}

bool GUtf8Str::IsStart()
{
	return !Ptr || Ptr == Start;
}

bool GUtf8Str::IsEnd()
{
	if (End)
		return Ptr >= End;

	return !Ptr || *Ptr == 0;
}

