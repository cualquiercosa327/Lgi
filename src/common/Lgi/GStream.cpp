#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include "Lgi.h"
#include "GStream.h"

//////////////////////////////////////////////////////////////////////
int LgiPrintf(GString &Str, const char *Format, va_list &Arg)
{
	int Bytes = 0;
	
	if (Format)
	{
		#ifndef WIN32
		va_list Cpy;
		va_copy(Cpy, Arg);
		Bytes = vsnprintf(0, 0, Format, Cpy);
		#else
		Bytes = _vscprintf(Format, Arg);
		#endif

		if (Bytes > 0 && Str.Set(NULL, Bytes))
			vsprintf_s(Str.Get(), Bytes+1, Format, Arg);
	}

	return Bytes;
}

ssize_t LgiPrintf(GAutoString &Str, const char *Format, va_list &Arg)
{
	ssize_t Bytes = 0;
	
	if (Format)
	{
		#ifndef WIN32
		va_list Cpy;
		va_copy(Cpy, Arg);
		Bytes = vsnprintf(0, 0, Format, Cpy);
		#else
		Bytes = _vscprintf(Format, Arg);
		#endif

		if (Bytes > 0 && Str.Reset(new char[Bytes+1]))
			vsprintf_s(Str, Bytes+1, Format, Arg);
	}

	return Bytes;
}

ssize_t GStreamPrintf(GStreamI *Stream, int Flags, const char *Format, va_list &Arg)
{
	if (!Stream || !Format)
		return 0;

	GAutoString a;
	ssize_t Bytes = LgiPrintf(a, Format, Arg);
	if (!a || Bytes == 0)
		return 0;
	
	return Stream->Write(a, Bytes, Flags);
}

ssize_t GStreamPrint(GStreamI *s, const char *fmt, ...)
{
	va_list Arg;
	va_start(Arg, fmt);
    ssize_t Ch = GStreamPrintf(s, 0, fmt, Arg);
	va_end(Arg);
	return Ch;
}

ssize_t GStream::Print(const char *Format, ...)
{
	va_list Arg;
	va_start(Arg, Format);
    ssize_t Ch = GStreamPrintf(this, 0, Format, Arg);
	va_end(Arg);
	return Ch;
}

/////////////////////////////////////////////////////////////////
GStreamOp::GStreamOp(ssize_t BufSize)
{
	StartTime = 0;
	EndTime = 0;
	Total = 0;
	Size = MAX(BufSize, 256);
	Buf = new char[Size];
}

GStreamOp::~GStreamOp()
{
	DeleteArray(Buf);
}

ssize_t GStreamOp::GetRate()
{
	return (Total * GetElapsedTime()) / 1000;
}

ssize_t GStreamOp::GetTotal()
{
	return Total;
}

ssize_t GStreamOp::GetElapsedTime()
{
	if (EndTime)
	{
		return EndTime - StartTime;
	}
	
	return LgiCurrentTime() - StartTime;
}

////////////////////////////////////////////////////////////////
// Stateful parser that matches the start of lines to the 'prefix'
GLinePrefix::GLinePrefix(const char *p, bool eol)
{
	Start = true;
	At = 0;
	Pos = 0;
	Eol = eol;
	Prefix = NewStr(p);
}

GLinePrefix::~GLinePrefix()
{
	DeleteArray(Prefix);
}

void GLinePrefix::Reset()
{
	Start = true;
	At = 0;
	Pos = 0;
}

ssize_t GLinePrefix::IsEnd(void *v, ssize_t Len)
{
	if (Prefix)
	{
		char *s = (char*)v;
		for (int i=0; i<Len; i++, Pos++)
		{
			if (At)
			{
				if (toupper(*At) == toupper(s[i]))
				{
					At++;
					if (!*At)
					{
						// Sequence found
						return Pos + 1 - (Eol ? 0 : (int)strlen(Prefix));
					}
				}
				else At = 0;
			}
			else if (s[i] == '\n')
			{
				Start = true;
			}
			else if (Start)
			{
				if (Prefix[0] &&
					s[i] == Prefix[0])
				{
					At = Prefix + 1;
				}

				Start = false;
			}
		}
	}

	return -1;
}

ssize_t GEndOfLine::IsEnd(void *s, ssize_t Len)
{
	for (int i=0; i<Len; i++)
	{
		if (((char*)s)[i] == '\n')
		{
			return i;
		}
	}

	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
ssize_t GCopyStreamer::Copy(GStreamI *Source, GStreamI *Dest, GStreamEnd *End)
{
	if (!Source || !Dest || !Buf)
		return -1;

	int64 Bytes = 0;
	ssize_t r, w, e = -1;
	StartTime = LgiCurrentTime();

	while (e < 0)
	{
		if ((r = Source->Read(Buf, Size)) > 0)
		{
			if (End)
			{
				e = End->IsEnd(Buf, r);
			}

			if ((w = Dest->Write(Buf, e >= 0 ? MIN(e, r) : r)) > 0)
			{
				Bytes += w;
				Total += w;
			}
			else break;
		}
		else break;
	}
	
	EndTime = LgiCurrentTime();

	return Bytes;
}

