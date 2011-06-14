/*hdr
**      FILE:           Mail.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           28/5/98
**      DESCRIPTION:    Mail app
**
**      Copyright (C) 1998, Matthew Allen
**              fret@ozemail.com.au
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
#include "GDateTime.h"

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
			Max = max(i, Max);
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
			char *e = s;
			for (; *e; e++)
			{
				if (strchr("\'\"", *e))
				{
					// handle string constant
					char EndChar = *e++;
					while (*e && *e != EndChar) e++;
				}

				if (strchr(Delim, *e))
				{
					break;
				}
			}

			int Len = e - s;
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

			s = e;
			for (; *s && strchr(Delim, *s); s++);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
char *DecodeBase64Str(char *Str, int Len)
{
	if (Str)
	{
		int B64Len = (Len < 0) ? strlen(Str) : Len;
		int BinLen = BufferLen_64ToBin(B64Len);
		char *s = new char[BinLen+1];
		if (s)
		{
			int Converted = ConvertBase64ToBinary((uchar*)s, BinLen, Str, B64Len);
			s[Converted] = 0;
			DeleteArray(Str);
			Str = s;
		}
	}
	return Str;
}

char *DecodeQuotedPrintableStr(char *Str, int Len)
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
	if (Str && strstr(Str, "=?"))
	{
		GBytePipe Temp;
		char *s = Str;
		while (*s)
		{
			// is there a word remaining
			bool Encoded = false;
			char *p = strstr(s, "=?");
			char *First = 0;
			char *Second = 0;
			char *End = 0;
			char *Cp = 0;
			if (p)
			{
				char *CpStart = p + 2;
				
				First = strchr(CpStart, '?');
				if (First)
				{
					Cp = NewStr(CpStart, First-CpStart);
					Second = strchr(First+1, '?');
					if (Second)
					{
						End = strstr(Second+1, "?=");
						Encoded = End != 0;
					}
				}
			}

			if (Encoded)
			{
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
					char *Block = NewStr(Second+1, End-Second-1);
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

						int Len = strlen(Block);
						if (StripUnderscores)
						{
							for (int i=0; i<Len; i++)
							{
								if (Block[i] == '_') Block[i] = ' ';
							}
						}

						char *c;
						for (c=s; c<p; c++)
						{
							if (!strchr(WhiteSpace, *c))
							{
								break;
							}
						}
						if (c < p)
						{
							Temp.Write((uchar*)s, p-s);
						}
						
						char *Utf8 = (char*)LgiNewConvertCp("utf-8", Block, Cp);
						if (Utf8)
						{
							Temp.Write((uchar*)Utf8, strlen(Utf8));
							DeleteArray(Utf8);
						}
						else
						{
							Temp.Write((uchar*)Block, Len);
						}

						DeleteArray(Block);
					}
					s = End + 2;
					if (*s == '\n')
					{
						s++;
						while (*s && strchr(WhiteSpace, *s)) s++;
					}
				}
			}

			if (!Encoded)
			{
				int Len = strlen(s);
				Temp.Write((uchar*) s, Len);
				s += Len;
			}

			DeleteArray(Cp);
		}

		Temp.Write((uchar*) "", 1);
		int Len = Temp.GetSize();
		char *r = new char[Len+1];
		if (r)
		{
			Temp.Read((uchar*) r, Len);
			r[Len] = 0;
		}

		DeleteArray(Str);
		return r;
	}

	return Str;
}

#define MIME_MAX_LINE		76

char *EncodeRfc2047(char *Str, const char *CodePage, List<char> *CharsetPrefs, int LineLength)
{
	if (!CodePage)
	{
		CodePage = "utf-8";
	}

	if (Str &&
		Is8Bit(Str))
	{
		GStringPipe p(128);

		// pick an encoding
		bool Base64 = false;
		const char *DestCp = "utf-8";
		int Len = strlen(Str);;
		if (stricmp(CodePage, "utf-8") == 0)
		{
			DestCp = LgiDetectCharset(Str, Len, CharsetPrefs);
		}

		int Chars = 0;
		for (int i=0; i<Len; i++)
		{
			if (Str[i] & 0x80) Chars++;
		}
		if (stristr(DestCp, "utf") || (((double)Chars/Len)  > 0.4))
		{
			Base64 = true;
		}

		char *Buf = (char*)LgiNewConvertCp(DestCp, Str, CodePage, Len);
		if (Buf)
		{
			// encode the word
			char Prefix[64];
			int Ch = sprintf(Prefix, "=?%s?%c?", DestCp, Base64 ? 'B' : 'Q');
			p.Write(Prefix, Ch);
			LineLength += Ch;

			if (Base64)
			{
				// Base64
				int InLen = strlen(Buf);
				int EstBytes = BufferLen_BinTo64(InLen);

				char Temp[512];
				int Bytes = ConvertBinaryToBase64(Temp, sizeof(Temp), (uchar*)Buf, InLen);
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
						Ch = sprintf(Temp, "=%2.2X", (uchar)*w);
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

	return Str;
}

//////////////////////////////////////////////////////////////////////////////
void DeNullText(char *in, int &len)
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
typedef char CharPair[2];

GAutoString RemovePairs(char *Str, CharPair *Pairs)
{
	char *s = Str;
	while (*s && strchr(WhiteSpace, *s)) s++;

	if (!*s)
		return GAutoString();

	// Get the end of the string...
	char *e = s + strlen(s);

	// Seek back over any trailing whitespace
	while (e > s && strchr(WhiteSpace, e[-1]))
		*e--;

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

	int Len = e - s;
	if (Len < 0)
		return GAutoString();

	return GAutoString(NewStr(s, Len));
}

bool IsValidEmail(char *email, GAutoString *out)
{
	// Local part
	char *e = email;
	char buf[321];
	char *o = buf;

	if (!e || *e == '.')
		return false;

	#define OutputChar()	\
		if (o - buf >= sizeof(buf) - 1)	\
			return false;	\
		*o++ = *e++

	// Local part
	while (*e)
	{
		if (strchr("!#$%&\'*+-/=?^_`.{|}~", *e) ||
			isalpha((uchar)*e) ||
			isdigit((uchar)*e))
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
		if (!isdigit(*e))
			return false;
		
		// Check the rest...
		char *Start = e;
		while (*e)
		{
			if (isdigit(*e) ||
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
		if (!isalpha(*e) && !isdigit(*e))
			return false;
		
		// Check the rest.
		while (*e)
		{
			if (isalpha(*e) ||
				isdigit(*e) ||
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
	if (out)
		out->Reset(NewStr(buf, o - buf));
	return true;
}

void DecodeAddrName(char *Start, GAutoString &Name, GAutoString &Addr, char *DefaultDomain)
{
	/* Testing code
	char *Input[] =
	{
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
		" Australian Bankers' Association (ABA)<survey@aawp.org.au>",
		0
	};

	GAutoString Name, Addr;
	for (char **i = Input; *i; i++)
	{
		DecodeAddrName(*i, Name, Addr, "name.com");
		LgiTrace("N=%-24s A=%-24s\n", Name, Addr);
	}
	*/

	if (!Start)
		return;

	GArray<char*> Str;
	for (char *c = Start; *c; )
	{
		if (strchr(WhiteSpace, *c))
		{
			// skip whitespace
		}
		else if (strchr("\"'", *c))
		{
			// string delim
			char End = *c++;
			char *s = c;
			for (c; *c && *c != End; c++)
			{
				if (*c == '\\')
					c++;
			}

			Str.Add(NewStr(s, c-s));
		}
		else if (strchr("<(", *c))
		{
			// brackets
			char Delim = (*c == '<') ? '>' : ')';
			char *End = strchr(c + 1, Delim);
			if (End)
			{
				End++;
			}
			else
			{
				End = c + 1;
				while (*End && *End != ' ')
					End++;
			}
			Str.Add(NewStr(c, End-c));
			c = End;
		}
		else
		{
			// Some string
			char *s = c;
			for (; *c && !strchr("<(\"", *c); c++);
			Str.Add(NewStr(s, c-s));
			continue;
		}

		if (*c)
			c++;
		else
			break;
	}

	CharPair Pairs[] =
	{
		{'<', '>'},
		{'(', ')'},
		{'\'', '\''},
		{'\"', '\"'},
		{0, 0},
	};
	
	// Process the email address
	int i;
	for (i=0; i<Str.Length(); i++)
	{
		GAutoString Parsed, Trimmed = RemovePairs(Str[i], Pairs);
		if (IsValidEmail(Trimmed, &Parsed))
		{
			Addr = Parsed;
			DeleteArray(Str[i]);
			Str.DeleteAt(i, true);
			break;
		}
	}

	if (!Addr)
	{
		// This code checks through the strings again for
		// something bracketed by <> which would normally
		// be the address, even if it's not formatted correctly
		// Scribe uses this to store group names in the email
		// address part of an address descriptor.
		for (i=0; i<Str.Length(); i++)
		{
			char *s = Str[i];
			if (*s == '<' && !strchr(s, '@'))
			{
				Addr = RemovePairs(Str[i], Pairs);
				DeleteArray(Str[i]);
				Str.DeleteAt(i, true);
			}
		}		
	}

	// Process the remaining parts into the name
	GStringPipe n(256);
	for (i=0; i<Str.Length(); i++)
	{
		GAutoString Name = RemovePairs(Str[i], Pairs);
		if (Name)
		{
			if (n.GetSize())
				n.Push(" ");

			// De-quote the string
			char *s = Name, *e;
			for (e = Name; e && *e; )
			{
				if (*e == '\\')
				{
					n.Write(s, e - s);
					s = ++e;
				}
				else
					e++;
			}
			n.Write(s, e - s);
		}
	}
	Name.Reset(n.NewStr());

	if (Name)
	{
		char *In = Name;
		char *Out = Name;
		while (*In)
		{
			if (!(*In == '\\' && *In == '\"'))
			{
				*Out++ = *In;
			}
			In++;
		}
		*Out = 0;
	}

	Str.DeleteArrays();
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

	Data = data ? new uchar[Size] : 0;
	if (Data)
	{
		memcpy(Data, data, Size);
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

void FileDescriptor::SetLock(GSemaphore *l)
{
	Lock = l;
}

GSemaphore *FileDescriptor::GetLock()
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
	return Size;
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

		if (Content != CONTENT_NONE);
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
					memcpy(Data, MimeData, Size);
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
				Data = new uchar[Size];
				if (Data)
				{
					int Converted = ConvertBase64ToBinary(Data, Size, Base64, OutCount);
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

void AddressDescriptor::Print(char *Str)
{
	if (Str)
	{
		if (Addr && Name)
		{
			sprintf(Str, "%s (%s)", Addr, Name);
		}
		else if (Addr)
		{
			strcpy(Str, Addr);
		}
		else if (Name)
		{
			sprintf(Str, "(%s)", Name);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailProtocol::MailProtocol()
{
	Buffer[0] = 0;
	Logger = 0;

	Items = 0;
	Transfer = 0;
	ProgramName = 0;
	DefaultDomain = 0;

	ExtraOutgoingHeaders = 0;
}

MailProtocol::~MailProtocol()
{
	CharsetPrefs.DeleteArrays();
	DeleteArray(ExtraOutgoingHeaders);
}

void MailProtocol::Log(const char *Str, COLOUR c)
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

		Logger->Write(s, o - s, c);
	}
}

bool MailProtocol::Error(const char *file, int line, const char *msg, ...)
{
	char s[1024];
	va_list a;
	va_start(a, msg);
	vsprintf(s, msg, a);
	va_end(a);

	Log(s, MAIL_ERROR_COLOUR);
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

bool MailProtocol::Write(char *Buf, bool LogWrite)
{
	bool Status = false;
	if (Socket)
	{
		char *p = (Buf) ? Buf : Buffer;
		Status = Socket->Write(p, strlen(p), 0);
		
		if (LogWrite)
		{
			Log(p, MAIL_SEND_COLOUR);
		}
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#define	VERIFY_RET_VAL(Arg)						\
{												\
	if (!Arg)									\
	{											\
		GSemaphore::Auto Lck(&SocketLock, _FL);	\
		Socket.Reset(0);						\
		return false;							\
	}											\
}

#define	VERIFY_ONERR(Arg)						\
{												\
	if (!Arg)									\
	{											\
		GSemaphore::Auto Lck(&SocketLock, _FL);	\
		Socket.Reset(0);						\
		goto CleanUp;							\
	}											\
}

MailSmtp::MailSmtp()
{
	ProgramName = 0;
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
		strcpy(Str, RemoteHost);
		char *Colon = strchr(Str, ':');
		if (Colon)
		{
			*Colon = 0;
			Colon++;
			Port = atoi(Colon);
		}
		if (Port == 0)
		{
			Port = SMTP_PORT;
		}

		char *Server = TrimStr(Str);
		if (Server)
		{
			if (SocketLock.Lock(_FL))
			{
				Socket.Reset(S);
				SocketLock.Unlock();
			}

			Socket->SetTimeout(30 * 1000);
			if (!Socket->Open(Server, Port))
				Error(_FL, "Failed to connect socket to %s:%i\n", Server, Port);
			else
			{
				GStringPipe Str;

				// receive signon message
				VERIFY_RET_VAL(ReadReply("220"));

				// Rfc 2554 ESMTP authentication
				SmtpHello:
				sprintf(Buffer, "EHLO %s\r\n", (ValidNonWSStr(LocalDomain)) ? LocalDomain : "default");
				VERIFY_RET_VAL(Write(0, true));
				bool HasSmtpExtensions = ReadReply("250", &Str);

				bool Authed = false;
				bool NoAuthTypes = false;
				bool SupportsStartTLS = false;
				GHashTable TheirAuthTypes;

				// Look through the response for the auth line
				char *Response = Str.NewStr();
				if (Response)
				{
					GToken Lines(Response, "\n");
					for (int i=0; i<Lines.Length(); i++)
					{
						char *l = Lines[i];
						char *AuthStr = strstr(l, "AUTH");
						if (AuthStr)
						{
							// walk through AUTH types
							GToken Types(AuthStr + 4, " ,;");
							for (int a=0; a<Types.Length(); a++)
							{
								TheirAuthTypes.Add(Types[a]);
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
					strcpy(Buffer, "STARTTLS\r\n");
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
					GHashTable MyAuthTypes(16);
					MyAuthTypes.Add("PLAIN");
					MyAuthTypes.Add("LOGIN");

					if (TheirAuthTypes.Length() == 0)
					{
						// No auth types? huh?
						if (TestFlag(Flags, MAIL_USE_AUTH))
						{
							if (TestFlag(Flags, MAIL_USE_PLAIN))
							{
								// Force plain type
								TheirAuthTypes.Add("PLAIN");
							}
							else if (TestFlag(Flags, MAIL_USE_LOGIN))
							{
								// Force login type
								TheirAuthTypes.Add("LOGIN");
							}
							else
							{
								// Oh well, we'll just try all types
								char *a;
								for (bool b=MyAuthTypes.First(&a); b; b=MyAuthTypes.Next(&a))
								{
									TheirAuthTypes.Add(a);
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
						uchar Tmp[256];
						ZeroObj(Tmp);
						char *a = (char*)Tmp + 1;
						strcpy(a, UserName);
						a += strlen(UserName) + 1;
						strcpy(a, Password);
						a += strlen(Password);
						int Len = a-(char*)Tmp;

						char B64[256];
						ZeroObj(B64);
						ConvertBinaryToBase64(B64, sizeof(B64), Tmp, Len);

						sprintf(Buffer, "AUTH PLAIN %s\r\n", B64);
						VERIFY_RET_VAL(Write(0, true));
						if (ReadReply("235"))
						{
							Authed = true;
						}
					}

					if (!Authed)
					{
						if (NoAuthTypes)
						{
							ServerMsg.Reset(NewStr(LgiLoadString(L_ERROR_ESMTP_NO_AUTHS,
															"The server didn't return the authentication methods it supports.")));
						}
						else
						{
							strcpy(Buffer, LgiLoadString(L_ERROR_ESMTP_UNSUPPORTED_AUTHS,
														"The server doesn't support any compatible authentication types:\n\t"));
							
							char *a = 0;
							for (void *p = TheirAuthTypes.First(&a); p; p = TheirAuthTypes.Next(&a))
							{
								int Bytes = strlen(Buffer);
								snprintf(Buffer + Bytes, sizeof(Buffer) - Bytes, "%s ", a);
							}
							ServerMsg.Reset(NewStr(Buffer));
						}
					}

					Status = Authed;
				}
				else
				{
					/*
					NormalLogin:
					// Normal SMTP login
					// send HELO message
					sprintf(Buffer, "HELO %s\r\n", (ValidNonWSStr(LocalDomain)) ? LocalDomain : "default");
					VERIFY_RET_VAL(Write(0, true));
					VERIFY_RET_VAL(ReadReply("250"));

					if (Flags & MAIL_SOURCE_STARTTLS)
					{
						if (SupportsStartTLS && TestFlag(Flags, MAIL_USE_STARTTLS))
						{
							strcpy(Buffer, "STARTTLS\r\n");
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
					}
					*/

					Status = true;
				}
			}

			DeleteArray(Server);
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
		GBytePipe Temp;

		const char *Start = Str;
		while (*Str)
		{
			if (*Str == '\n')
			{
				// send a string
				int Size = Str-Start;
				if (Str[-1] == '\r') Size--;
				Temp.Write((uchar*) Start, Size);
				Temp.Write((uchar*) "\r\n", 2);
				Start = Str + 1;
			}

			Str++;
		}

		// send the final string
		int Size = Str-Start;
		if (Str[-1] == '\r') Size--;
		Temp.Write((uchar*) Start, Size);

		Size = Temp.GetSize();
		char *Data = new char[Size];
		if (Data)
		{
			Temp.Read((uchar*) Data, Size);
			Status = Socket->Write(Data, Size, 0);
			DeleteArray(Data);
		}
	}

	return Status;
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

			if (a->Addr && strchr(a->Addr, ','))
			{
				// Multiple address format
				GToken t(a->Addr, ",");
				for (int i=0; i<t.Length(); i++)
				{
					sprintf(Buffer, "<%s>", t[i]);
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

				sprintf(Buffer, "%s <%s>", Name, a->Addr);

				DeleteArray(Mem);
			}
			else if (a->Addr)
			{
				// Just addr
				sprintf(Buffer, "<%s>", a->Addr);
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

	int Read(void *Ptr, int Size, int Flags)
	{
		return false;
	}
	
	int64 SetSize(int64 Size)
	{
		if (p)
		{
			p->Start = LgiCurrentTime();
			p->Range = Size;
			return Size;
		}

		return -1;
	}

	int Write(const void *Ptr, int Size, int Flags)
	{
		int w = s->Write((char*)Ptr, Size, 0);

		if (p && p->Range && w > 0)
		{
			p->Value += w;
		}

		return w;
	}
};

bool MailSmtp::SendToFrom(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	bool AddrOk = false;

	if (To.First() && From)
	{
		// send MAIL message
		if (From && ValidStr(From->Addr))
		{
			sprintf(Buffer, "MAIL FROM: <%s>\r\n", From->Addr);
		}
		else
		{
			ServerMsg.Reset(NewStr("No 'from' address in email."));
			return false;
		}

		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply("250", 0, Err));

		// send RCPT message
		AddrOk = true;
		List<AddressDescriptor>::I Recip = To.Start();
		for (AddressDescriptor *a = *Recip; a; a = *++Recip)
		{
			char *Addr = ValidStr(a->Addr) ? a->Addr : a->Name;
			if (ValidStr(Addr))
			{
				GToken Parts(Addr, ",");
				for (int p=0; p<Parts.Length(); p++)
				{
					sprintf(Buffer, "RCPT TO: <%s>\r\n", Parts[p]);
					VERIFY_RET_VAL(Write(0, true));
					a->Status = ReadReply("25", 0, Err);
					AddrOk &= a->Status != 0; // at least one address is ok
				}
			}
			else
			{
				LgiTrace("%s:%i - Send Addr wasn't valid\n", _FL);
			}
		}
	}

	return AddrOk;
}

GStringPipe *MailSmtp::SendData(MailProtocolError *Err)
{
	// send DATA message
	sprintf(Buffer, "DATA\r\n");
	VERIFY_RET_VAL(Write(0, true));
	VERIFY_RET_VAL(ReadReply("354", 0, Err));

	return new SocketPipe(Socket, Transfer);
}

GStringPipe *MailSmtp::SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	return SendToFrom(To, From, Err) ? SendData(Err) : 0;
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

bool MailSmtp::Close()
{
	if (Socket)
	{
		// send QUIT message
		sprintf(Buffer, "QUIT\r\n");
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply("221"));
		
		GSemaphore::Auto Lock(&SocketLock, _FL);
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
			int Len = Socket->Read(Buffer+Pos, sizeof(Buffer)-Pos, 0);
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
							Err->Msg = NewStr(Sp ? Sp + 1 : Start);
						}

						// Log
						Log(Start, atoi(Start) >= 400 ? MAIL_ERROR_COLOUR : MAIL_RECEIVE_COLOUR);

						// exit loop
						Pos = sizeof(Buffer);
						break;
					}
					else
					{
						Log(Start, MAIL_RECEIVE_COLOUR);

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
			ServerMsg.Reset(NewStr(Buffer));
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
			sprintf(n, "%u.mail", LgiRand());
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
				for (int n=0; n<Addrs.Length(); n++, i++)
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

	int Read(void *Buffer, int Size, int Flags = 0)
	{
		return F.Read(Buffer, Size, Flags);
	}

	int Write(const void *Buffer, int Size, int Flags = 0)
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

bool MailReceiveFolder::Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags)
{
	// We don't use the socket so just free it here...
	DeleteObj(S);

	// Argument check
	if (!DirExists(d->Path))
		return false;
	GAutoPtr<GDirectory> Dir(FileDev->GetDir());
	if (!Dir)
		return false;

	// Loop through files, looking for email
	for (bool b = Dir->First(d->Path, "*.*"); b; b = Dir->Next())
	{
		if
		(
			!Dir->IsDir()
			&&
			(
				MatchStr("*.eml", Dir->GetName())
				||
				MatchStr("*.mail", Dir->GetName())
			)
		)
		{
			for (bool b = Dir->First(d->Path, LGI_ALL_FILES); b; b = Dir->Next())
			{
				if (!Dir->IsDir())
				{
					if (MatchStr("*.eml", Dir->GetName()) ||
						MatchStr("*.mail", Dir->GetName()))
					{
						char p[300];
						Dir->Path(p, sizeof(p));
						d->Mail.Insert(new MailItem(p));
					}
				}
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
	return d->Mail.Length();
}

bool MailReceiveFolder::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	for (int i=0; i<Trans.Length(); i++)
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
		return LgiFileSize(m->File);
	}

	return 0;
}

bool MailReceiveFolder::GetUid(int Message, char *Id)
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
				int Len = e - s;
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
		if (GetUid(i, Uid))
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
	if (Messages == 0)
	{
		int asd=0;
	}

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
			int Result = Socket->Read(Buffer+Pos, sizeof(Buffer)-Pos, 0);
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
			Log(Buffer, (Status) ? MAIL_RECEIVE_COLOUR : MAIL_ERROR_COLOUR);
		if (Cr) *Cr = '\r';

		if (!Status)
		{
			ServerMsg.Reset(NewStr(Buffer));
		}
	}

	return Status;
}

bool MailPop3::ListCmd(char *Cmd, GHashTable &Results)
{
	sprintf(Buffer, "%s\r\n", Cmd);
	if (!Write(0, true))
		return false;

	char *b = Buffer;
	int r;
	while ((r = Socket->Read(b, sizeof(Buffer)-(b-Buffer))) > 0)
	{
		b += r;
		if (strnstr(Buffer, "\r\n.\r\n", b-Buffer))
			break;
	}

	if (r <= 0)
		return false;

	GToken t(Buffer, "\r\n");
	for (int i=1; i<t.Length()-1; i++)
	{
		Results.Add(t[i]);
	}

	return true;
}

bool MailPop3::Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags)
{
	bool Status = false;
	bool RemoveMail = false;
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
				IsSsl.CastBool())
				Port = POP3_SSL_PORT;
			else
				Port = POP3_PORT;
		}

		strcpy(Str, RemoteHost);
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
				bool NoAPOP = Cookie ? stristr(Cookie, "NoAPOP") : 0;
				if (!NoAPOP)
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
						strcpy(Buffer, "STARTTLS\r\n");
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
						strcpy(Key, Apop);
						strcat(Key, pass);

						ZeroObj(Digest);
						MDStringToDigest(Digest, Key);
						for (int i = 0; i < 16; i++)
							sprintf(HexDigest + (i*2), "%02.2x", Digest[i]);
						HexDigest[32] = 0;

						sprintf(Buffer, "APOP %s %s\r\n", user, HexDigest);
						VERIFY_ONERR(Write(0, true));
						Authed = ReadReply();

						if (!Authed)
						{
							DeleteArray(Cookie);
							DeleteArray(Apop);
							Cookie = NewStr("NoAPOP");
							S->Close();
							goto ReStartConnection;
						}
					}

					if (!SecurityError && SecureAuth)
					{
						GHashTable AuthTypes, Capabilities;
						if (ListCmd("AUTH", AuthTypes) &&
							ListCmd("CAPA", Capabilities))
						{
							if (AuthTypes.Find("GSSAPI"))
							{
								sprintf(Buffer, "AUTH GSSAPI\r\n");
								VERIFY_ONERR(Write(0, true));
								VERIFY_ONERR(ReadReply());

								// http://www.faqs.org/rfcs/rfc2743.html
							}
						}
					}					
					else if (!SecurityError && !Authed)
					{
						// have to use non-key method
						sprintf(Buffer, "USER %s\r\n", user);
						VERIFY_ONERR(Write(0, true));
						VERIFY_ONERR(ReadReply());

						sprintf(Buffer, "PASS %s\r\n", pass);
						VERIFY_ONERR(Write(0, false));
						Log("PASS *******", MAIL_SEND_COLOUR);

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

bool MailPop3::MailIsEnd(char *Ptr, int Len)
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
		for (int n = 0; n<Trans.Length(); n++)
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
					sprintf(Buffer, "LIST %i\r\n", Message + 1);
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
						sprintf(Buffer, "RETR %i\r\n", Message + 1);
					}
					else
					{
						sprintf(Buffer, "TOP %i %i\r\n", Message + 1, TopLines);
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
						int r = Socket->Read(Buffer+Used, sizeof(Buffer)-Used-1, 0);
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
									Log(Buffer, MAIL_RECEIVE_COLOUR);	

									// The Buffer was zero'd at the beginning garrenteeing
									// NULL termination
									int Len = strlen(Eol);
									int EndPos = End.IsEnd(Eol, Len);
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
									Log(Buffer, MAIL_ERROR_COLOUR);	
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
								int r = Socket->Read(Buffer, sizeof(Buffer), 0);
								if (r > 0)
								{
									DeNullText(Buffer, r);
									
									if (Transfer)
									{
										Transfer->Value += r;
									}

									int EndPos = End.IsEnd(Buffer, r);
									if (EndPos >= 0)
									{
										int Actual = EndPos - DataPos - 3;
										if (Actual > 0)
										{
											int w = Msg->Write(Buffer, Actual);
											LgiAssert(w == Actual);
										}
										// else the end point was in the last buffer

										Status = Trans[n]->Status = true;
										break;
									}
									else
									{
										int w = Msg->Write(Buffer, r);
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
		strcpy(Buffer, (char*)"LIST\r\n");
		VERIFY_RET_VAL(Write(0, true));

		char *s = 0;
		if (ReadMultiLineReply(s))
		{
			GToken l(s, "\r\n");
			DeleteArray(s);
			for (int i=0; i<l.Length(); i++)
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
		sprintf(Buffer, "LIST %i\r\n", Message + 1);
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
		sprintf(Buffer, "DELE %i\r\n", Message + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		return true;
	}

	return false;
}

bool MailPop3::GetUid(int Index, char *Id)
{
	if (Socket && Id)
	{
		sprintf(Buffer, "UIDL %i\r\n", Index + 1);
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
				
				strcpy(Id, Space+1);
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

		sprintf(Buffer, "UIDL\r\n");
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadMultiLineReply(Str));
		if (Str)
		{
			Status = true;
			GToken T(Str, "\r\n");
			for (int i=0; i<T.Length(); i++)
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
		sprintf(Buffer, "TOP %i 0\r\n", Message + 1);
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
		GBytePipe Temp;
		int ReadLen = Socket->Read(Buffer, sizeof(Buffer), 0);
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

				int Len = Temp.GetSize();
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
MailMessage::MailMessage()
{
	Private = 0;
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
		int InLen = In.GetSize(); // file remaining to read
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
	else
	{
		LgiTrace("%s:%i - Error allocating buffers\n", _FL);
	}

	DeleteArray(InBuf);
	DeleteArray(OutBuf);
				
	return Status;
}

int MailMessage::EncodeQuotedPrintable(GStreamI &Out, GStreamI &In)
{
	int Status = 0;
	char OutBuf[100], InBuf[1024];
	char *o = OutBuf;
	int InLen;

	// Read the input data one chunk at a time
	while ((InLen = In.Read(InBuf, sizeof(InBuf))) > 0)
	{
		// For all the input bytes we just got
		for (char *s = InBuf; s - InBuf < InLen; )
		{
			if (*s == '\n')
			{
				strcpy(o, "\r\n");
				int w = Out.Write(OutBuf, o-OutBuf+2);
				if (w <= 0)
					break;

				o = OutBuf;
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

				if (o == OutBuf)
				{
					sprintf(o, "=%02.2X", (uchar)*s);
					o += 3;
				}
				else
				{
					*o++ = *s;
				}
			}
			else if (*s & 0x80 || *s == '=')
			{
				// Require hex encoding of 8-bit chars and the equals itself.
				sprintf(o, "=%02.2X", (uchar)*s);
				o += 3;
			}
			else if (*s != '\r')
			{
				*o++ = *s;
			}
			s++;

			if (o-OutBuf > 73)
			{
				// time for a new line.
				strcpy(o, "=\r\n");
				int w = Out.Write(OutBuf, o - OutBuf + 3);
				if (w <= 0)
					break;
				o = OutBuf;
				Status += w;
			}
		}
	}

	strcpy(o, "\r\n");
	int w = Out.Write(OutBuf, o-OutBuf+2);
	if (w > 0)
		Status += w;

	return Status;
}

int MailMessage::EncodeText(GStreamI &Out, GStreamI &In)
{
	int Status = 0;
	char InBuf[4096];
	int InLen, InUsed = 0;
	char *Eol = "\r\n";

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

		return w;
	}
};

// Encode the whole email
bool MailMessage::Encode(GStreamI &Out, GStream *HeadersSink, MailProtocol *Protocol, bool Mime)
{
	GStringPipe p;
	bool Status = EncodeHeaders(p, Protocol, Mime);
	if (Status)
	{
		int Len = p.GetSize();
		char *Headers = p.NewStr();
		if (HeadersSink)
		{
			HeadersSink->Write(Headers, Len);
		}
		else
		{
			InternetHeader = NewStr(Headers);
		}

		if (Headers &&
			Out.Write(Headers, Len))
		{
			SendBuf *Buf = new SendBuf(&Out);
			if (Buf)
			{
				Status = EncodeBody(*Buf, Protocol, Mime);			
				if (Status)
				{
					Buf->Flush();
					Status = Buf->Status;
				}
				else
				{
					LgiTrace("%s:%i - EncodeBody failed.\n", _FL);
				}

				DeleteObj(Buf);
			}
		}
		else
		{
			LgiTrace("%s:%i - Headers output failed.\n", _FL);
		}
		
		DeleteArray(Headers);
	}
	else
	{
		LgiTrace("%s:%i - EncodeHeaders failed.\n", _FL);
	}

	return Status;
}

// This encodes the main headers but not the headers relating to the 
// actual content. Thats done by the ::EncodeBody function.
bool MailMessage::EncodeHeaders(GStreamI &Out, MailProtocol *Protocol, bool Mime)
{
	bool Status = true;

	// Setup
	char Buffer[1025];

	// Construct date
	char *Weekday[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	char *Month[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	GDateTime Dt;
	int TimeZone = Dt.SystemTimeZone();
	Dt.SetNow();
	sprintf(Buffer,
			"Date: %s, %i %s %i %i:%02.2i:%02.2i %s%2.2d%2.2d\r\n",
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
	Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

	if (Protocol && Protocol->ProgramName)
	{
		// X-Mailer:
		sprintf(Buffer, "X-Mailer: %s\r\n", Protocol->ProgramName);
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
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
	}

	if (Priority != MAIL_PRIORITY_NORMAL)
	{
		// X-Priority:
		sprintf(Buffer, "X-Priority: %i\r\n", Priority);
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
	}

	if (MarkColour >= 0)
	{
		// X-Color (HTML Colour Ref for email marking)
		sprintf(Buffer,
			"X-Color: #%02.2X%02.2X%02.2X\r\n",
			R24(MarkColour),
			G24(MarkColour),
			B24(MarkColour));
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
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
		
		int Ch = sprintf(Buffer, "Message-ID: %s\r\n", MessageID.Get());
		Status &= Out.Write(Buffer, Ch) > 0;
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

		sprintf(Buffer, "References: <%s>\r\n", Ref);
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
	}

	// To:
	char *ToAddr = CreateAddressTag(To, 0, &Protocol->CharsetPrefs);
	if (ToAddr)
	{
		Status &= Out.Write(ToAddr, strlen(ToAddr)) > 0;
		DeleteArray(ToAddr);
	}

	char *CcAddr = CreateAddressTag(To, 1, &Protocol->CharsetPrefs);
	if (CcAddr)
	{
		Status &= Out.Write(CcAddr, strlen(CcAddr)) > 0;
		DeleteArray(CcAddr);
	}

	// From:
	if (From && From->Addr)
	{
		strcpy(Buffer, "From: ");

		char *Nme = EncodeRfc2047(NewStr(From->Name), 0, &Protocol->CharsetPrefs);
		if (Nme)
		{
			if (strchr(Nme, '\"'))
			{
				sprintf(Buffer + strlen(Buffer), "'%s' ", Nme);
			}
			else
			{
				sprintf(Buffer + strlen(Buffer), "\"%s\" ", Nme);
			}
			DeleteArray(Nme);
		}

		sprintf(Buffer + strlen(Buffer), "<%s>\r\n", From->Addr);
	}
	else
	{
		LgiTrace("%s:%i - No from address.\n", _FL);
		return false;
	}
	Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

	// Reply-To:
	if (Reply &&
		ValidStr(Reply->Addr))
	{
		strcpy(Buffer, "Reply-To: ");

		char *Nme = EncodeRfc2047(NewStr(Reply->Name), 0, &Protocol->CharsetPrefs);
		if (Nme)
		{
			if (strchr(Nme, '\"'))
			{
				sprintf(Buffer + strlen(Buffer), "'%s' ", Nme);
			}
			else
			{
				sprintf(Buffer + strlen(Buffer), "\"%s\" ", Nme);
			}
			DeleteArray(Nme);
		}

		sprintf(Buffer+strlen(Buffer), "<%s>\r\n", Reply->Addr);

		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
	}

	// Subject:
	char *Subj = EncodeRfc2047(NewStr(Subject), 0, &Protocol->CharsetPrefs, 9);
	sprintf(Buffer, "Subject: %s\r\n", (Subj) ? Subj : "");
	Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
	DeleteArray(Subj);

	// DispositionNotificationTo
	if (DispositionNotificationTo)
	{
		strcpy(Buffer, "Disposition-Notification-To:");
		char *Nme = EncodeRfc2047(NewStr(From->Name), 0, &Protocol->CharsetPrefs);
		if (Nme)
		{
			sprintf(Buffer + strlen(Buffer), " \"%s\"", Nme);
			DeleteArray(Nme);
		}
		sprintf(Buffer + strlen(Buffer), " <%s>\r\n", From->Addr);
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
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
		bool MultipartAlternate = (Text != 0) && (Html != 0);
		bool MultipartMixed = FileDesc.Length() > 0;
		int Now = LgiCurrentTime();

		char Separator[256];
		sprintf(Separator, "----=_NextPart_%08.8X.%08.8X", Now, LgiGetCurrentThread());

		sprintf(Buffer, "MIME-Version: 1.0\r\n");
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
		
		char StrMultipartAlternative[] = "multipart/alternative";
		if (MultiPart)
		{
			char *Type = MultipartMixed ? (char*)"multipart/mixed" : StrMultipartAlternative;
			sprintf(Buffer, "Content-Type: %s;\r\n\tboundary=\"%s\"\r\n", Type, Separator);
			Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
		}

		if (Text != 0 || Html != 0)
		{
			char AlternateBoundry[128] = "";

			if (MultiPart)
			{
				sprintf(Buffer, "\r\n--%s\r\n", Separator);
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

				if (MultipartMixed && MultipartAlternate)
				{
					sprintf(AlternateBoundry, "----=_NextPart_%08.8X.%08.8X", Now++, LgiGetCurrentThread());
					sprintf(Buffer,
							"Content-Type: %s;\r\n\tboundary=\"%s\"\r\n",
							StrMultipartAlternative,
							AlternateBoundry);
					Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

					sprintf(Buffer, "\r\n--%s\r\n", AlternateBoundry);
					Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
				}
			}

			if (!Status)
				LgiTrace("%s:%i - Status=%i\n", _FL, Status);

			if (Text)
			{
				const char *Cs = 0;
				char *Txt = Text, *Mem = 0;
				
				// Detect charset
				if (!TextCharset || stricmp(TextCharset, "utf-8") == 0)
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
				sprintf(Buffer,
						"Content-Type: text/plain; charset=\"%s\"\r\n",
						Cs ? Cs : "us-ascii");
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

				if (!Status)
					LgiTrace("%s:%i - Status=%i\n", _FL, Status);

				// Transfer encoding
				if (Txt &&
					(Is8Bit(Txt) || MaxLineLen(Txt) >= 80))
				{
					char QuotPrint[] = "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
					Status &= Out.Write(QuotPrint, strlen(QuotPrint)) > 0;

					if (!Status)
						LgiTrace("%s:%i - Status=%i\n", _FL, Status);

					GMemStream TxtStr(Txt, strlen(Txt), false);
					Status &= EncodeQuotedPrintable(Out, TxtStr) > 0;
					
					if (!Status)
						LgiTrace("%s:%i - Status=%i\n", _FL, Status);
				}
				else
				{
					char Cte[] = "Content-Transfer-Encoding: 7bit\r\n\r\n";
					Status &= Out.Write(Cte, strlen(Cte)) > 0;

					if (!Status)
						LgiTrace("%s:%i - Status=%i\n", _FL, Status);

					GMemStream TxtStr(Txt, strlen(Txt), false);
					Status &= EncodeText(Out, TxtStr) > 0;

					if (!Status)
						LgiTrace("%s:%i - Status=%i\n", _FL, Status);
				}
				
				DeleteArray(Mem);
			}

			if (!Status)
				LgiTrace("%s:%i - Status=%i\n", _FL, Status);

			// Break alternate part
			if (AlternateBoundry[0])
			{
				sprintf(Buffer, "--%s\r\n", AlternateBoundry);
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
			}
			else if (MultipartAlternate)
			{
				sprintf(Buffer, "--%s\r\n", Separator);
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
			}

			if (Html)
			{
				// Content type
				sprintf(Buffer,
						"Content-Type: text/html; charset=\"%s\"\r\n",
						TextCharset ? TextCharset : "us-ascii");
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

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
				LgiTrace("%s:%i - Status=%i\n", _FL, Status);

			if (AlternateBoundry[0])
			{
				// End alternate part
				sprintf(Buffer, "\r\n--%s--\r\n", AlternateBoundry);
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
			}
		}

		if (!Status)
			LgiTrace("%s:%i - Status=%i\n", _FL, Status);

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
				sprintf(Buffer, "--%s\r\n", Separator);
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
			}

			char FileName[256];
			char *s = FileDes->Name(), *d = FileName;
			if (!s)
				LgiTrace("%s:%i - File descriptor has no name.\n", _FL);
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
				sprintf(Buffer,
						"Content-Type: %s; name=\"%s\"\r\n"
						"Content-Disposition: attachment\r\n",
						FileDes->GetMimeType() ? FileDes->GetMimeType() : "application/x-zip-compressed",
						(FName) ? FName : FileName);
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
				DeleteArray(FName);

				if (FileDes->GetContentId())
				{
					sprintf(Buffer, "Content-Id: %s\r\n", FileDes->GetContentId());
					Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
				}

				sprintf(Buffer, "Content-Transfer-Encoding: base64\r\n\r\n");
				Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
				Status &= F ? EncodeBase64(Out, *F) > 0 : false;
			}

			FileDes = FileDesc.Next();
		}

		if (!Status)
			LgiTrace("%s:%i - Status=%i\n", _FL, Status);

		if (MultiPart)
		{
			// write final separator
			sprintf(Buffer, "--%s--\r\n\r\n", Separator);
			Status &= Out.Write(Buffer, strlen(Buffer)) > 0;
		}
	}
	else
	{
		// send content type
		sprintf(Buffer, "Content-Type: text/plain; charset=\"%s\"\r\n", TextCharset ? TextCharset : "us-ascii");
		Status &= Out.Write(Buffer, strlen(Buffer)) > 0;

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
		LgiTrace("%s:%i - Status=%i\n", _FL, Status);

	return true;
}

