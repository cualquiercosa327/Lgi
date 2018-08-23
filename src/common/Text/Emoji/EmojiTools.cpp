// http://code.iamcal.com/php/emoji/
#include <wchar.h>

#include "Lgi.h"
#include "Emoji.h"
#include "GVariant.h"
#include "GDocView.h"

#ifdef WIN32
typedef uint32 WChar;
#else
typedef wchar_t WChar;
#endif

bool HasEmoji(char *Txt)
{
	if (!Txt)
		return false;
		
	GUtf8Ptr p(Txt);
	WChar u;
	while ((u = p++))
	{
		int IcoIdx = EmojiToIconIndex((uint32*)&u, 1);
		if (IcoIdx >= 0)
			return true;
	}

	return false;
}

bool HasEmoji(uint32 *Txt)
{
	if (!Txt)
		return false;

	for (uint32 *s = Txt; *s; s++)
	{
		int IcoIdx = EmojiToIconIndex(s, 2);
		if (IcoIdx >= 0)
			return true;
	}

	return false;
}

/*
#ifdef WIN32
#define snwprintf _snwprintf
#else
#include <wchar.h>
*/
template<typename T>
ssize_t my_snwprintf(T *ptr, int ptr_size, const char16 *fmt, ...)
{
	T *start = ptr;
	T *end = ptr + ptr_size - 1;
	
	va_list ap;
	va_start(ap, fmt);
	
	// int a = 1;
	while (ptr < end && *fmt)
	{
		if (*fmt == '%')
		{
			int len = -1;
			fmt++;
			if (*fmt == '.')
				fmt++;
			if (*fmt == '*')
			{
				len = va_arg(ap, int);
				fmt++;
			}
			if (*fmt == 's')
			{
				T *in = va_arg(ap, T*);
				if (len >= 0)
					while (ptr < end && in && len-- > 0)
						*ptr++ = *in++;
				else
					while (ptr < end && in && *in)
						*ptr++ = *in++;
			}
			else if (*fmt == 'S')
			{
				char *in = va_arg(ap, char*);
				if (len >= 0)
					while (ptr < end && in && len-- > 0)
						*ptr++ = *in++;
				else
					while (ptr < end && in && *in)
						*ptr++ = *in++;
			}
			else if (*fmt == 'i')
			{
				char tmp[32];
				int n = 0;
				int in = va_arg(ap, int);
				if (in)
					while (in)
					{
						tmp[n++] = '0' + (in % 10);
						in /= 10;
					}
				else
					tmp[n++] = '0';
				while (ptr < end && n > 0)
					*ptr++ = tmp[--n];
			}
			else LgiAssert(!"Unknown format specifier");
			fmt++;
		}
		else *ptr++ = *fmt++;
	}
	
	*ptr++ = 0;
	
	va_end(ap);
	
	return ptr - start - 1;
}

// #endif

#define BUF_SIZE	256

const char16 *h1 = L"<html><body>\n";
const char16 *h2 = L"</body></html>\n";
const char16 *newline = L"<br>\n";
const char16 *mail_link = L"<a href=\'mailto:%.*s\'>%.*s</a>";
const char16 *anchor = L"<a href=\'%.*s\'>%.*s</a>";
const char16 *img = L"<img src=\'file:/%S\' style=\'x-rect:rect(%S);\'>";

struct EmojiMemQ : GMemQueue
{
	EmojiMemQ() : GMemQueue(1024)
	{
	}
	
	#ifdef WINDOWS
	int WriteWide(const char16 *s, ssize_t bytes)
	{
		GAutoPtr<uint32> c((uint32*)LgiNewConvertCp("utf-32", s, LGI_WideCharset, bytes));
		int len = Strlen(c.Get());
		return GMemQueue::Write(c, len * sizeof(uint32));
	}
	#endif
	
	ssize_t WriteWide(const WChar *s, ssize_t bytes)
	{
		return GMemQueue::Write(s, bytes);
	}
};

GAutoWString TextToEmoji(uint32 *Txt, bool IsHtml)
{
	EmojiMemQ p;
	GArray<GLinkInfo> Links;
	int Lnk = 0;
	ssize_t Ch;
	WChar Buf[BUF_SIZE];
	char EmojiPng[MAX_PATH];

	#ifdef MAC
	LgiGetExeFile(EmojiPng, sizeof(EmojiPng));
	LgiMakePath(EmojiPng, sizeof(EmojiPng), EmojiPng, "Contents/Resources/EmojiMap.png");
	#else
	LgiGetSystemPath(LSP_APP_INSTALL, EmojiPng, sizeof(EmojiPng));
	LgiMakePath(EmojiPng, sizeof(EmojiPng), EmojiPng, "resources/EmojiMap.png");
	#endif

	LgiAssert(sizeof(WChar) == sizeof(uint32));
	
	if (!IsHtml)
	{
		LgiDetectLinks(Links, Txt);
		Ch = my_snwprintf(Buf, BUF_SIZE, h1);
		if (Ch > 0)
			p.Write(Buf, (int) (Ch * sizeof(*Buf)));
	}
	
	WChar *Start = (WChar*)Txt;
	WChar *s = (WChar*)Txt;
	for (; *s; s++)
	{
		if (Lnk < (int)Links.Length() && s - (WChar*)Txt == Links[Lnk].Start)
		{
			GLinkInfo &l = Links[Lnk];

			// Start of embedded link, convert into <A>
			if (s > Start) p.Write(Start, (int) ((s - Start) * sizeof(*s)));
			if (l.Email)
				Ch = my_snwprintf(Buf, BUF_SIZE, mail_link, l.Len, s, l.Len, s);
			else
				Ch = my_snwprintf(Buf, BUF_SIZE, anchor, l.Len, s, l.Len, s);
			if (Ch > 0)
				p.Write(Buf, (int) (Ch * sizeof(*Buf)));
			Start = s + l.Len;
			Lnk++;
		}
		else if (!IsHtml && *s == '\n')
		{
			// Eol
			if (s > Start)
				p.Write(Start, (int) ((s - Start) * sizeof(*s)));
			Ch = my_snwprintf(Buf, BUF_SIZE, newline);
			if (Ch > 0)
				p.Write(Buf, (int) (Ch * sizeof(*Buf)));
			Start = s + 1;
		}
		else
		{
			int IcoIdx = EmojiToIconIndex((uint32*)s, 2);
			if (IcoIdx >= 0)
			{
				// Emoji character, convert to <IMG>
				if (s > Start)
					p.Write(Start, (int) ((s - Start) * sizeof(*s)));

				int XChar = IcoIdx % EMOJI_GROUP_X;
				int YChar = IcoIdx / EMOJI_GROUP_X;
				
				GRect rc;
				rc.ZOff(EMOJI_CELL_SIZE - 1, EMOJI_CELL_SIZE - 1);
				rc.Offset(XChar * EMOJI_CELL_SIZE, YChar * EMOJI_CELL_SIZE);
				Ch = my_snwprintf(Buf, BUF_SIZE, img, EmojiPng, rc.GetStr());
				if (Ch > 0)
					p.Write(Buf, (int) (Ch * sizeof(*Buf)));

				Start = s + 1;
				if (*Start == 0xfe0f)
				{
					s++;
					Start++;
				}
			}
		}
	}

	if (s > Start) p.Write(Start, (int) ((s - Start) * sizeof(*s)));

	if (!IsHtml)
	{
		Ch = my_snwprintf(Buf, BUF_SIZE, h2);
		if (Ch > 0)
			p.Write(Buf, (int) (Ch * sizeof(*Buf)));
	}

	GAutoPtr<WChar, true> WideVer( (WChar*)p.New(sizeof(*s)) );
	GAutoWString Final( (char16*)LgiNewConvertCp(LGI_WideCharset, WideVer, "utf-32") );
	return Final;
}
