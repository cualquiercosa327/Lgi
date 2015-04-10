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
			vsprintf(Str.Get(), Format, Arg);
	}

	return Bytes;
}

int LgiPrintf(GAutoString &Str, const char *Format, va_list &Arg)
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

		if (Bytes > 0 && Str.Reset(new char[Bytes+1]))
			vsprintf(Str, Format, Arg);
	}

	return Bytes;
}

int GStreamPrintf(GStreamI *Stream, int Flags, const char *Format, va_list &Arg)
{
	if (!Stream || !Format)
		return 0;

	GAutoString a;
	int Bytes = LgiPrintf(a, Format, Arg);
	if (!a || Bytes == 0)
		return 0;
	
	return Stream->Write(a, Bytes, Flags);
}

int GStreamPrint(GStreamI *s, const char *fmt, ...)
{
	va_list Arg;
	va_start(Arg, fmt);
    int Ch = GStreamPrintf(s, 0, fmt, Arg);
	va_end(Arg);
	return Ch;
}

int GStream::Print(const char *Format, ...)
{
	va_list Arg;
	va_start(Arg, Format);
    int Ch = GStreamPrintf(this, 0, Format, Arg);
	va_end(Arg);
	return Ch;
}

/////////////////////////////////////////////////////////////////
GStreamer::GStreamer(int BufSize)
{
	StartTime = 0;
	EndTime = 0;
	Total = 0;
	Size = min(BufSize, 256);
	Buf = new char[Size];
}

GStreamer::~GStreamer()
{
	DeleteArray(Buf);
}

int GStreamer::GetRate()
{
	return (Total * GetElapsedTime()) / 1000;
}

int GStreamer::GetTotal()
{
	return Total;
}

int GStreamer::GetElapsedTime()
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

int GLinePrefix::IsEnd(void *v, int Len)
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
						return Pos + 1 - (Eol ? 0 : strlen(Prefix));
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

int GEndOfLine::IsEnd(void *s, int Len)
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
int64 GCopyStreamer::Copy(GStreamI *Source, GStreamI *Dest, GStreamEnd *End)
{
	if (!Source || !Dest || !Buf)
		return -1;

	int64 Bytes = 0;
	int r, w, e = -1;
	StartTime = LgiCurrentTime();

	while (e < 0)
	{
		if ((r = Source->Read(Buf, Size)) > 0)
		{
			if (End)
			{
				e = End->IsEnd(Buf, r);
			}

			if ((w = Dest->Write(Buf, e >= 0 ? min(e, r) : r)) > 0)
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

