#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include "Lgi.h"
#include "GStream.h"

#if defined(WIN32)
p_vscprintf lgi_vscprintf = 0;
#endif

//////////////////////////////////////////////////////////////////////
int GStreamPrintf(GStreamI *Stream, int Flags, const char *Format, va_list &Arg)
{
	int Chars = 0;

	if (Stream)
	{
		#ifndef WIN32

		int Size = vsnprintf(0, 0, Format, Arg);
		if (Size > 0)
		{
			GAutoString Buffer(new char[Size+1]);
			if (Buffer)
			{
				vsprintf(Buffer, Format, Arg);
				Stream->Write(Buffer, Size, Flags);
			}
		}

		#else

		if (lgi_vscprintf)
		{
			// WinXP and up...
			Chars = lgi_vscprintf(Format, Arg);
			if (Chars > 0)
			{
				GAutoString Buf(new char[Chars+1]);
				if (Buf)
				{
					vsprintf(Buf, Format, Arg);
					Stream->Write(Buf, Chars, Flags);
				}
			}
		}
		else
		{
			// Win2k or earlier, no _vscprintf for us...
			// so we'll just have to bump the buffer size
			// until it fits;
			for (int Size = 256; Size <= (256 << 10); Size <<= 2)
			{
				GAutoString Buf(new char[Size]);
				if (!Buf)
					break;

				int c = _vsnprintf(Buf, Size, Format, Arg);
				if (c > 0)
				{
					Stream->Write(Buf, Chars = c, Flags);
					break;
				}
			}			
		}
		#endif
	}

	return Chars;
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
int GCopyStreamer::Copy(GStreamI *Source, GStreamI *Dest, GStreamEnd *End)
{
	int Bytes = 0;
	int r, w, e = -1;

	if (Source && Dest)
	{
		char Buf[4 << 10];
		while (e < 0)
		{
			if ((r = Source->Read(Buf, sizeof(Buf))) > 0)
			{
				if (End)
				{
					e = End->IsEnd(Buf, r);
				}

				if ((w = Dest->Write(Buf, e >= 0 ? min(e, r) : r)) > 0)
				{
					Bytes += w;
				}
				else break;
			}
			else break;
		}
	}

	return Bytes;
}

