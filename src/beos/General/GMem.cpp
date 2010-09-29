/*hdr
**      FILE:           Memory.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           10/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "GMem.h"

#define ToUpper(c) ( ( (c) >= 'a' AND (c) <= 'z') ? (c)+('A'-'a') : (c) )
#define ToLower(c) (((c) >= 'A' AND (c) <= 'Z') ? (c)+('a'-'A') : (c))

#ifdef MEMORY_DEBUG
void *operator new(size_t Size, char *file, int line)
{
	long *Ptr = (long*) malloc(Size + 4);
	if (Ptr)
	{
		memset(Ptr+1, 0xCC, Size);
		*Ptr = Size;
	}
	return Ptr + 1;
}

void *operator new[](size_t Size, char *file, int line)
{
	long *Ptr = (long*) malloc(Size + 4);
	if (Ptr)
	{
		memset(Ptr+1, 0xCC, Size);
		*Ptr = Size;
	}
	return Ptr + 1;
}

void operator delete(void *p)
{
	long *Ptr = (long*) p;
	Ptr--;
	memset(Ptr+1, 0xCC, *Ptr);
	free(Ptr);
}
#endif

void MemorySizeToStr(char *Str, uint Size)
{
	#define K				1024
	#define M				(K*K)
	#define G				(M*K)

	if (Size >= G)
	{
		sprintf(Str, "%.2f G", (double) Size / G);
	}
	else if (Size >= M)
	{
		sprintf(Str, "%.2f M", (double) Size / M);
	}
	else if (Size >= (10 * K))
	{
		sprintf(Str, "%.2f K", (double) Size / K);
	}
	else
	{
		sprintf(Str, "%ld bytes", Size);
	}

	#undef K
	#undef M
	#undef G
}

// String class
/*
String::String()
{
	Length = 0;
	Alloc = 0;
	Data = NULL;
}

String::String(char *Src)
{
	Length = (Src) ? strlen(Src) : 0;
	Alloc = Length;
	Data = new char[Alloc];
	if (Data AND Src)
	{
		strcpy(Data, Src);
	}
}

String::~String()
{
	Empty();
}

bool String::SetLength(int Len)
{
	bool Status = FALSE;

	if (Len > Alloc - 1)
	{
		int NewAlloc = Len + 32;
		char *Temp = new char[NewAlloc];
		if (Temp)
		{
			if (Data)
			{
				memcpy(Temp, Data, min(Length, Len));
			}

			Length = Len;
			Alloc = NewAlloc;
			DeleteArray(Data);
			Data = Temp;
			Data[Length] = 0;
			Status = TRUE;
		}
	}
	else
	{
		Status = TRUE;
		Length = Len;
		if (Length > 0)
		{
			Data[Length] = 0;
		}
	}

	return Status;
}

int String::GetLength()
{
	return Length;
}

bool String::IsEmpty()
{
	return Length == 0;
}

void String::Empty()
{
	Length = 0;
	Alloc = 0;
	DeleteArray(Data);
}

char String::operator[](int i)
{
	return (Data AND i >= 0 AND i < Length) ? Data[i] : 0;
}

String::operator char *() const
{
	return Data;
}

String& String::operator =(String &Str)
{
	Empty();
	SetLength(Str.Length);
	if (Length > 0)
	{
		strncpy(Data, Str.Data, Length);
	}

	return *this;
}

String& String::operator =(char ch)
{
	Empty();
	SetLength(2);
	if (Length > 1)
	{
		Data[0] = ch;
		Data[1] = 0;
	}

	return *this;
}

String& String::operator =(char *Str)
{
	if (Str)
	{
		SetLength(strlen(Str));
		if (Length > 0)
		{
			strncpy(Data, Str, Length);
			Data[Length] = 0;
		}
	}
	
	return *this;
}

String& String::operator +=(String &Str)
{
	if (Str.Length > 0)
	{
		int Pos = Length;

		SetLength(Length + Str.Length);
		if (Length > 0)
		{
			strncpy(Data + Pos, Str.Data, Length - Pos);
			Data[Length] = 0;
		}
	}
	return *this;
}

String& String::operator +=(char ch)
{
	int Pos = Length;

	SetLength(Length + 1);
	if (Length > 0)
	{
		Data[Length-1] = ch;
		Data[Length] = 0;
	}

	return *this;
}

String& String::operator +=(char *Str)
{
	if (Str)
	{
		int Pos = Length;

		SetLength(Length + strlen(Str));
		if (Length > 0)
		{
			strncpy(Data + Pos, Str, Length - Pos);
			Data[Length] = 0;
		}
	}
	return *this;
}

int String::Compare(char *Str)
{
	return (Str AND Data) ? strcmp(Str, Data) : -1;
}

int String::CompareNoCase(char *Str)
{
	return (Str AND Data) ? stricmp(Str, Data) : -1;
}

String String::Mid(int nFirst, int nCount)
{
	String Temp;

	nFirst = limit(nFirst, 0, Length);
	if (nCount < 0) nCount = Length - nFirst;
	if (nCount + nFirst > Length) nCount = Length - nFirst;
	Temp.SetLength(nCount);
	if (Temp.Length > 0)
	{
		strncpy(Temp.Data, Data + nCount, Temp.Length);
		Temp.Data[Temp.Length] = 0;
	}

	return Temp;
}

String String::Left(int nCount)
{
	return Mid(0, nCount);
}

String String::Right(int nCount)
{
	return Mid(Length - nCount, nCount);
}

void String::MakeUpper()
{
	if (Length > 0)
	{
		for (int i=0; i<Length; i++)
		{
			if (Data[i] >= 'a' AND Data[i] <= 'z')
			{
				Data[i] += (int) 'A' - 'a';
			}
		}
	}
}

void String::MakeLower()
{
	if (Length > 0)
	{
		for (int i=0; i<Length; i++)
		{
			if (Data[i] >= 'A' AND Data[i] <= 'Z')
			{
				Data[i] += (int) 'a' - 'A';
			}
		}
	}
}

void String::TrimRight()
{
	if (Length > 0)
	{
		char White[] = " \t\r\n";
		int i;

		for (i=0; i<Length AND strchr(White, Data[Length-i-1]); i++) {}

		if (i > 0)
		{
			SetLength(Length - i);
		}
	}
}

void String::TrimLeft()
{
	if (Length > 0)
	{
		char White[] = " \t\r\n";
		int i;

		for (i=0; i<Length AND strchr(White, Data[i]); i++) {}

		if (i > 0)
		{
			memmove(Data, Data + i, Length - i);
			SetLength(Length - i);
		}
	}
}

int String::Find(char ch)
{
	if (Length > 0)
	{
		for (int i=0; i<Length; i++)
		{
			if (Data[i] == ch)
			{
				return i;
			}
		}
	}

	return -1;
}

int String::ReverseFind(char ch)
{
	if (Length > 0)
	{
		for (int i=Length-1; i>0; i--)
		{
			if (Data[i] == ch)
			{
				return i;
			}
		}
	}

	return -1;
}

int String::Find(char *Sub)
{
	if (Length > 0 AND Sub)
	{
		char *p = strstr(Data, Sub);
		if (p)
		{
			return p - Data;
		}
	}

	return -1;
}

void String::Format(char *Format, ...)
{
	if (Format)
	{
		char *Buffer = new char[2048];
		if (Buffer)
		{
			va_list Arg;

			va_start(Arg, Format);
			vsprintf(Buffer, Format, Arg);
			va_end(Arg);

			SetLength(strlen(Buffer));
			if (Length > 0)
			{
				strncpy(Data, Buffer, Length);
				Data[Length] = 0;
			}

			DeleteArray(Buffer);
		}
	}
}

bool operator ==(String &s1, String &s2)
{
	return strcmp(s1, s2) == 0;
}

bool operator ==(String &s1, char *s2)
{
	return strcmp(s1, s2) == 0;
}

bool operator ==(char *s1, String &s2)
{
	return strcmp(s1, s2) == 0;
}

bool operator !=(String &s1, String &s2)
{
	return strcmp(s1, s2) != 0;
}

bool operator !=(String &s1, char *s2)
{
	return strcmp(s1, s2) != 0;
}

bool operator !=(char *s1, String &s2)
{
	return strcmp(s1, s2) != 0;
}

bool operator <(String& s1, String &s2)
{
	return strcmp(s1, s2) < 0;
}

bool operator <(String &s1, char *s2)
{
	return strcmp(s1, s2) < 0;
}

bool operator <(char *s1, String &s2)
{
	return strcmp(s1, s2) < 0;
}

bool operator >(String &s1, String &s2)
{
	return strcmp(s1, s2) > 0;
}

bool operator >(String &s1, char *s2)
{
	return strcmp(s1, s2) > 0;
}

bool operator >(char *s1, String &s2)
{
	return strcmp(s1, s2) > 0;
}

bool operator <=(String &s1, String &s2)
{
	return strcmp(s1, s2) <= 0;
}

bool operator <=(String &s1, char *s2)
{
	return strcmp(s1, s2) <= 0;
}

bool operator <=(char *s1, String &s2)
{
	return strcmp(s1, s2) <= 0;
}

bool operator >=(String &s1, String &s2)
{
	return strcmp(s1, s2) >= 0;
}

bool operator >=(String &s1, char *s2)
{
	return strcmp(s1, s2) >= 0;
}

bool operator >=(char *s1, String &s2)
{
	return strcmp(s1, s2) >= 0;
}

String operator +(String &s1, String &s2)
{
	String Temp(s1);
	Temp += s2;
	return Temp;
}

String operator +(String &s1, char ch)
{
	String Temp(s1);
	Temp += ch;
	return Temp;
}

String operator +(char ch, String &s1)
{
	String Temp;
	Temp += ch;
	Temp += s1;
	return Temp;
}

String operator +(String &s1, char *s2)
{
	String Temp(s1);
	Temp += s2;
	return Temp;
}

String operator +(char *s1, String &s2)
{
	String Temp(s1);
	Temp += s2;
	return Temp;
}
*/
