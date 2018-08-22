/*hdr
**      FILE:           Mail.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           28/5/98
**      DESCRIPTION:    Mail app
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "Lgi.h"
#include "Mail.h"
#include "GToken.h"
#include "Base64.h"
#include "INetTools.h"
#include "LDateTime.h"
#include "GDocView.h"
#include "Store3Defs.h"
#include "LgiRes.h"

const char *sTextPlain = "text/plain";
const char *sTextHtml = "text/html";
const char *sTextXml = "text/xml";
const char *sApplicationInternetExplorer = "application/internet-explorer";
const char sMultipartMixed[] = "multipart/mixed";
const char sMultipartEncrypted[] = "multipart/encrypted";
const char sMultipartSigned[] = "multipart/signed";
const char sMultipartAlternative[] = "multipart/alternative";
const char sMultipartRelated[] = "multipart/related";
const char sAppOctetStream[] = "application/octet-stream";

//////////////////////////////////////////////////////////////////////////////////////////////////
LogEntry::LogEntry(GColour col)
{
	c = col;
}

bool LogEntry::Add(const char *t, ssize_t len)
{
	if (!t)
		return false;

	if (len < 0)
		len = strlen(t);

	/*
	// Strip off any whitespace on the end of the line.
	while (len > 0 && strchr(" \t\r\n", t[len-1]))
		len--;
	*/

	GAutoWString w(Utf8ToWide(t, len));
	if (!w)
		return false;

	size_t ch = StrlenW(w);
	return Txt.Add(w, ch);	
}
	
//////////////////////////////////////////////////////////////////////////////////////////////////
// return true if there are any characters with the 0x80 bit set
bool Is8Bit(char *Text)
{
	if (!Text)
		return false;

	while (*Text)
	{
		if (*Text & 0x80)
			return true;
		Text++;
	}

	return false;
}

// returns the maximum length of the lines contained in the string
int MaxLineLen(char *Text)
{
	if (!Text)
		return false;

	int Max = 0;
	int i = 0;
	for (char *c = Text; *c; c++)
	{
		if (*c == '\r')
		{
			// return
		}
		else if (*c == '\n')
		{
			// eol
			Max = MAX(i, Max);
			i = 0;
		}
		else
		{
			// normal char
			i++;
		}
	}

	return Max;
}

bool IsDotLined(char *Text)
{
	if (Text)
	{
		for (char *l = Text; l && *l; )
		{
			if (l[0] == '.')
			{
				if (l[1] == '\n' || l[1] == 0)
				{
					return true;
				}
			}

			l = strchr(l, '\n');
			if (l) l++;
		}
	}

	return false;
}

char ConvHexToBin(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c + 10 - 'a';
	if (c >= 'A' && c <= 'F')
		return c + 10 - 'A';
	return 0;
}

// Is s a valid non-whitespace string?
bool ValidNonWSStr(const char *s)
{
	if (s && *s)
	{
		while (*s && strchr(" \r\t\n", *s))
		{
			s++;
		}

		if (*s)
		{
			return true;
		}
	}

	return false;
}

void TokeniseStrList(char *Str, List<char> &Output, const char *Delim)
{
	if (Str && Delim)
	{
		char *s = Str;
		while (*s)
		{
		    while (*s && strchr(WhiteSpace, *s))
		        s++;

			char *e = s;
			for (; *e; e++)
			{
				if (strchr("\'\"", *e))
				{
					// handle string constant
					char delim = *e++;
					e = strchr(e, delim);
				}
				else if (*e == '<')
				{
					e = strchr(e, '>');
				}
				else
				{
					while (*e && *e != '<' && !IsWhiteSpace(*e) && !strchr(Delim, *e))
						e++;
				}

				if (!e || !*e || strchr(Delim, *e))
				{
					break;
				}
			}

			ssize_t Len = e ? e - s : strlen(s);
			if (Len > 0)
			{
				char *Temp = new char[Len+1];
				if (Temp)
				{
					memcpy(Temp, s, Len);
					Temp[Len] = 0;
					Output.Insert(Temp);
				}
			}

			if (e)
			{
				s = e;
				for (; *s && strchr(Delim, *s); s++);
			}
			else break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
char *DecodeBase64Str(char *Str, int Len)
{
	if (Str)
	{
		ssize_t B64Len = (Len < 0) ? strlen(Str) : Len;
		ssize_t BinLen = BufferLen_64ToBin(B64Len);
		char *s = new char[BinLen+1];
		if (s)
		{
			ssize_t Converted = ConvertBase64ToBinary((uchar*)s, BinLen, Str, B64Len);
			s[Converted] = 0;
			DeleteArray(Str);
			Str = s;
		}
	}
	return Str;
}

char *DecodeQuotedPrintableStr(char *Str, ssize_t Len)
{
	if (Str)
	{
		if (Len < 0) Len = strlen(Str);
		uchar *s = new uchar[Len+1];
		if (s)
		{
			char *Out = (char*) s;
			char *Text = Str;

			for (int i=0; i<Len; i++)
			{
				if (Text[i] == '=')
				{
					if (Text[i+1] == '\r' || Text[i+1] == '\n')
					{
						if (Text[i+1] == '\r') i++;
						if (Text[i+1] == '\n') i++;
					}
					else
					{
						i++;
						char c = ConvHexToBin(Text[i]) << 4;
						i++;
						c |= ConvHexToBin(Text[i]) & 0xF;

						*Out++ = c;
					}
				}
				else
				{
					*Out++ = Text[i];
				}
			}

			*Out++ = 0;
			DeleteArray(Str);
			Str = (char*) s;
		}
	}
	return Str;
}

#ifndef ToUpper
#define ToUpper(c)						( ((c)>='a'&&(c)<='z')?(c)-'a'+'A':(c) )
#endif

char *DecodeRfc2047(char *Str)
{
	if (!Str)
		return NULL;
	
	GStringPipe p(256);
	for (char *s = Str; *s; )
	{
		char *e = s;
		bool Decode = 0, Descape = 0;
		while (*e)
		{
			if
			(
				(Decode = (e[0] == '=' && e[1] == '?'))
				||
				(Descape = (e[0] == '\\'))
			)
			{
				// Emit characters between 's' and 'e'
				if (e > s)
					p.Write(s, e - s);
				break;
			}
			e++;
		}

		if (Decode)
		{
			// is there a word remaining
			bool Encoded = false;
			char *Start  = e + 2;
			char *First  = strchr(Start, '?');
			char *Second = First ? strchr(First + 1, '?') : NULL;
			char *End    = Second ? strstr(Second + 1, "?=") : NULL;
			if (End)
			{
				GString Cp(Start, First - Start);
				int Type = CONTENT_NONE;
				bool StripUnderscores = false;
				if (ToUpper(First[1]) == 'B')
				{
					// Base64 encoding
					Type = CONTENT_BASE64;
				}
				else if (ToUpper(First[1]) == 'Q')
				{
					// Quoted printable
					Type = CONTENT_QUOTED_PRINTABLE;
					StripUnderscores = true;
				}

				if (Type != CONTENT_NONE)
				{
					Second++;					
					char *Block = NewStr(Second, End-Second);
					if (Block)
					{
						switch (Type)
						{
							case CONTENT_BASE64:
								Block = DecodeBase64Str(Block);
								break;
							case CONTENT_QUOTED_PRINTABLE:
								Block = DecodeQuotedPrintableStr(Block);
								break;
						}

						size_t Len = strlen(Block);
						if (StripUnderscores)
						{
							for (char *i=Block; *i; i++)
							{
								if (*i == '_')
									*i = ' ';
							}
						}

						if (Cp && !_stricmp(Cp, "utf-8"))
						{
							p.Write((uchar*)Block, Len);
						}
						else
						{
							GAutoString Utf8((char*)LgiNewConvertCp("utf-8", Block, Cp, Len));
							if (Utf8)
							{
								if (LgiIsUtf8(Utf8))
									p.Write((uchar*)Utf8.Get(), strlen(Utf8));
							}
							else
							{
								p.Write((uchar*)Block, Len);
							}
						}

						DeleteArray(Block);
					}
					
					s = End + 2;
					if (*s == '\n')
					{
						s++;
						while (*s && strchr(WhiteSpace, *s)) s++;
					}
					
					Encoded = true;
				}
			}

			if (!Encoded)
			{
				// Encoding error, just emit the raw string and exit.
				size_t Len = strlen(s);
				p.Write((uchar*) s, Len);
				break;
			}
		}
		else if (Descape)
		{
			// Un-escape the string...
			e++;
			if (*e)
				p.Write(e, 1);
			else
				break;
			s = e + 1;
		}
		else
		{
			// Last segment of string...
			LgiAssert(*e == 0);
			if (e > s)
				p.Write(s, e - s);
			break;
		}
	}

	DeleteArray(Str);
	return p.NewStr();
}

#define MIME_MAX_LINE		76

char *EncodeRfc2047(char *Str, const char *CodePage, List<char> *CharsetPrefs, ssize_t LineLength)
{
	if (!CodePage)
	{
		CodePage = "utf-8";
	}

	GStringPipe p(256);

	if (!Str)
		return NULL;

	if (Is8Bit(Str))
	{
		// pick an encoding
		bool Base64 = false;
		const char *DestCp = "utf-8";
		size_t Len = strlen(Str);;
		if (_stricmp(CodePage, "utf-8") == 0)
		{
			DestCp = LgiDetectCharset(Str, Len, CharsetPrefs);
		}

		int Chars = 0;
		for (unsigned i=0; i<Len; i++)
		{
			if (Str[i] & 0x80)
				Chars++;
		}
		if
		(
			stristr(DestCp, "utf") ||
			(
				Len > 0 &&
				((double)Chars/Len) > 0.4
			)
		)
		{
			Base64 = true;
		}

		char *Buf = (char*)LgiNewConvertCp(DestCp, Str, CodePage, Len);
		if (Buf)
		{
			// encode the word
			char Prefix[64];
			int Ch = sprintf_s(Prefix, sizeof(Prefix), "=?%s?%c?", DestCp, Base64 ? 'B' : 'Q');
			p.Write(Prefix, Ch);
			LineLength += Ch;

			if (Base64)
			{
				// Base64
				size_t InLen = strlen(Buf);
				// int EstBytes = BufferLen_BinTo64(InLen);

				char Temp[512];
				ssize_t Bytes = ConvertBinaryToBase64(Temp, sizeof(Temp), (uchar*)Buf, InLen);
				p.Push(Temp, Bytes);
			}
			else
			{
				// Quoted printable
				for (char *w = Buf; *w; w++)
				{
					if (*w == ' ')
					{
						if (LineLength > MIME_MAX_LINE - 3)
						{
							p.Print("?=\r\n\t%s", Prefix);
							LineLength = 1 + strlen(Prefix);
						}

						p.Write((char*)"_", 1);
						LineLength++;
					}
					else if (*w & 0x80 ||
							*w == '_' ||
							*w == '?' ||
							*w == '=')
					{
						if (LineLength > MIME_MAX_LINE - 5)
						{
							p.Print("?=\r\n\t%s", Prefix);
							LineLength = 1 + strlen(Prefix);
						}

						char Temp[16];
						Ch = sprintf_s(Temp, sizeof(Temp), "=%2.2X", (uchar)*w);
						p.Write(Temp, Ch);
						LineLength += Ch;
					}
					else
					{
						if (LineLength > MIME_MAX_LINE - 3)
						{
							p.Print("?=\r\n\t%s", Prefix);
							LineLength = 1 + strlen(Prefix);
						}

						p.Write(w, 1);
						LineLength++;
					}
				}
			}

			p.Push("?=");
			DeleteArray(Buf);
		}

		DeleteArray(Str);
		Str = p.NewStr();
	}
	else
	{
		bool RecodeNewLines = false;
		for (char *s = Str; *s; s++)
		{
			if (*s == '\n' && (s == Str || s[-1] != '\r'))
			{
				RecodeNewLines = true;
				break;
			}
		}
		
		if (RecodeNewLines)
		{
			for (char *s = Str; *s; s++)
			{
				if (*s == '\r')
					;
				else if (*s == '\n')
					p.Write("\r\n", 2);
				else
					p.Write(s, 1);
			}
			
			DeleteArray(Str);
			Str = p.NewStr();
		}
	}

	return Str;
}

//////////////////////////////////////////////////////////////////////////////
void DeNullText(char *in, ssize_t &len)
{
	char *out = in;
	char *end = in + len;
	while (in < end)
	{
		if (*in)
		{
			*out++ = *in;
		}
		else
		{
			len--;
		}
		in++;
	}
}

//////////////////////////////////////////////////////////////////////////////
bool IsValidEmail(GString &Email)
{
	// Local part
	char buf[321];
	char *o = buf;
	char *e = Email;

	if (!Email || *e == '.')
		return false;

	#define OutputChar()	\
		if (o - buf >= sizeof(buf) - 1)	\
			return false;	\
		*o++ = *e++

	// Local part
	while (*e)
	{
		if (strchr("!#$%&\'*+-/=?^_`.{|}~", *e) ||
			IsAlpha((uchar)*e) ||
			IsDigit((uchar)*e))
		{
			OutputChar();
		}
		else if (*e == '\"')
		{
			// Quoted string
			OutputChar();

			bool quote = false;
			while (*e && !quote)
			{
				quote = *e == '\"';
				OutputChar();
			}
		}
		else if (*e == '\\')
		{
			// Quoted character
			e++;
			if (*e < ' ' || *e >= 0x7f)
				return false;
			OutputChar();
		}
		else if (*e == '@')
		{
			break;
		}
		else
		{
			// Illegal character
			return false;
		}
	}

	// Process the '@'
	if (*e != '@' || o - buf > 64)
		return false;
	OutputChar();
	
	// Domain part...
	if (*e == '[')
	{
		// IP addr
		OutputChar();

		// Initial char must by a number
		if (!IsDigit(*e))
			return false;
		
		// Check the rest...
		char *Start = e;
		while (*e)
		{
			if (IsDigit(*e) ||
				*e == '.')
			{
				OutputChar();
			}
			else
			{
				return false;
			}
		}

		// Not a valid IP
		if (e - Start > 15)
			return false;

		if (*e != ']')
			return false;

		OutputChar();
	}
	else
	{
		// Hostname, check initial char
		if (!IsAlpha(*e) && !IsDigit(*e))
			return false;
		
		// Check the rest.
		while (*e)
		{
			if (IsAlpha(*e) ||
				IsDigit(*e) ||
				strchr(".-", *e))
			{
				OutputChar();
			}
			else
			{
				return false;
			}
		}
	}

	// Remove any trailing dot/dash
	while (strchr(".-", o[-1]))
		o--;

	// Output
	*o = 0;
	LgiAssert(o - buf <= sizeof(buf));
	if (strcmp(Email, buf))
		Email.Set(buf, o - buf);
	return true;
}

typedef char CharPair[2];
static CharPair Pairs[] =
{
    {'<', '>'},
    {'(', ')'},
    {'\'', '\''},
    {'\"', '\"'},
    {0, 0},
};

struct MailAddrPart
{
    GAutoString Part;
    bool Brackets;
    bool ValidEmail;
    

    GAutoString RemovePairs(char *Str, ssize_t Len, CharPair *Pairs)
    {
	    char *s = Str;
	    if (Len < 0)
	        Len = strlen(s);
	    while (*s && strchr(WhiteSpace, *s))
	    {
	        s++;
	        Len--;
	    }

	    if (!*s)
		    return GAutoString();

	    // Get the end of the string...
	    char *e = s;
	    if (Len < 0)
	        e += strlen(s);
	    else
	        e += Len;

	    // Seek back over any trailing whitespace
	    while (e > s && strchr(WhiteSpace, e[-1]))
		    e--;

	    for (CharPair *p = Pairs; (*p)[0]; p++)
	    {
		    if ((*p)[0] == *s && (*p)[1] == e[-1])
		    {
			    s++;
			    e--;
			    if (s < e)
			    {
				    // reset search
				    p = Pairs - 1;
			    }
			    else break;
		    }
	    }

	    Len = e - s;
	    if (Len < 0)
		    return GAutoString();

	    return GAutoString(NewStr(s, Len));
    }

    MailAddrPart(char *s, ssize_t len)
    {
        ValidEmail = false;
        Brackets = false;
        if (s)
        {
		    if (len < 0)
		        len = strlen(s);
		    while (strchr(WhiteSpace, *s) && len > 0)
		    {
			    s++;
			    len--;
		    }
            
            Brackets = *s == '<';
            Part = RemovePairs(s, len, Pairs);
            // ValidEmail = IsValidEmail(Part);
        }
    }
    
    int Score()
    {
        if (!Part)
            return 0;
        return (ValidEmail ? 1 : 0) + (Brackets ? 1 : 0);
    }
};

int PartCmp(GAutoPtr<MailAddrPart> *a, GAutoPtr<MailAddrPart> *b)
{
    return (*b)->Score() - (*a)->Score();
}

void DecodeAddrName(const char *Str, GAutoString &Name, GAutoString &Addr, const char *DefaultDomain)
{
	/* Testing code
	char *Input[] =
	{
		"\"Sound&Secure@speedytechnical.com\" <soundandsecure@speedytechnical.com>",
		"\"@MM-Social Mailman List\" <social@cisra.canon.com.au>",
		"'Matthew Allen (fret)' <fret@memecode.com>",
		"Matthew Allen (fret) <fret@memecode.com>",
		"\"'Matthew Allen'\" <fret@memecode.com>",
		"Matthew Allen",
		"fret@memecode.com",
		"\"<Matthew Allen>\" <fret@memecode.com>",
		"<Matthew Allen> (fret@memecode.com)",
		"Matthew Allen <fret@memecode.com>",
		"\"Matthew, Allen\" (fret@memecode.com)",
		"Matt'hew Allen <fret@memecode.com>",
		"john.omalley <john.O'Malley@testing.com>",
		"Bankers' Association (ABA)<survey@aawp.org.au>",
		"'Amy's Mum' <name@domain.com>",
		"\"Philip Doggett (JIRA)\" <jira@audinate.atlassian.net>",
		0
	};

	GAutoString Name, Addr;
	for (char **i = Input; *i; i++)
	{
		Name.Reset();
		Addr.Reset();
		DecodeAddrName(*i, Name, Addr, "name.com");
		LgiTrace("N=%-#32s A=%-32s\n", Name, Addr);
	}
	*/

	if (!Str)
		return;

	GArray< GAutoPtr<MailAddrPart> > Parts;

	GString s = Str;
	GString non;
	GString email;
	GString::Array a = s.SplitDelimit("<>");
	for (unsigned i=0; i<a.Length(); i++)
	{
		if (IsValidEmail(a[i]))
		{
			email = a[i];
		}
		else
		{
			non += a[i];
		}
	}
	if (!email)
	{
		a = s.SplitDelimit("()");
		non.Empty();
		for (unsigned i=0; i<a.Length(); i++)
		{
			if (IsValidEmail(a[i]))
			{
				email = a[i];
			}
			else
			{
				non += a[i];
			}
		}	
		if (!email)
		{
			a = s.SplitDelimit(WhiteSpace);
			non.Empty();
			for (unsigned i=0; i<a.Length(); i++)
			{
				if (IsValidEmail(a[i]))
				{
					email = a[i];
				}
				else
				{
					if (non.Get())
						non += " ";
					non += a[i];
				}
			}	
		}
	}
	
	if (non.Length() > 0)
	{
		const char *ChSet = " \t\r\n\'\"<>";
		do
		{
			non = non.Strip(ChSet);
		}
		while (non.Length() > 0 && strchr(ChSet, non(0)));
	}
	
	Name.Reset(NewStr(non));
	Addr.Reset(NewStr(email.Strip()));
}

void StrCopyToEOL(char *d, char *s)
{
	if (d && s)
	{
		while (*s && *s != '\r' && *s != '\n')
		{
			*d++ = *s++;
		}

		*d = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailTransaction::MailTransaction()
{
	Index = -1;
	Flags = 0;
	Status = false;
	Oversize = false;
	Stream = 0;
	UserData = 0;
}

MailTransaction::~MailTransaction()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
FileDescriptor::FileDescriptor()
{
	Embeded = 0;
	Offset = 0;
	Size = 0;
	Data = 0;
	MimeType = 0;
	ContentId = 0;
	Lock = 0;
	OwnEmbeded = false;
}

FileDescriptor::FileDescriptor(GStreamI *embed, int64 offset, int64 size, char *name)
{
	Embeded = embed;
	Offset = offset;
	Size = size;
	Data = 0;
	MimeType = 0;
	Lock = 0;
	ContentId = 0;
	OwnEmbeded = false;

	if (name)
	{
		Name(name);
	}
}

FileDescriptor::FileDescriptor(char *name)
{
	Embeded = 0;
	Offset = 0;
	Size = 0;
	Data = 0;
	MimeType = 0;
	Lock = 0;
	ContentId = 0;
	OwnEmbeded = false;

	if (name)
	{
		Name(name);
		if (File.Open(name, O_READ))
		{
			Size = File.GetSize();
			File.Close();
		}
	}
}

FileDescriptor::FileDescriptor(char *data, int64 len)
{
	Embeded = 0;
	Offset = 0;
	MimeType = 0;
	Lock = 0;
	ContentId = 0;
	Size = len;
	OwnEmbeded = false;

	Data = data ? new uchar[(size_t)Size] : 0;
	if (Data)
	{
		memcpy(Data, data, (size_t)Size);
	}
}

FileDescriptor::~FileDescriptor()
{
	if (OwnEmbeded)
	{
		DeleteObj(Embeded);
	}

	DeleteArray(MimeType);
	DeleteArray(ContentId);
	DeleteArray(Data);
}

void FileDescriptor::SetOwnEmbeded(bool i)
{
	OwnEmbeded = i;
}

void FileDescriptor::SetLock(LMutex *l)
{
	Lock = l;
}

LMutex *FileDescriptor::GetLock()
{
	return Lock;
}

GStreamI *FileDescriptor::GotoObject()
{
	if (Embeded)
	{
		Embeded->SetPos(Offset);
		return Embeded;
	}
	else if (Name() && File.Open(Name(), O_READ))
	{
		return &File;
	}
	else if (Data && Size > 0)
	{
		DataStream.Reset(new GMemStream(Data, Size, false));
		return DataStream;
	}

	return 0;
}

int FileDescriptor::Sizeof()
{
	return (int)Size;
}

uchar *FileDescriptor::GetData()
{
	return Data;
}

bool FileDescriptor::Decode(char *ContentType,
							char *ContentTransferEncoding,
							char *MimeData,
							int MimeDataLen)
{
	bool Status = false;
	int Content = CONTENT_NONE;

	if (ContentType && ContentTransferEncoding)
	{
		// Content-Type: application/octet-stream; name="Scribe.opt"
		Content = CONTENT_OCTET_STREAM;

		if (strnistr(ContentTransferEncoding, "base64", 1000))
		{
			Content = CONTENT_BASE64;
		}

		if (strnistr(ContentTransferEncoding, "quoted-printable", 1000))
		{
			Content = CONTENT_QUOTED_PRINTABLE;
		}

		if (Content != CONTENT_NONE)
		{
			const char *NameKey = "name";
			char *n = strnistr(ContentType, NameKey, 1000);
			if (n)
			{
				char *Equal = strchr(n, '=');
				if (Equal)
				{
					Equal++;
					while (*Equal && *Equal == '\"')
					{
						Equal++;
					}

					char *End = strchr(Equal, '\"');
					if (End)
					{
						*End = 0;
					}

					Name(Equal);
					Status = true;
				}
			}
		}
	}

	if (Status && MimeData && MimeDataLen > 0 && Content != CONTENT_NONE)
	{
		Status = false;
		char *Base64 = new char[MimeDataLen];

		switch (Content)
		{
			case CONTENT_OCTET_STREAM:
			{
				Size = 0;
				DeleteObj(Data);
				Data = new uchar[MimeDataLen];
				if (Data)
				{
					Size = MimeDataLen;
					memcpy(Data, MimeData, (size_t)Size);
					Status = true;
				}
				break;
			}
			case CONTENT_QUOTED_PRINTABLE:
			{
				Size = 0;
				DeleteObj(Data);
				Data = new uchar[MimeDataLen+1];
				if (Data)
				{
					char *Out = (char*) Data;
					for (int i=0; i<MimeDataLen; i++)
					{
						if (MimeData[i] == '=')
						{
							if (MimeData[i+1] == '\r' || MimeData[i+1] == '\n')
							{
								if (MimeData[i+1] == '\r') i++;
								if (MimeData[i+1] == '\n') i++;
							}
							else
							{
								i++;
								char c = ConvHexToBin(MimeData[i]) << 4;
								i++;
								c |= ConvHexToBin(MimeData[i]) & 0xF;

								Size++;
								*Out++ = c;
							}
						}
						else
						{
							Size++;
							*Out++ = MimeData[i];
						}
					}

					*Out++ = 0;
					Status = true;
				}
				break;
			}
			case CONTENT_BASE64:
			{
				int InCount = 0;
				int OutCount = 0;
				char *In = MimeData;
				char *Out = Base64;
				while (InCount < MimeDataLen)
				{
					if (*In != '\r' && *In != '\n')
					{
						*Out++ = *In;
						OutCount++;
					}
					InCount++;
					In++;
				}

				Size = BufferLen_64ToBin(OutCount);
				Data = new uchar[(size_t)Size];
				if (Data)
				{
					ssize_t Converted = ConvertBase64ToBinary(Data, Size, Base64, OutCount);
					Status = Converted <= Size && Converted >= Size - 3;
					if (Status)
					{
						Size = Converted;
					}
					else
					{
						DeleteArray(Data);
						Size = 0;
					}
				}
				break;
			}
		}
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
AddressDescriptor::AddressDescriptor(AddressDescriptor *Copy)
{
	Data = 0;
	Status = Copy ? Copy->Status : false;
	CC = Copy ? Copy->CC : false;
	Addr = Copy ? NewStr(Copy->Addr) : 0;
	Name = Copy ? NewStr(Copy->Name) : 0;
}

AddressDescriptor::~AddressDescriptor()
{
	_Delete();
}

void AddressDescriptor::_Delete()
{
	Data = 0;
	Status = false;
	CC = 0;
	DeleteArray(Name);
	DeleteArray(Addr);
}

void AddressDescriptor::Print(char *Str, int Len)
{
	if (!Str)
	{
		LgiAssert(0);
		return;
	}

	if (Addr && Name)
	{
		sprintf_s(Str, Len, "%s (%s)", Addr, Name);
	}
	else if (Addr)
	{
		strcpy_s(Str, Len, Addr);
	}
	else if (Name)
	{
		sprintf_s(Str, Len, "(%s)", Name);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailProtocol::MailProtocol()
{
	Buffer[0] = 0;
	Logger = 0;
	ErrMsgId = 0;

	Items = 0;
	Transfer = 0;
}

MailProtocol::~MailProtocol()
{
	CharsetPrefs.DeleteArrays();
}

void MailProtocol::Log(const char *Str, GSocketI::SocketMsgType type)
{
	if (Logger && Str)
	{
		char s[1024];
		char *e = s + sizeof(s) - 2;
		const char *i = Str;
		char *o = s;
		while (*i && o < e)
		{
			*o++ = *i++;
		}
		while (o > s && (o[-1] == '\r' || o[-1] == '\n'))
			o--;
		*o++ = '\n';
		*o = 0;

		Logger->Write(s, o - s, type);
	}
}

bool MailProtocol::Error(const char *file, int line, const char *msg, ...)
{
	char s[1024];
	va_list a;
	va_start(a, msg);
	vsprintf_s(s, sizeof(s), msg, a);
	va_end(a);

	Log(s, GSocketI::SocketMsgError);
	LgiTrace("%s:%i - Error: %s", file, line, s);
	return false;
}

bool MailProtocol::Read()
{
	bool Status = false;
	if (Socket)
	{
		Status = Socket->Read(Buffer, sizeof(Buffer), 0) > 0;
	}
	return Status;
}

bool MailProtocol::Write(const char *Buf, bool LogWrite)
{
	bool Status = false;
	if (Socket)
	{
		const char *p = Buf ? Buf : Buffer;
		Status = Socket->Write(p, strlen(p), 0) > 0;
		
		if (LogWrite)
		{
			Log(p, GSocketI::SocketMsgSend);
		}
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#define	VERIFY_RET_VAL(Arg)						\
{												\
	if (!Arg)									\
	{											\
		LMutex::Auto Lck(&SocketLock, _FL);	\
		Socket.Reset(0);						\
		return NULL;							\
	}											\
}

#define	VERIFY_ONERR(Arg)						\
{												\
	if (!Arg)									\
	{											\
		LMutex::Auto Lck(&SocketLock, _FL);	\
		Socket.Reset(0);						\
		goto CleanUp;							\
	}											\
}

MailSmtp::MailSmtp()
{
}

MailSmtp::~MailSmtp()
{
}

bool MailSmtp::Open(GSocketI *S,
					const char *RemoteHost,
					const char *LocalDomain,
					const char *UserName,
					const char *Password,
					int Port,
					int Flags)
{
	char Str[256] = "";
	bool Status = false;

	if (!RemoteHost)
		Error(_FL, "No remote SMTP host.\n");
	else
	{
		strcpy_s(Str, sizeof(Str), RemoteHost);
		char *Colon = strchr(Str, ':');
		if (Colon)
		{
			*Colon = 0;
			Colon++;
			Port = atoi(Colon);
		}
		if (Port == 0)
		{
			if (Flags & MAIL_SSL)
				Port = SMTP_SSL_PORT;
			else
				Port = SMTP_PORT;
		}

		GAutoString Server(TrimStr(Str));
		if (Server)
		{
			if (SocketLock.Lock(_FL))
			{
				Socket.Reset(S);
				SocketLock.Unlock();
			}

			Socket->SetTimeout(30 * 1000);

			char Msg[256];
			sprintf_s(Msg, sizeof(Msg), "Connecting to %s:%i...", Server.Get(), Port);
			Log(Msg, GSocketI::SocketMsgInfo);
	
			if (!Socket->Open(Server, Port))
				Error(_FL, "Failed to connect socket to %s:%i\n", Server.Get(), Port);
			else
			{
				GStringPipe Str;

				// receive signon message
				VERIFY_RET_VAL(ReadReply("220"));

				// Rfc 2554 ESMTP authentication
				SmtpHello:
				sprintf_s(Buffer, sizeof(Buffer), "EHLO %s\r\n", (ValidNonWSStr(LocalDomain)) ? LocalDomain : "default");
				VERIFY_RET_VAL(Write(0, true));
				/*bool HasSmtpExtensions =*/ ReadReply("250", &Str);

				bool Authed = false;
				bool NoAuthTypes = false;
				bool SupportsStartTLS = false;
				GHashTbl<const char*, bool> TheirAuthTypes;

				// Look through the response for the auth line
				char *Response = Str.NewStr();
				if (Response)
				{
					GToken Lines(Response, "\n");
					for (uint32 i=0; i<Lines.Length(); i++)
					{
						char *l = Lines[i];
						char *AuthStr = strstr(l, "AUTH");
						if (AuthStr)
						{
							// walk through AUTH types
							GToken Types(AuthStr + 4, " ,;");
							for (uint32 a=0; a<Types.Length(); a++)
							{
								TheirAuthTypes.Add(Types[a], true);
							}
						}
						if (stristr(l, "STARTTLS"))
						{
							SupportsStartTLS = true;
						}
					}
					DeleteArray(Response);
				}

				if (SupportsStartTLS && TestFlag(Flags, MAIL_USE_STARTTLS))
				{
					strcpy_s(Buffer, sizeof(Buffer), "STARTTLS\r\n");
					VERIFY_RET_VAL(Write(0, true));
					VERIFY_RET_VAL(ReadReply("220", &Str));

					GVariant v;
					if (Socket->SetValue(GSocket_Protocol, v="SSL"))
					{
						Flags &= ~MAIL_USE_STARTTLS;
						goto SmtpHello;
					}
					else
					{
						// SSL init failed... what to do here?
					}
				}


				if (ValidStr(UserName) &&
					ValidStr(Password))
				{
					GHashTbl<const char*, bool> MyAuthTypes(16);
					MyAuthTypes.Add("PLAIN", true);
					MyAuthTypes.Add("LOGIN", true);

					if (TheirAuthTypes.Length() == 0)
					{
						// No auth types? huh?
						if (TestFlag(Flags, MAIL_USE_AUTH))
						{
							if (TestFlag(Flags, MAIL_USE_PLAIN))
							{
								// Force plain type
								TheirAuthTypes.Add("PLAIN", true);
							}
							else if (TestFlag(Flags, MAIL_USE_LOGIN))
							{
								// Force login type
								TheirAuthTypes.Add("LOGIN", true);
							}
							else
							{
								// Oh well, we'll just try all types
								// const char *a;
								// for (bool b=MyAuthTypes.First(&a); b; b=MyAuthTypes.Next(&a))
								for (auto i : MyAuthTypes)
								{
									TheirAuthTypes.Add(i.key, true);
								}
							}
						}
						else
						{						
							NoAuthTypes = true;
						}
					}

					// Try all their auth types against our internally support types
					if (TheirAuthTypes.Find("LOGIN"))
					{
						VERIFY_RET_VAL(Write("AUTH LOGIN\r\n", true));
						VERIFY_RET_VAL(ReadReply("334"));

						ZeroObj(Buffer);
						ConvertBinaryToBase64(Buffer, sizeof(Buffer), (uchar*)UserName, strlen(UserName));
						strcat(Buffer, "\r\n");
						VERIFY_RET_VAL(Write(0, true));
						if (ReadReply("334"))
						{
							ZeroObj(Buffer);
							ConvertBinaryToBase64(Buffer, sizeof(Buffer), (uchar*)Password, strlen(Password));
							strcat(Buffer, "\r\n");
							VERIFY_RET_VAL(Write(0, true));
							if (ReadReply("235"))
							{
								Authed = true;
							}
						}
					}
					
					if (!Authed && TheirAuthTypes.Find("PLAIN"))
					{
						char Tmp[256];
						ZeroObj(Tmp);
						int ch = 1;
						ch += sprintf_s(Tmp+ch, sizeof(Tmp)-ch, "%s", UserName) + 1;
						ch += sprintf_s(Tmp+ch, sizeof(Tmp)-ch, "%s", Password) + 1;

						char B64[256];
						ZeroObj(B64);
						ConvertBinaryToBase64(B64, sizeof(B64), (uint8*)Tmp, ch);

						sprintf_s(Buffer, sizeof(Buffer), "AUTH PLAIN %s\r\n", B64);
						VERIFY_RET_VAL(Write(0, true));
						if (ReadReply("235"))
						{
							Authed = true;
						}
					}

					if (!Authed)
					{
						if (NoAuthTypes)
							SetError(L_ERROR_ESMTP_NO_AUTHS, "The server didn't return the authentication methods it supports.");
						else
						{
							GString p;
							// const char *a;
							// for (bool b = TheirAuthTypes.First(&a); b; b = TheirAuthTypes.Next(&a))
							for (auto i : TheirAuthTypes)
							{
								if (p.Get())
									p += ", ";
								p += i.key;
							}

							SetError(L_ERROR_UNSUPPORTED_AUTH, "Authentication failed, types available:\n\t%s", p);
						}
					}

					Status = Authed;
				}
				else
				{
					Status = true;
				}
			}
		}
	}

	return Status;
}

bool MailSmtp::WriteText(const char *Str)
{
	// we have to convert all strings to CRLF in here
	bool Status = false;
	if (Str)
	{
		GMemQueue Temp;

		const char *Start = Str;
		while (*Str)
		{
			if (*Str == '\n')
			{
				// send a string
				ssize_t Size = Str-Start;
				if (Str[-1] == '\r') Size--;
				Temp.Write((uchar*) Start, Size);
				Temp.Write((uchar*) "\r\n", 2);
				Start = Str + 1;
			}

			Str++;
		}

		// send the final string
		ssize_t Size = Str-Start;
		if (Str[-1] == '\r') Size--;
		Temp.Write((uchar*) Start, (int)Size);

		Size = (int)Temp.GetSize();
		char *Data = new char[(size_t)Size];
		if (Data)
		{
			Temp.Read((uchar*) Data, Size);
			Status = Socket->Write(Data, (int)Size, 0) == Size;
			DeleteArray(Data);
		}
	}

	return Status;
}

char *StripChars(char *Str, const char *Chars = "\r\n")
{
	if (Str)
	{
		char *i = Str;
		char *o = Str;
		while (*i)
		{
			if (strchr(Chars, *i))
				i++;
			else
				*o++ = *i++;
		}
		*o++ = 0;
	}

	return Str;
}

char *CreateAddressTag(List<AddressDescriptor> &l, int Type, List<char> *CharsetPrefs)
{
	char *Result = 0;
	List<AddressDescriptor> Addr;
	AddressDescriptor *a;
	for (a = l.First(); a; a = l.Next())
	{
		if (a->CC == Type)
		{
			Addr.Insert(a);
		}
	}
	
	if (Addr.Length() > 0)
	{
		GStringPipe StrBuf;
		StrBuf.Push((Type == 0) ? (char*)"To: " : (char*)"Cc: ");

		for (a = Addr.First(); a; )
		{
			AddressDescriptor *NextA = Addr.Next();
			char Buffer[256] = "";
			
			StripChars(a->Name);
			StripChars(a->Addr);

			if (a->Addr && strchr(a->Addr, ','))
			{
				// Multiple address format
				GToken t(a->Addr, ",");
				for (uint32 i=0; i<t.Length(); i++)
				{
					sprintf_s(Buffer, sizeof(Buffer), "<%s>", t[i]);
					if (i < t.Length()-1) strcat(Buffer, ",\r\n\t");
					StrBuf.Push(Buffer);
					Buffer[0] = 0;
				}
			}
			else if (a->Name)
			{
				// Name and addr
				char *Mem = 0;
				char *Name = a->Name;

				if (Is8Bit(Name))
				{
					Name = Mem = EncodeRfc2047(NewStr(Name), 0, CharsetPrefs);
				}

				if (strchr(Name, '\"'))
				    sprintf_s(Buffer, sizeof(Buffer), "'%s' <%s>", Name, a->Addr);
				else
				    sprintf_s(Buffer, sizeof(Buffer), "\"%s\" <%s>", Name, a->Addr);

				DeleteArray(Mem);
			}
			else if (a->Addr)
			{
				// Just addr
				sprintf_s(Buffer, sizeof(Buffer), "<%s>", a->Addr);
			}

			if (NextA) strcat(Buffer, ",\r\n\t");
			StrBuf.Push(Buffer);

			a = NextA;
		}

		StrBuf.Push("\r\n");
		Result = StrBuf.NewStr();
	}

	return Result;
}

// This class implements a pipe that writes to a socket
class SocketPipe : public GStringPipe
{
	GSocketI *s;
	MailProtocolProgress *p;

public:
	bool Status;

	SocketPipe(GSocketI *socket, MailProtocolProgress *progress)
	{
		s = socket;
		p = progress;
		Status = true;
	}

	ssize_t Read(void *Ptr, ssize_t Size, int Flags)
	{
		return false;
	}
	
	int64 SetSize(int64 Size)
	{
		if (p)
		{
			p->Start = LgiCurrentTime();
			p->Range = (int)Size;
			return Size;
		}

		return -1;
	}

	ssize_t Write(const void *InPtr, ssize_t Size, int Flags)
	{
		char *Ptr = (char*)InPtr;
		char *e = Ptr + Size;
		
		while (Ptr < e)
		{
			ssize_t w = s->Write(Ptr, e - Ptr, 0);
			if (w > 0)
			{
				Ptr += w;

				if (p && p->Range && w > 0)
					p->Value += w;
			}
			else break;
		}

		return Ptr - (char*)InPtr;
	}
};

bool MailSmtp::SendToFrom(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	bool AddrOk = false;

	if (To.Length() == 0)
	{
		ErrMsgId = L_ERROR_ESMTP_NO_RECIPIENT;
		ErrMsgFmt = "No recipients to send to.";
		ErrMsgParam.Empty();
		return false;
	}

	// send MAIL message
	if (From && ValidStr(From->Addr))
	{
		sprintf_s(Buffer, sizeof(Buffer), "MAIL FROM: <%s>\r\n", From->Addr);
	}
	else
	{
		ErrMsgId = L_ERROR_ESMTP_NO_FROM;
		ErrMsgFmt = "No 'from' address in email.";
		ErrMsgParam.Empty();
		return false;
	}

	VERIFY_RET_VAL(Write(0, true));
	VERIFY_RET_VAL(ReadReply("250", 0, Err));

	// send RCPT message
	AddrOk = true;
	List<AddressDescriptor>::I Recip = To.begin();
	for (AddressDescriptor *a = *Recip; a; a = *++Recip)
	{
		char *Addr = ValidStr(a->Addr) ? a->Addr : a->Name;
		if (ValidStr(Addr))
		{
			GToken Parts(Addr, ",");
			for (unsigned p=0; p<Parts.Length(); p++)
			{
				sprintf_s(Buffer, sizeof(Buffer), "RCPT TO: <%s>\r\n", Parts[p]);
				VERIFY_RET_VAL(Write(0, true));
				a->Status = ReadReply("25", 0, Err);
				AddrOk |= a->Status != 0; // at least one address is ok
			}
		}
		else if (Err)
		{
			ErrMsgId = L_ERROR_ESMTP_BAD_RECIPIENT;
			ErrMsgFmt = "Invalid recipient '%s'.";
			ErrMsgParam = Addr;
		}
	}

	return AddrOk;
}

GStringPipe *MailSmtp::SendData(MailProtocolError *Err)
{
	// send DATA message
	sprintf_s(Buffer, sizeof(Buffer), "DATA\r\n");
	VERIFY_RET_VAL(Write(0, true));
	VERIFY_RET_VAL(ReadReply("354", 0, Err));

	return new SocketPipe(Socket, Transfer);
}

GStringPipe *MailSmtp::SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	return SendToFrom(To, From, Err) ? SendData(Err) : NULL;
}

bool MailSmtp::SendEnd(GStringPipe *m)
{
	bool Status = false;

	SocketPipe *Msg = dynamic_cast<SocketPipe*>(m);
	if (Msg)
	{
		// send message terminator and receive reply
		if (Msg->Status &&
			Msg->Write((void*)"\r\n.\r\n", 5, 0))
		{
			Status = ReadReply("250");
		}
		// else
		//		just close the connection on them
		//		so nothing gets sent
	}

	DeleteObj(m);

	return Status;
}

/*
bool MailSmtp::Send(MailMessage *Msg, bool Mime)
{
	bool Status = false;

	if (Socket && Msg)
	{
		GStringPipe *Sink = SendStart(Msg->To, Msg->From);
		if (Sink)
		{
			// setup a gui progress meter to send the email,
			// the length is just a guesstimate as we won't know the exact
			// size until we encode it all, and I don't want it hanging around
			// in memory at once, so we encode and send on the fly.
			int Length =	1024 +
									(Msg->GetBody() ? strlen(Msg->GetBody()) : 0);
			for (FileDescriptor *f=Msg->FileDesc.First(); f; f=Msg->FileDesc.Next())
			{
				Length += f->Sizeof() * 4 / 3;
			}

			// encode and send message for transport
			Msg->Encode(*Sink, 0, this);
			Status = SendEnd(Sink);
		}
	}

	return Status;
}
*/

bool MailSmtp::Close()
{
	if (Socket)
	{
		// send QUIT message
		sprintf_s(Buffer, sizeof(Buffer), "QUIT\r\n");
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply("221"));
		
		LMutex::Auto Lock(&SocketLock, _FL);
		Socket.Reset(0);
		return true;
	}

	return false;
}

bool MailSmtp::ReadReply(const char *Str, GStringPipe *Pipe, MailProtocolError *Err)
{
	bool Status = false;
	if (Socket && Str)
	{
		int Pos = 0;
		char *Start = Buffer;
		ZeroObj(Buffer);

		while (Pos < sizeof(Buffer))
		{
			ssize_t Len = Socket->Read(Buffer+Pos, sizeof(Buffer)-Pos, 0);
			if (Len > 0)
			{
				char *Eol = strstr(Start, "\r\n");
				while (Eol)
				{
					// wipe EOL chars
					*Eol++ = 0;
					*Eol++ = 0;

					// process
					if (Pipe)
					{
						if (Pipe->GetSize()) Pipe->Push("\n");
						Pipe->Push(Start);
					}

					if (Start[3] == ' ')
					{
						// end of response
						if (!strncmp(Start, Str, strlen(Str)))
						{
							Status = true;
						}

						if (Err)
						{
							Err->Code = atoi(Start);
							char *Sp = strchr(Start, ' ');
							Err->ErrMsg = Sp ? Sp + 1 : Start;
						}

						// Log
						Log(Start, atoi(Start) >= 400 ? GSocketI::SocketMsgError : GSocketI::SocketMsgReceive);

						// exit loop
						Pos = sizeof(Buffer);
						break;
					}
					else
					{
						Log(Start, GSocketI::SocketMsgReceive);

						// more lines follow
						Start = Eol;
						Eol = strstr(Start, "\r\n");
					}
				}

				Pos += Len;
   			}
			else break;
		}

		if (!Status)
		{
			SetError(L_ERROR_GENERIC, "Error: %s", Buffer);
		}
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
class Mail2Folder : public GStringPipe
{
	char File[256];
	GFile F;

public:
	Mail2Folder(char *Path, List<AddressDescriptor> &To)
	{
		do
		{
			char n[32];
			sprintf_s(n, sizeof(n), "%u.mail", LgiRand());
			LgiMakePath(File, sizeof(File), Path, n);
		}
		while (FileExists(File));

		if (F.Open(File, O_WRITE))
		{
			F.Print("Forward-Path: ");

			int i = 0;
			for (AddressDescriptor *a=To.First(); a; a=To.Next())
			{
				a->Status = true;
				GToken Addrs(a->Addr, ",");
				for (unsigned n=0; n<Addrs.Length(); n++, i++)
				{
					if (i) F.Print(",");
					F.Print("<%s>", Addrs[n]);
				}
			}

			F.Print("\r\n");
		}
	}

	~Mail2Folder()
	{
		F.Close();
	}

	ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0)
	{
		return F.Read(Buffer, Size, Flags);
	}

	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0)
	{
		return F.Write(Buffer, Size, Flags);
	}
};

class MailPostFolderPrivate
{
public:
	char *Path;

	MailPostFolderPrivate()
	{
		Path = 0;
	}

	~MailPostFolderPrivate()
	{
		DeleteArray(Path);
	}
};

MailSendFolder::MailSendFolder(char *Path)
{
	d = new MailPostFolderPrivate;
	d->Path = NewStr(Path);
}

MailSendFolder::~MailSendFolder()
{
	DeleteObj(d);
}

bool MailSendFolder::Open(GSocketI *S, const char *RemoteHost, const char *LocalDomain, const char *UserName, const char *Password, int Port, int Flags)
{
	return DirExists(d->Path);
}

bool MailSendFolder::Close()
{
	return true;
}

GStringPipe *MailSendFolder::SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	return new Mail2Folder(d->Path, To);
}

bool MailSendFolder::SendEnd(GStringPipe *Sink)
{
	DeleteObj(Sink);
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
class MailItem
{
public:
	char *File;
	bool Delete;
	
	MailItem(char *f)
	{
		File = NewStr(f);
		Delete = false;
	}

	~MailItem()
	{
		DeleteArray(File);
	}
};

class MailReceiveFolderPrivate
{
public:
	char *Path;
	List<MailItem> Mail;

	MailReceiveFolderPrivate()
	{
		Path = 0;
	}

	~MailReceiveFolderPrivate()
	{
		DeleteArray(Path);
		Mail.DeleteObjects();
	}

	void Empty()
	{
		for (MailItem *m = Mail.First(); m; m = Mail.Next())
		{
			if (m->Delete)
			{
				FileDev->Delete(m->File, false);
			}
		}

		Mail.DeleteObjects();
	}
};

MailReceiveFolder::MailReceiveFolder(char *Path)
{
	d = new MailReceiveFolderPrivate;
	d->Path = NewStr(Path);
}

MailReceiveFolder::~MailReceiveFolder()
{
	DeleteObj(d);
}

bool MailReceiveFolder::Open(GSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, GDom *SettingStore, int Flags)
{
	// We don't use the socket so just free it here...
	DeleteObj(S);

	// Argument check
	if (!DirExists(d->Path))
		return false;
	
	GDirectory Dir;

	// Loop through files, looking for email
	for (int b = Dir.First(d->Path, LGI_ALL_FILES); b; b = Dir.Next())
	{
		if (!Dir.IsDir())
		{
			if (MatchStr("*.eml", Dir.GetName()) ||
				MatchStr("*.mail", Dir.GetName()))
			{
				char p[300];
				Dir.Path(p, sizeof(p));
				d->Mail.Insert(new MailItem(p));
			}
		}
	}

	return true;
}

bool MailReceiveFolder::Close()
{
	d->Empty();
	return true;
}

int MailReceiveFolder::GetMessages()
{
	return (int)d->Mail.Length();
}

bool MailReceiveFolder::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	for (unsigned i=0; i<Trans.Length(); i++)
	{
		MailTransaction *t = Trans[i];
		if (t && t->Stream)
		{
			t->Status = false;

			MailItem *m = d->Mail[t->Index];
			if (m)
			{
				GFile i;
				if (i.Open(m->File, O_READ))
				{
					GCopyStreamer c;
					if (c.Copy(&i, t->Stream))
					{
						Status = t->Status = true;
						if (Callbacks && Callbacks->OnReceive)
						{
							Callbacks->OnReceive(t, Callbacks->CallbackData);
						}
					}
				}
			}
		}
	}

	return Status;
}

bool MailReceiveFolder::Delete(int Message)
{
	MailItem *m = d->Mail[Message];
	if (m)
	{
		m->Delete = true;
		return false;
	}

	return false;
}

int MailReceiveFolder::Sizeof(int Message)
{
	MailItem *m = d->Mail[Message];
	if (m)
	{
		return (int)LgiFileSize(m->File);
	}

	return 0;
}

bool MailReceiveFolder::GetUid(int Message, char *Id, int IdLen)
{
	if (Id)
	{
		MailItem *m = d->Mail[Message];
		if (m)
		{
			char *s = strrchr(m->File, DIR_CHAR);
			if (s++)
			{
				char *e = strchr(s, '.');
				if (!e) e = s + strlen(s);
				ssize_t Len = e - s;
				memcpy(Id, s, Len);
				Id[Len] = 0;

				return true;
			}		
		}
	}

	return false;
}

bool MailReceiveFolder::GetUidList(List<char> &Id)
{
	bool Status = false;

	for (int i=0; i<d->Mail.Length(); i++)
	{
		char Uid[256];
		if (GetUid(i, Uid, sizeof(Uid)))
		{
			Status = true;
			Id.Insert(NewStr(Uid));
		}
		else
		{
			Id.DeleteArrays();
			Status = false;
			break;
		}
	}

	return Status;
}

char *MailReceiveFolder::GetHeaders(int Message)
{
	MailItem *m = d->Mail[Message];
	if (m)
	{
		GFile i;
		if (i.Open(m->File, O_READ))
		{
			GStringPipe o;
			GCopyStreamer c;
			GLinePrefix e("", false);
			if (c.Copy(&i, &o, &e))
			{
				return o.NewStr();
			}
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailPop3::MailPop3()
{
	End = "\r\n.\r\n";
	Marker = End;
	Messages = -1;
}

MailPop3::~MailPop3()
{
}

int MailPop3::GetMessages()
{
	if (Messages < 0)
	{
		if (Socket && Socket->IsOpen())
		{
			// see how many messages there are
			VERIFY_ONERR(Write("STAT\r\n", true));
			VERIFY_ONERR(ReadReply());

			Messages = GetInt();
		}
		else LgiAssert(!"No socket to get message count.");
	}

CleanUp:
	return Messages;
}

int MailPop3::GetInt()
{
	char Buf[32];
	char *Start = strchr(Buffer, ' ');
	if (Start)
	{
		Start++;
		char *End = strchr(Start, ' ');
		if (End)
		{
			int Len = (int) (End - Start);
			memcpy(Buf, Start, Len);
			Buf[Len] = 0;
			return atoi(Buf);
		}
	}
	return 0;
}

bool MailPop3::ReadReply()
{
	bool Status = false;
	if (Socket)
	{
		int Pos = 0;
		ZeroObj(Buffer);
		do
		{
			ssize_t Result = Socket->Read(Buffer+Pos, sizeof(Buffer)-Pos, 0);
			if (Result <= 0) // an error?
			{
				// Leave the loop...
				break;
			}

			Pos += Result;
    	}
		while (	!strstr(Buffer, "\r\n") &&
				sizeof(Buffer)-Pos > 0);

		Status = (Buffer[0] == '+') && strstr(Buffer, "\r\n");

		char *Cr = strchr(Buffer, '\r');
		if (Cr) *Cr = 0;
		if (ValidStr(Buffer))
			Log(Buffer, (Status) ? GSocketI::SocketMsgReceive : GSocketI::SocketMsgError);
		if (Cr) *Cr = '\r';

		if (!Status)
		{
			SetError(L_ERROR_GENERIC, "Error: %s", Buffer);
		}
	}

	return Status;
}

bool MailPop3::ListCmd(const char *Cmd, LHashTbl<ConstStrKey<char>, bool> &Results)
{
	sprintf_s(Buffer, sizeof(Buffer), "%s\r\n", Cmd);
	if (!Write(0, true))
		return false;

	char *b = Buffer;
	ssize_t r;
	while ((r = Socket->Read(b, sizeof(Buffer)-(b-Buffer))) > 0)
	{
		b += r;
		if (strnstr(Buffer, "\r\n.\r\n", b-Buffer))
			break;
	}

	if (r <= 0)
		return false;

	GToken t(Buffer, "\r\n");
	for (unsigned i=1; i<t.Length()-1; i++)
	{
		Results.Add(t[i], true);
	}

	return true;
}

#define OPT_Pop3NoApop		"NoAPOP"

bool MailPop3::Open(GSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, GDom *SettingStore, int Flags)
{
	bool Status = false;
	// bool RemoveMail = false;
	char Str[256] = "";
	char *Apop = 0;
	char *Server = 0;
	bool SecureAuth = TestFlag(Flags, MAIL_SECURE_AUTH);

	if (!RemoteHost)
		Error(_FL, "No remote POP host.\n");
	else 
	{
		if (Port < 1)
		{
			GVariant IsSsl;
			if (S->GetValue("IsSSL", IsSsl) &&
				IsSsl.CastInt32())
				Port = POP3_SSL_PORT;
			else
				Port = POP3_PORT;
		}

		strcpy_s(Str, sizeof(Str), RemoteHost);
		char *Colon = strchr(Str, ':');
		if (Colon)
		{
			*Colon = 0;
			Colon++;
			Port = atoi(Colon);
		}

		if (S &&
			User &&
			Password &&
			(Server = TrimStr(Str)))
		{
			S->SetTimeout(30 * 1000);

			ReStartConnection:
			
			if (SocketLock.Lock(_FL))
			{
				Socket.Reset(S);
				SocketLock.Unlock();
			}

			if (Socket &&
				Socket->Open(Server, Port) &&
				ReadReply())
			{
				GVariant NoAPOP = false;
				if (SettingStore)
					SettingStore->GetValue(OPT_Pop3NoApop, NoAPOP);
				if (!NoAPOP.CastInt32())
				{
					char *s = strchr(Buffer + 3, '<');
					if (s)
					{
						char *e = strchr(s + 1, '>');
						if (e)
						{
							Apop = NewStr(s, e - s + 1);
						}
					}
				}

				// login
				bool Authed = false;
				char *user = (char*) LgiNewConvertCp("iso-8859-1", User, "utf-8");
				char *pass = (char*) LgiNewConvertCp("iso-8859-1", Password, "utf-8");

				if (user && (pass || SecureAuth))
				{
					bool SecurityError = false;

					if (TestFlag(Flags, MAIL_USE_STARTTLS))
					{
						strcpy_s(Buffer, sizeof(Buffer), "STARTTLS\r\n");
						VERIFY_RET_VAL(Write(0, true));
						VERIFY_RET_VAL(ReadReply());

						GVariant v;
						if (Socket->SetValue(GSocket_Protocol, v="SSL"))
						{
							Flags &= ~MAIL_USE_STARTTLS;
						}
						else
						{
							SecurityError = true;
						}
					}

					if (!SecurityError && Apop) // GotKey, not implemented
					{
						// using encrypted password
						unsigned char Digest[16];
						char HexDigest[33];

						// append password
						char Key[256];
						sprintf_s(Key, sizeof(Key), "%s%s", Apop, pass);

						ZeroObj(Digest);
						MDStringToDigest(Digest, Key);
						for (int i = 0; i < 16; i++)
							sprintf_s(HexDigest + (i*2), 3, "%2.2x", Digest[i]);
						HexDigest[32] = 0;

						sprintf_s(Buffer, sizeof(Buffer), "APOP %s %s\r\n", user, HexDigest);
						VERIFY_ONERR(Write(0, true));
						Authed = ReadReply();

						if (!Authed)
						{
							DeleteArray(Apop);
							GVariant NoAPOP = true;
							if (SettingStore)
								SettingStore->SetValue(OPT_Pop3NoApop, NoAPOP);
							S->Close();
							goto ReStartConnection;
						}
					}

					if (!SecurityError && SecureAuth)
					{
						GHashTbl<const char*, bool> AuthTypes, Capabilities;
						if (ListCmd("AUTH", AuthTypes) &&
							ListCmd("CAPA", Capabilities))
						{
							if (AuthTypes.Find("GSSAPI"))
							{
								sprintf_s(Buffer, sizeof(Buffer), "AUTH GSSAPI\r\n");
								VERIFY_ONERR(Write(0, true));
								VERIFY_ONERR(ReadReply());

								// http://www.faqs.org/rfcs/rfc2743.html
							}
						}
					}					
					else if (!SecurityError && !Authed)
					{
						// have to use non-key method
						sprintf_s(Buffer, sizeof(Buffer), "USER %s\r\n", user);
						VERIFY_ONERR(Write(0, true));
						VERIFY_ONERR(ReadReply());

						sprintf_s(Buffer, sizeof(Buffer), "PASS %s\r\n", pass);
						VERIFY_ONERR(Write(0, false));
						Log("PASS *******", GSocketI::SocketMsgSend);

						Authed = ReadReply();
					}

					DeleteArray(user);
					DeleteArray(pass);
				}

				if (Authed)
				{
					Status = true;
				}
				else
				{
					if (SocketLock.Lock(_FL))
					{
						Socket.Reset(0);
						SocketLock.Unlock();
					}
					LgiTrace("%s:%i - Failed auth.\n", _FL);
				}
			}
			else Error(_FL, "Failed to open socket to %s:%i and read reply.\n", Server, Port);
		}
		else Error(_FL, "No user/pass.\n");
	}

CleanUp:
	DeleteArray(Apop);
	DeleteArray(Server);

	return Status;
}

bool MailPop3::MailIsEnd(char *Ptr, ssize_t Len)
{
	for (char *c = Ptr; Len-- > 0; c++)
	{
		if (*c != *Marker)
		{
			Marker = End;
		}

		if (*c == *Marker)
		{
			Marker++;
			if (!*Marker)
			{
				return true;
			}
		}
	}

	return false;
}

bool MailPop3::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	if (Trans.Length() > 0 &&
		Socket)
	{
		for (unsigned n = 0; n<Trans.Length(); n++)
		{
			int Message = Trans[n]->Index;
			GStreamI *Msg = Trans[n]->Stream;
			if (Msg)
			{
				int Size = 0;
				
				// Transfer is not null when the caller wants info on the bytes comming in
				if (Transfer || Callbacks)
				{
					// get message size
					sprintf_s(Buffer, sizeof(Buffer), "LIST %i\r\n", Message + 1);
					VERIFY_RET_VAL(Write(0, true));
					VERIFY_RET_VAL(ReadReply());

					char *s = strchr(Buffer, ' ');
					if (s)
					{
						s = strchr(s+1, ' ');
						if (s)
						{
							Size = atoi(s);
						}
					}
				}

				MailSrcStatus Action = DownloadAll;
				int TopLines = 100;

				if (Callbacks && Callbacks->OnSrc)
				{
					Action = Callbacks->OnSrc(Trans[n], Size, &TopLines, Callbacks->CallbackData);
				}

				if (Action == DownloadAbort)
				{
					break;
				}

				if (Action == DownloadAll ||
					Action == DownloadTop)
				{
					if (Action == DownloadAll)
					{
						sprintf_s(Buffer, sizeof(Buffer), "RETR %i\r\n", Message + 1);
					}
					else
					{
						sprintf_s(Buffer, sizeof(Buffer), "TOP %i %i\r\n", Message + 1, TopLines);
					}
					VERIFY_RET_VAL(Write(0, true));

					GLinePrefix End(".\r\n");

					if (Transfer)
					{
						Transfer->Value = 0;
						Transfer->Range = Size;
						Transfer->Start = LgiCurrentTime();
					}

					// Read status line
					ZeroObj(Buffer);

					int Used = 0;
					bool Ok = false;
					bool Finished = false;
					int64 DataPos = 0;
					while (Socket->IsOpen())
					{
						ssize_t r = Socket->Read(Buffer+Used, sizeof(Buffer)-Used-1, 0);
						if (r > 0)
						{
							DeNullText(Buffer + Used, r);
							
							if (Transfer)
							{
								Transfer->Value += r;
							}

							char *Eol = strchr(Buffer, '\n');
							if (Eol)
							{
								Eol++;
								Ok = Buffer[0] == '+';
								if (Ok)
								{
									// Log(Buffer, GSocketI::SocketMsgReceive);

									// The Buffer was zero'd at the beginning garrenteeing
									// NULL termination
									size_t Len = strlen(Eol);
									ssize_t EndPos = End.IsEnd(Eol, Len);
									if (EndPos >= 0)
									{
										Msg->Write(Eol, EndPos - 3);
										Status = Trans[n]->Status = true;
										Finished = true;
									}
									else
									{
										Msg->Write(Eol, Len);
										DataPos += Len;
									}
								}
								else
								{
									Log(Buffer, GSocketI::SocketMsgError);
									Finished = true;								
								}
								break;
							}

							Used += r;
						}
						else break;
					}

					if (!Finished)
					{
						if (Ok)
						{
							// Read rest of message
							while (Socket->IsOpen())
							{
								ssize_t r = Socket->Read(Buffer, sizeof(Buffer), 0);
								if (r > 0)
								{
									DeNullText(Buffer, r);
									
									if (Transfer)
									{
										Transfer->Value += r;
									}

									ssize_t EndPos = End.IsEnd(Buffer, r);
									if (EndPos >= 0)
									{
										ssize_t Actual = EndPos - DataPos - 3;
										if (Actual > 0)
										{
											ssize_t w = Msg->Write(Buffer, Actual);
											LgiAssert(w == Actual);
										}
										// else the end point was in the last buffer

										Status = Trans[n]->Status = true;
										break;
									}
									else
									{
										ssize_t w = Msg->Write(Buffer, r);
										LgiAssert(w == r);
										DataPos += r;
									}
								}
								else
								{
									break;
								}
							}
							
							if (!Status)
							{
								LgiTrace("%s:%i - Didn't get end-of-mail marker.\n", _FL);
							}
						}
						else
						{
							LgiTrace("%s:%i - Didn't get Ok.\n", _FL);
							break;
						}
					}

					if (Callbacks && Callbacks->OnReceive)
					{
						Callbacks->OnReceive(Trans[n], Callbacks->CallbackData);
					}

					if (Transfer)
					{
						Transfer->Empty();
					}
				}
				else
				{
					Trans[n]->Oversize = Status = true;
				}

				if (Items)
				{
					Items->Value++;
				}
			}
			else
			{
				LgiTrace("%s:%i - No stream.\n", _FL);
			}
		}
	}
	else
	{
		LgiTrace("%s:%i - Arg check failed, len=%p, sock=%p.\n", _FL, Trans.Length(), Socket.Get());
	}

	return Status;
}

bool MailPop3::GetSizes(GArray<int> &Sizes)
{
	if (Socket)
	{
		strcpy_s(Buffer, sizeof(Buffer), (char*)"LIST\r\n");
		VERIFY_RET_VAL(Write(0, true));

		char *s = 0;
		if (ReadMultiLineReply(s))
		{
			GToken l(s, "\r\n");
			DeleteArray(s);
			for (unsigned i=0; i<l.Length(); i++)
			{
				char *sp = strrchr(l[i], ' ');
				if (sp)
				{
					Sizes[i] = atoi(sp+1);
				}
			}
		}
	}

	return Sizes.Length() > 0;
}

int MailPop3::Sizeof(int Message)
{
	int Size = 0;
	if (Socket)
	{
		sprintf_s(Buffer, sizeof(Buffer), "LIST %i\r\n", Message + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		char *s = strchr(Buffer, ' ');
		if (s)
		{
			s = strchr(s+1, ' ');
			if (s)
			{
				Size = atoi(s);
			}
		}
	}

	return Size;
}

bool MailPop3::Delete(int Message)
{
	if (Socket)
	{
		sprintf_s(Buffer, sizeof(Buffer), "DELE %i\r\n", Message + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		return true;
	}

	return false;
}

bool MailPop3::GetUid(int Index, char *Id, int IdLen)
{
	if (Socket && Id)
	{
		sprintf_s(Buffer, sizeof(Buffer), "UIDL %i\r\n", Index + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		char *Space = strchr(Buffer, ' ');
		if (Space)
		{
			Space = strchr(Space+1, ' ');
			if (Space)
			{
				for (char *s = Space+1; *s; s++)
				{
					if (*s == '\r' || *s == '\n')
					{
						*s = 0;
						break;
					}
				}
				
				strcpy_s(Id, IdLen, Space+1);
				return true;
			}
		}
	}

	return false;
}

bool MailPop3::GetUidList(List<char> &Id)
{
	bool Status = false;
	if (Socket)
	{
		char *Str = 0;

		sprintf_s(Buffer, sizeof(Buffer), "UIDL\r\n");
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadMultiLineReply(Str));
		if (Str)
		{
			Status = true;
			GToken T(Str, "\r\n");
			for (unsigned i=0; i<T.Length(); i++)
			{
				char *s = T[i];
				if (s && *s != '.')
				{
					char *Space = strchr(s, ' ');
					if (Space++)
					{
						Id.Insert(NewStr(Space));
					}
				}
			}

			DeleteArray(Str);
		}
	}

	return Status;
}

char *MailPop3::GetHeaders(int Message)
{
	char *Header = 0;
	if (Socket)
	{
		sprintf_s(Buffer, sizeof(Buffer), "TOP %i 0\r\n", Message + 1);
		if (Write(0, true))
		{
			ReadMultiLineReply(Header);
		}
	}

	return Header;
}

bool MailPop3::ReadMultiLineReply(char *&Str)
{
	bool Status = false;
	if (Socket)
	{
		GMemQueue Temp;
		ssize_t ReadLen = Socket->Read(Buffer, sizeof(Buffer), 0);
		if (ReadLen > 0 && Buffer[0] == '+')
		{
			// positive response
			char *Eol = strchr(Buffer, '\n');
			if (Eol)
			{
				char *Ptr = Eol + 1;
				ReadLen -= Ptr-Buffer;
				memmove(Buffer, Ptr, ReadLen);
				Temp.Write((uchar*) Buffer, ReadLen);

				while (!MailIsEnd(Buffer, ReadLen))
				{
					ReadLen = Socket->Read(Buffer, sizeof(Buffer), 0);
					if (ReadLen > 0)
					{
						Temp.Write((uchar*) Buffer, ReadLen);
					}
					else break;
				}

				int Len = (int)Temp.GetSize();
				Str = new char[Len+1];
				if (Str)
				{
					Temp.Read((uchar*)Str, Len);
					Str[Len] = 0;
					Status = true;
				}
			}
		}
	}

	return Status;
}

bool MailPop3::Close()
{
	if (Socket)
	{
		// logout
		VERIFY_RET_VAL(Write("QUIT\r\n", true));
		
		// 2 sec timeout, we don't really care about the server's response
		Socket->SetTimeout(2000);
		ReadReply();

		if (SocketLock.Lock(_FL))
		{
			Socket.Reset(0);
			SocketLock.Unlock();
		}
		Messages = 0;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/*
MailMessage::MailMessage()
{
	Log = this;
	
	From = 0;
	Reply = 0;
	InternetHeader = 0;
	Priority = MAIL_PRIORITY_NORMAL;

	Text = 0;
	TextCharset = 0;
	Html = 0;
	HtmlCharset = 0;

	MarkColour = -1;
	DispositionNotificationTo = false;
	Raw = 0;
}

MailMessage::~MailMessage()
{
	Empty();
	DeleteObj(From);
	DeleteObj(Reply);
	DeleteObj(Raw);
}

int MailMessage::Write(const void *Ptr, int Size, int Flags)
{
	LgiTrace("%.*s", Size, Ptr);
	return Size;
}

void MailMessage::Empty()
{
	Subject.Reset();
	MessageID.Reset();
	References.Reset();
	FwdMsgId.Reset();
	BounceMsgId.Reset();

	DeleteArray(InternetHeader);
	DeleteArray(Text);
	DeleteArray(TextCharset);
	DeleteArray(Html);
	DeleteArray(HtmlCharset);

	To.DeleteObjects();
	FileDesc.DeleteObjects();
}

char *MailMessage::GetBody()
{
	return Text;
}

char *MailMessage::GetBodyCharset()
{
	return TextCharset;
}

bool MailMessage::SetBodyCharset(const char *Cs)
{
	DeleteArray(TextCharset);
	TextCharset = NewStr(Cs);
	return true;
}

bool MailMessage::SetBody(const char *Txt, int Bytes, bool Copy, const char *Cs)
{
	if (Txt != Text)
	{
		DeleteArray(Text);
		Text = Copy ? NewStr(Txt, Bytes) : (char*)Txt;
		if (Txt && !Text)
			return false;
	}
	if (Cs != TextCharset)
	{
		DeleteArray(TextCharset);
		if (!(TextCharset = NewStr(Cs)))
			return false;
	}
	return true;
}

char *MailMessage::GetHtml()
{
	return Html;
}

char *MailMessage::GetHtmlCharset()
{
	return HtmlCharset;
}

bool MailMessage::SetHtmlCharset(const char *Cs)
{
	DeleteArray(HtmlCharset);
	HtmlCharset = NewStr(Cs);
	return true;
}

bool MailMessage::SetHtml(const char *Txt, int Bytes, bool Copy, const char *Cs)
{
	if (Txt != Html)
	{
		DeleteArray(Html);
		Html = Copy ? NewStr(Txt, Bytes) : (char*)Txt;
		if (Txt && !Html)
			return false;
	}
	if (Cs != HtmlCharset)
	{
		DeleteArray(HtmlCharset);
		if (!(HtmlCharset = NewStr(Cs)))
			return false;
	}
	return true;
}

int MailMessage::EncodeBase64(GStreamI &Out, GStreamI &In)
{
	int64 Start = LgiCurrentTime();
	int Status = 0;
	int BufSize = 4 << 10;
	char *InBuf = new char[BufSize];
	char *OutBuf = new char[BufSize];
	if (InBuf && OutBuf)
	{
		int InLen = (int)In.GetSize(); // file remaining to read
		int InUsed = 0;
		int InDone = 0;
		int OutUsed = 0;

		do
		{
			if (InUsed - InDone < 256 && InLen > 0)
			{
				// Move any bit left over down to the start
				memmove(InBuf, InBuf + InDone, InUsed - InDone);
				InUsed -= InDone;
				InDone = 0;

				// Read in as much data as we can
				int Max = min(BufSize-InUsed, InLen);
				int r = In.Read(InBuf + InUsed, Max);
				if (r <= 0)
					break;

				// FilePos += r;
				InUsed += r;
				InLen -= r;
			}

			if (OutUsed > BufSize - 256)
			{
				int w = Out.Write(OutBuf, OutUsed);
				if (w > 0)
				{
					OutUsed = 0;
					Status += w;
				}
				else
				{
					break;
				}
			}

			int OutLen = ConvertBinaryToBase64(	OutBuf + OutUsed,
												76,
												(uchar*)InBuf + InDone,
												InUsed - InDone);
			int In = OutLen * 3 / 4;
			InDone += In;
			OutUsed += OutLen;
			OutBuf[OutUsed++] = '\r';
			OutBuf[OutUsed++] = '\n';
		}
		while (InDone < InUsed);

		if (OutUsed > 0)
		{
			int w = Out.Write(OutBuf, OutUsed);
			if (w >= 0) Status += w;
			w = Out.Write((char*)"\r\n", 2);
			if (w >= 0) Status += w;
		}

		#if 0
		double Sec = (double)((int64)LgiCurrentTime() - Start) / 1000.0;
		double Kb = (double)FileDes->Sizeof() / 1024.0;
		LgiTrace("rate: %ikb/s\n", (int)(Kb / Sec));
		#endif
	}
	else Log->Print("%s:%i - Error allocating buffers\n", _FL);

	DeleteArray(InBuf);
	DeleteArray(OutBuf);
				
	return Status;
}

int MailMessage::EncodeQuotedPrintable(GStreamI &Out, GStreamI &In)
{
	int Status = 0;
	char OutBuf[100], InBuf[1024];
	int ch = 0;
	int InLen;

	// Read the input data one chunk at a time
	while ((InLen = In.Read(InBuf, sizeof(InBuf))) > 0)
	{
		// For all the input bytes we just got
		for (char *s = InBuf; s - InBuf < InLen; )
		{
			if (*s == '\n')
			{
				ch += sprintf_s(OutBuf+ch, sizeof(OutBuf)-ch, "\r\n");
				int w = Out.Write(OutBuf, ch);
				if (w <= 0)
					break;

				ch = 0;
				Status += w;
			}
			else if (*s == '.')
			{
				// If the '.' character happens to fall at the
				// end of a paragraph and gets pushed onto the next line it
				// forms the magic \r\n.\r\n sequence that ends an SMTP data
				// session. Which is bad. The solution taken here is to 
				// hex encode it if it falls at the start of the line.
				// Otherwise allow it through unencoded.

				if (ch == 0)
				{
					ch += sprintf_s(OutBuf+ch, sizeof(OutBuf)-ch, "=%2.2X", (uchar)*s);
				}
				else
				{
					OutBuf[ch++] = *s;
				}
			}
			else if (*s & 0x80 || *s == '=')
			{
				// Require hex encoding of 8-bit chars and the equals itself.
				ch += sprintf_s(OutBuf+ch, sizeof(OutBuf)-ch, "=%2.2X", (uchar)*s);
			}
			else if (*s != '\r')
			{
				OutBuf[ch++] = *s;
			}
			s++;

			if (ch > 73)
			{
				// time for a new line.
				ch += sprintf_s(OutBuf+ch, sizeof(OutBuf)-ch, "=\r\n");
				int w = Out.Write(OutBuf, ch);
				if (w <= 0)
					break;
				
				ch = 0;
				Status += w;
			}
		}
	}

	ch += sprintf_s(OutBuf+ch, sizeof(OutBuf)-ch, "\r\n");
	int w = Out.Write(OutBuf, ch);
	if (w > 0)
		Status += w;

	return Status;
}

int MailMessage::EncodeText(GStreamI &Out, GStreamI &In)
{
	int Status = 0;
	char InBuf[4096];
	int InLen, InUsed = 0;
	const char *Eol = "\r\n";

	while ((InLen = In.Read(InBuf+InUsed, sizeof(InBuf)-InUsed)) > 0)
	{
		InUsed += InLen;

		char *s;
		for (s = InBuf; s - InBuf < InUsed; )
		{
			// Do we have a complete line?
			int RemainingBytes = InUsed - (s - InBuf);
			char *NewLine = strnchr(s, '\n', RemainingBytes);
			if (NewLine)
			{
				// Yes... write that out.
				int Len = NewLine - s;

				if (Len > 0 && s[Len-1] == '\r') Len--;
				if (Len == 1 && s[0] == '.')
				{
					// this removes the sequence ".\n"
					// which is the END OF MAIL in the SMTP protocol.
					int w = Out.Write((char*)". ", 2);
					if (w <= 0)
						break;
					Status += w;
				}
				else if (Len)
				{
					int w = Out.Write(s, Len);
					if (w <= 0)
						break;
					Status += w;
				}
					
				int w = Out.Write(Eol, 2);
				if (w <= 0)
					break;
				s = NewLine + 1;
				Status += w;
			}
			else
			{
				// No... move the data down to the start of the buffer
				memmove(InBuf, s, RemainingBytes);
				InUsed = RemainingBytes;
				s = 0;
				break;
			}
		}

		if (s)
			InUsed -= s - InBuf;
	}

	if (InUsed)
	{
		int w = Out.Write(InBuf, InUsed);
		if (w > 0) Status += w;
		w = Out.Write(Eol, 2);
		if (w > 0) Status += w;
	}

	return Status;
}

#define SEND_BUF_SIZE		(16 << 10)

class SendBuf : public GStream
{
	GStreamI *Out;
	int Used;
	uchar Buf[SEND_BUF_SIZE];

public:
	bool Status;

	SendBuf(GStreamI *out)
	{
		Out = out;
		Used = 0;
		Status = true;
	}

	~SendBuf()
	{
		Flush();
	}

	int64 GetSize() { return Used; }
	int64 SetSize(int64 Size) { return Out->SetSize(Size); }
	int Read(void *Buffer, int Size, int Flags = 0) { return -1; }

	void Flush()
	{
		if (Used > 0)
		{
			int w = Out->Write(Buf, Used, 0);
			if (w < Used)
			{
				Status = false;
			}
			Used = 0;
		}
	}

	int Write(const void *Buffer, int Size, int Flags = 0)
	{
		int64 w = 0;

		uchar *Ptr = (uchar*)Buffer;
		while (Ptr && Size > 0)
		{
			if (Size + Used >= SEND_BUF_SIZE)
			{
				int Chunk = SEND_BUF_SIZE - Used;
				memcpy(Buf + Used, Ptr, Chunk);
				int s = Out->Write(Buf, SEND_BUF_SIZE, 0);
				if (s < SEND_BUF_SIZE)
				{
					return -1;
					break;
				}

				Ptr += Chunk;
				Size -= Chunk;
				w += Chunk;
				Used = 0;
			}
			else
			{
				memcpy(Buf + Used, Ptr, Size);
				Used += Size;
				w += Size;
				Size = 0;
			}
		}

		return (int)w;
	}
};

// Encode the whole email
bool MailMessage::Encode(GStreamI &Out, GStream *HeadersSink, MailProtocol *Protocol, bool Mime)
{
	GStringPipe p;
	bool Status = EncodeHeaders(p, Protocol, Mime);
	if (Status)
	{
		int Len = (int)p.GetSize();
		char *Headers = p.NewStr();
		if (HeadersSink)
		{
			HeadersSink->Write(Headers, Len);
		}
		else
		{
			InternetHeader = NewStr(Headers);
		}

		if (Headers && Out.Write(Headers, Len))
		{
			SendBuf *Buf = new SendBuf(&Out);
			if (Buf)
			{
				Status = EncodeBody(*Buf, Protocol, Mime);			
				if (Status)
				{
					Buf->Flush();
					Status = Buf->Status;
					if (!Status)
						Log->Print("%s:%i - Buffer status failed.\n", _FL);
				}
				else Log->Print("%s:%i - EncodeBody failed.\n", _FL);

				DeleteObj(Buf);
			}
		}
		else Log->Print("%s:%i - Headers output failed.\n", _FL);
		
		DeleteArray(Headers);
	}
	else Log->Print("%s:%i - EncodeHeaders failed.\n", _FL);

	return Status;
}

#define WriteOutput() \
	if (Out.Write(Buffer, Len) != Len) \
	{ \
		Log->Print("%s:%i - Write failed.\n", _FL); \
		Status = false; \
	}

// This encodes the main headers but not the headers relating to the 
// actual content. Thats done by the ::EncodeBody function.
bool MailMessage::EncodeHeaders(GStreamI &Out, MailProtocol *Protocol, bool Mime)
{
	bool Status = true;

	// Setup
	char Buffer[1025];

	// Construct date
	const char *Weekday[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	const char *Month[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	LDateTime Dt;
	int TimeZone = Dt.SystemTimeZone();
	Dt.SetNow();
	int Len = sprintf_s(Buffer, sizeof(Buffer),
						"Date: %s, %i %s %i %i:%2.2i:%2.2i %s%2.2d%2.2d\r\n",
						Weekday[Dt.DayOfWeek()],
						Dt.Day(),
						Month[Dt.Month()-1],
						Dt.Year(),
						Dt.Hours(),
						Dt.Minutes(),
						Dt.Seconds(),
						(TimeZone >= 0) ? "+" : "",
						TimeZone / 60,
						abs(TimeZone) % 60);
	WriteOutput();

	if (Protocol && Protocol->ProgramName)
	{
		// X-Mailer:
		Len = sprintf_s(Buffer, sizeof(Buffer), "X-Mailer: %s\r\n", Protocol->ProgramName.Get());
		WriteOutput();
	}

	if (Protocol && Protocol->ExtraOutgoingHeaders)
	{
		for (char *s=Protocol->ExtraOutgoingHeaders; s && *s; )
		{
			char *e = s;
			while (*e && *e != '\r' && *e != '\n') e++;
			int l = e-s;
			if (l > 0)
			{
				Status &= Out.Write(s, l) > 0;
				Status &= Out.Write((char*)"\r\n", 2) > 0;
			}
			while (*e && (*e == '\r' || *e == '\n')) e++;
			s = e;
		}
		if (!Status)
			Log->Print("%s:%i - Writing ExtraOutgoingHeaders failed.\n", _FL);
	}

	if (Priority != MAIL_PRIORITY_NORMAL)
	{
		// X-Priority:
		Len = sprintf_s(Buffer, sizeof(Buffer), "X-Priority: %i\r\n", Priority);
		WriteOutput();
	}

	if (MarkColour >= 0)
	{
		// X-Color (HTML Colour Ref for email marking)
		Len = sprintf_s(Buffer, sizeof(Buffer), 
			"X-Color: #%2.2X%2.2X%2.2X\r\n",
			R24(MarkColour),
			G24(MarkColour),
			B24(MarkColour));
		WriteOutput();
	}

	// Message-ID:
	if (MessageID)
	{
		for (char *m=MessageID; *m; m++)
		{
			if (*m <= ' ')
			{
				printf("%s:%i - Bad message ID '%s'\n", _FL, MessageID.Get());
				return false;
			}			
		}
		
		Len = sprintf_s(Buffer, sizeof(Buffer), "Message-ID: %s\r\n", MessageID.Get());
		WriteOutput();
	}

	// References:
	if (ValidStr(References))
	{
		char *Dir = strrchr(References, '/');
		GAutoString a;
		char *Ref = 0;
		if (Dir)
		{
			GUri u;
			a = u.Decode(Dir + 1);
			Ref = a;
		}
		else
			Ref = References;

		int Len = sprintf_s(Buffer, sizeof(Buffer), "References: <%s>\r\n", Ref);
		WriteOutput();
	}

	// To:
	char *ToAddr = CreateAddressTag(To, 0, &Protocol->CharsetPrefs);
	if (ToAddr)
	{
		Status &= Out.Write(ToAddr, strlen(ToAddr)) > 0;
		DeleteArray(ToAddr);
		if (!Status)
			Log->Print("%s:%i - Writing ToAddr failed.\n", _FL);
	}

	char *CcAddr = CreateAddressTag(To, 1, &Protocol->CharsetPrefs);
	if (CcAddr)
	{
		Status &= Out.Write(CcAddr, strlen(CcAddr)) > 0;
		DeleteArray(CcAddr);
		if (!Status)
			Log->Print("%s:%i - Writing CcAddr failed.\n", _FL);
	}

	// From:
	if (From && From->Addr)
	{
		Len = sprintf_s(Buffer, sizeof(Buffer), "From: ");

		char *Nme = EncodeRfc2047(NewStr(From->Name), 0, &Protocol->CharsetPrefs);
		if (Nme)
		{
			if (strchr(Nme, '\"'))
				Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, "'%s' ", Nme);
			else
				Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, "\"%s\" ", Nme);
			DeleteArray(Nme);
		}

		Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, "<%s>\r\n", From->Addr);
		WriteOutput();
	}
	else
	{
		Log->Print("%s:%i - No 'from' address to send email.\n", _FL);
		return false;
	}

	// Reply-To:
	if (Reply &&
		ValidStr(Reply->Addr))
	{
		Len = sprintf_s(Buffer, sizeof(Buffer), "Reply-To: ");

		char *Nme = EncodeRfc2047(NewStr(Reply->Name), 0, &Protocol->CharsetPrefs);
		if (Nme)
		{
			if (strchr(Nme, '\"'))
				Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, "'%s' ", Nme);
			else
				Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, "\"%s\" ", Nme);
			DeleteArray(Nme);
		}

		Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, "<%s>\r\n", Reply->Addr);
		WriteOutput();
	}

	// Subject:
	char *Subj = EncodeRfc2047(NewStr(Subject), 0, &Protocol->CharsetPrefs, 9);
	Len = sprintf_s(Buffer, sizeof(Buffer), "Subject: %s\r\n", (Subj) ? Subj : "");
	WriteOutput();
	DeleteArray(Subj);

	// DispositionNotificationTo
	if (DispositionNotificationTo)
	{
		Len = sprintf_s(Buffer, sizeof(Buffer), "Disposition-Notification-To:");
		char *Nme = EncodeRfc2047(NewStr(From->Name), 0, &Protocol->CharsetPrefs);
		if (Nme)
		{
			Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, " \"%s\"", Nme);
			DeleteArray(Nme);
		}
		Len += sprintf_s(Buffer+Len, sizeof(Buffer)-Len, " <%s>\r\n", From->Addr);
		WriteOutput();
	}

	return Status;
}

bool MailMessage::EncodeBody(GStreamI &Out, MailProtocol *Protocol, bool Mime)
{
	bool Status = true;
	char Buffer[1025];

	if (Mime)
	{
		bool MultiPart = ((Text ? 1 : 0) + (Html ? 1 : 0) + FileDesc.Length()) > 1;
		bool MultipartAlternate = ValidStr(Text) && ValidStr(Html);
		bool MultipartMixed = FileDesc.Length() > 0;
		uint64 Now = LgiCurrentTime();

		char Separator[256];
		sprintf_s(Separator, sizeof(Separator), "----=_NextPart_%8.8X.%8.8X",
				(uint32)Now,
				(unsigned)(int64)LgiGetCurrentThread());

		int Len = sprintf_s(Buffer, sizeof(Buffer), "MIME-Version: 1.0\r\n");
		Status &= Out.Write(Buffer, Len) > 0;
		
		if (MultiPart)
		{
			const char *Type =	MultipartMixed
								?
								EncryptedMsg ? sMultipartEncrypted : sMultipartMixed
								:
								sMultipartAlternative;
			Len = sprintf_s(Buffer, sizeof(Buffer), "Content-Type: %s;\r\n\tboundary=\"%s\"\r\n", Type, Separator);
			Status &= Out.Write(Buffer, Len) > 0;
		}

		if (ValidStr(Text) || ValidStr(Html))
		{
			char AlternateBoundry[128] = "";

			if (MultiPart)
			{
				Len = sprintf_s(Buffer, sizeof(Buffer), "\r\n--%s\r\n", Separator);
				Status &= Out.Write(Buffer, Len) > 0;

				if (MultipartMixed && MultipartAlternate)
				{
					sprintf_s(AlternateBoundry, sizeof(AlternateBoundry), "----=_NextPart_%8.8X.%8.8X", (uint32)++Now, (uint32)(int64)LgiGetCurrentThread());
					Len = sprintf_s(Buffer, sizeof(Buffer),
									"Content-Type: %s;\r\n\tboundary=\"%s\"\r\n",
									sMultipartAlternative,
									AlternateBoundry);
					Status &= Out.Write(Buffer, Len) > 0;

					Len = sprintf_s(Buffer, sizeof(Buffer), "\r\n--%s\r\n", AlternateBoundry);
					Status &= Out.Write(Buffer, Len) > 0;
				}
			}

			if (!Status)
				Log->Print("%s:%i - Status=%i\n", _FL, Status);

			if (ValidStr(Text))
			{
				const char *Cs = 0;
				char *Txt = Text, *Mem = 0;
				
				// Detect charset
				if (!TextCharset || _stricmp(TextCharset, "utf-8") == 0)
				{
					Cs = LgiDetectCharset(Text, -1, Protocol ? &Protocol->CharsetPrefs : 0);
					if (Cs)
					{
						Mem = Txt = (char*)LgiNewConvertCp(Cs, Text, "utf-8", -1);
					}
				}
				if (!Cs)
					Cs = TextCharset;
				
				// Content type
				Len = sprintf_s(Buffer, sizeof(Buffer),
								"Content-Type: text/plain; charset=\"%s\"\r\n",
								Cs ? Cs : "us-ascii");
				Status &= Out.Write(Buffer, Len) > 0;

				if (!Status)
					Log->Print("%s:%i - Status=%i\n", _FL, Status);

				// Transfer encoding
				if (Txt &&
					(Is8Bit(Txt) || MaxLineLen(Txt) >= 80))
				{
					char QuotPrint[] = "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
					Status &= Out.Write(QuotPrint, strlen(QuotPrint)) > 0;

					if (!Status)
						Log->Print("%s:%i - Status=%i\n", _FL, Status);

					GMemStream TxtStr(Txt, strlen(Txt), false);
					Status &= EncodeQuotedPrintable(Out, TxtStr) > 0;
					
					if (!Status)
						Log->Print("%s:%i - Status=%i\n", _FL, Status);
				}
				else
				{
					char Cte[] = "Content-Transfer-Encoding: 7bit\r\n\r\n";
					Status &= Out.Write(Cte, strlen(Cte)) > 0;

					if (!Status)
						Log->Print("%s:%i - Status=%i\n", _FL, Status);

					GMemStream TxtStr(Txt, strlen(Txt), false);
					Status &= EncodeText(Out, TxtStr) > 0;

					if (!Status)
						Log->Print("%s:%i - Status=%i\n", _FL, Status);
				}
				
				DeleteArray(Mem);
			}

			if (!Status)
				Log->Print("%s:%i - Status=%i\n", _FL, Status);

			// Break alternate part
			if (AlternateBoundry[0])
			{
				Len = sprintf_s(Buffer, sizeof(Buffer), "--%s\r\n", AlternateBoundry);
				Status &= Out.Write(Buffer, Len) > 0;
			}
			else if (MultipartAlternate)
			{
				Len = sprintf_s(Buffer, sizeof(Buffer), "--%s\r\n", Separator);
				Status &= Out.Write(Buffer, Len) > 0;
			}

			if (ValidStr(Html))
			{
				// Content type
				Len = sprintf_s(Buffer, sizeof(Buffer), 
								"Content-Type: text/html; charset=\"%s\"\r\n",
								TextCharset ? TextCharset : "us-ascii");
				Status &= Out.Write(Buffer, Len) > 0;

				// Transfer encoding
				if (Is8Bit(Html) ||
					MaxLineLen(Html) >= 80)
				{
					char Qp[] = "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
					Status &= Out.Write(Qp, strlen(Qp)) > 0;

					GMemStream HtmlStr(Html, strlen(Html), false);
					Status &= EncodeQuotedPrintable(Out, HtmlStr) > 0;
				}
				else
				{
					char Sb[] = "Content-Transfer-Encoding: 7bit\r\n\r\n";
					Status &= Out.Write(Sb, strlen(Sb)) > 0;

					GMemStream HtmlStr(Html, strlen(Html), false);
					Status &= EncodeText(Out, HtmlStr) > 0;
				}
			}

			if (!Status)
				Log->Print("%s:%i - Status=%i\n", _FL, Status);

			if (AlternateBoundry[0])
			{
				// End alternate part
				Len = sprintf_s(Buffer, sizeof(Buffer), "\r\n--%s--\r\n", AlternateBoundry);
				Status &= Out.Write(Buffer, Len) > 0;
			}
		}

		if (!Status)
			Log->Print("%s:%i - Status=%i\n", _FL, Status);

		int SizeEst = 1024;
		FileDescriptor *FileDes = FileDesc.First();
		for (; FileDes; FileDes=FileDesc.Next())
		{
			SizeEst += FileDes->Sizeof() * 4 / 3;
		}
		Out.SetSize(SizeEst);

		FileDes = FileDesc.First();
		while (FileDes)
		{
			GStreamI *F = FileDes->GotoObject();

			// write a MIME segment for this attachment
			if (MultiPart)
			{
				Len = sprintf_s(Buffer, sizeof(Buffer), "--%s\r\n", Separator);
				Status &= Out.Write(Buffer, Len) > 0;
			}

			char FileName[256];
			char *s = FileDes->Name(), *d = FileName;
			if (!s)
				Log->Print("%s:%i - File descriptor has no name.\n", _FL);
			else
			{
				while (*s)
				{
					if (*s != '\\')
					{
						*d++ = *s++;
					}
					else
					{
						*d++ = '\\';
						*d++ = '\\';
						s++;
					}
				}
				*d = 0;

				char *FName = EncodeRfc2047(NewStr(FileName), 0, &Protocol->CharsetPrefs);
				Len = sprintf_s(Buffer, sizeof(Buffer), 
								"Content-Type: %s; name=\"%s\"\r\n"
								"Content-Disposition: attachment\r\n",
								FileDes->GetMimeType() ? FileDes->GetMimeType() : "application/x-zip-compressed",
								(FName) ? FName : FileName);
				Status &= Out.Write(Buffer, Len) > 0;
				DeleteArray(FName);

				if (FileDes->GetContentId())
				{
					Len = sprintf_s(Buffer, sizeof(Buffer), "Content-Id: %s\r\n", FileDes->GetContentId());
					Status &= Out.Write(Buffer, Len) > 0;
				}

				Len = sprintf_s(Buffer, sizeof(Buffer), "Content-Transfer-Encoding: base64\r\n\r\n");
				Status &= Out.Write(Buffer, Len) > 0;
				Status &= F ? EncodeBase64(Out, *F) > 0 : false;
			}

			FileDes = FileDesc.Next();
		}

		if (!Status)
			Log->Print("%s:%i - Status=%i\n", _FL, Status);

		if (MultiPart)
		{
			// write final separator
			Len = sprintf_s(Buffer, sizeof(Buffer), "--%s--\r\n\r\n", Separator);
			Status &= Out.Write(Buffer, Len) > 0;
		}
	}
	else
	{
		// send content type
		int Len = sprintf_s(Buffer, sizeof(Buffer), "Content-Type: text/plain; charset=\"%s\"\r\n", TextCharset ? TextCharset : "us-ascii");
		Status &= Out.Write(Buffer, Len) > 0;

		if (Is8Bit(Text))
		{
			// send the encoding and a blank line
			char Qp[] = "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
			Status &= Out.Write(Qp, strlen(Qp)) > 0;
			
			// send message text
			GMemStream TextStr(Text, strlen(Text), false);
			Status &= EncodeQuotedPrintable(Out, TextStr) > 0;
		}
		else
		{
			// send a blank line
			Status &= Out.Write((char*)"\r\n", 2) > 0;
			
			// send message text
			GMemStream TextStr(Text, strlen(Text), false);
			Status &= EncodeText(Out, TextStr) > 0;
		}
	}

	if (!Status)
		Log->Print("%s:%i - Status=%i\n", _FL, Status);

	return true;
}

*/