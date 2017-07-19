#include <stdio.h>

#include "LgiNetInc.h"
#include "Lgi.h"
#include "GMime.h"
#include "GToken.h"
#include "Base64.h"

static const char *MimeEol				= "\r\n";
static const char *MimeWs				= " \t\r\n";
static const char *MimeStr				= "\'\"";
static const char *MimeQuotedPrintable	= "quoted-printable";
static const char *MimeBase64			= "base64";

#define MimeMagic					( ('M'<<24) | ('I'<<24) | ('M'<<24) | ('E'<<24) )
#define SkipWs(s)					while (*s && strchr(MimeWs, *s)) s++
#define SkipNonWs(s)				while (*s && !strchr(MimeWs, *s)) s++

const char *GMime::DefaultCharset =		"text/plain";

template<typename T>
int CastInt(T in)
{
	int out = (int)in;
	LgiAssert(out == in);
	return out;
}

///////////////////////////////////////////////////////////////////////
enum MimeBoundary
{
	MimeData = 0,
	MimeNextSeg = 1,
	MimeEndSeg = 2
};

MimeBoundary IsMimeBoundary(char *Boundary, char *Line)
{
	if (Boundary)
	{
		size_t BoundaryLen = strlen(Boundary);
		if (Line &&
			*Line++ == '-' &&
			*Line++ == '-' &&
			strncmp(Line, Boundary, strlen(Boundary)) == 0)
		{
			// MIME segment boundary
			Line += BoundaryLen;
			if (Line[0] == '-' &&
				Line[1] == '-')
			{
				return MimeEndSeg;
			}
			else
			{
				return MimeNextSeg;
			}
		}
	}

	return MimeData;
}

void CreateMimeBoundary(char *Buf, int BufLen)
{
	if (Buf)
	{
		static int Count = 1;
		sprintf_s(Buf, BufLen, "--%x-%x-%x--", (int)LgiCurrentTime(), (int)(uint64)LgiGetCurrentThread(), Count++);
	}
}

///////////////////////////////////////////////////////////////////////
class GCoderStream : public GStream
{
protected:
	GStreamI *Out;

public:
	GCoderStream(GStreamI *o)
	{
		Out = o;
	}
};

class GMimeTextEncode : public GCoderStream
{
	// This code needs to make sure it writes an end-of-line at
	// the end, otherwise a following MIME boundary could be missed.
	bool LastEol;
	
public:
	GMimeTextEncode(GStreamI *o) : GCoderStream(o)
	{
		LastEol = false;
	}
	
	~GMimeTextEncode()
	{
		if (!LastEol)
		{
			int w = Out->Write(MimeEol, 2);
			LgiAssert(w == 2);
		}
	}

	int Write(const void *p, int size, int f = 0)
	{
		// Make sure any new lines are \r\n
		char *s = (char*)p, *e = s + size;
		int wr = 0;
		
		while (s < e)
		{
			char *c = s;
			while (c < e && *c != '\r' && *c != '\n')
				c++;
			
			if (c > s)
			{
				ptrdiff_t bytes = c - s;
				int w = Out->Write(s, (int)bytes);
				if (w != bytes)
					return wr;
				wr += w;
				LastEol = false;
			}
			
			while (c < e && (*c == '\r' || *c == '\n'))
			{
				if (*c == '\n')
				{
					int w = Out->Write(MimeEol, 2);
					if (w != 2)
						return wr;
					LastEol = true;
				}
				wr++;
				c++;
			}

			s = c;
		}
		
		return wr;
	}
};

class GMimeQuotedPrintableEncode : public GCoderStream
{
	// 'd' is our current position in 'Buf'
	char *d;
	
	// A buffer for a line of quoted printable text
	char Buf[128];

public:
	GMimeQuotedPrintableEncode(GStreamI *o) : GCoderStream(o)
	{
		Buf[0] = 0;
		d = Buf;
	}
	
	~GMimeQuotedPrintableEncode()
	{
		if (d > Buf)
		{
			// Write partial line
			*d++ = '\r';
			*d++ = '\n';

			ptrdiff_t Len = d - Buf;
			Out->Write(Buf, CastInt(Len));
		}
	}

	int Write(const void *p, int size, int f = 0)
	{
		char *s = (char*)p;
		char *e = s + size;
		while (s < e)
		{
			if (*s == '\n' || !*s)
			{
				*d++ = '\r';
				*d++ = '\n';

				ptrdiff_t Len = d - Buf;
				if (Out->Write(Buf, CastInt(Len)) < Len)
				{
					LgiAssert(!"write error");
					break;
				}
				if (!*s)
				{
					break;
				}

				d = Buf;
				s++;
			}
			else if (*s & 0x80 ||
					 *s == '.' ||
					 *s == '=')
			{
				int Ch = sprintf_s(d, sizeof(Buf)-(d-Buf), "=%2.2X", (uchar)*s);
				if (Ch < 0)
				{
					LgiAssert(!"printf error");
					break;
				}
				d += Ch;
				s++;
			}
			else if (*s != '\r')
			{
				*d++ = *s++;
			}
			else
			{
				// Consume any '\r' without outputting them
				s++;
			}

			if (d-Buf > 73)
			{
				// time for a new line.
				*d++ = '=';
				*d++ = '\r';
				*d++ = '\n';

				ptrdiff_t Len = d-Buf;
				if (Out->Write(Buf, CastInt(Len)) < Len)
				{
					LgiAssert(!"write error");
					break;
				}

				d = Buf;
			}
		}

		return CastInt(s - (const char*)p);
	}
};

class GMimeQuotedPrintableDecode : public GCoderStream
{
	GStringPipe Buf;

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

public:
	GMimeQuotedPrintableDecode(GStreamI *o) : GCoderStream(o) {}

	int Write(const void *p, int size, int f = 0)
	{
		int Written = 0;
		
		Buf.Push((char*)p, size);

		char In[1024];
		while (Buf.Pop(In, sizeof(In)))
		{
			char Line[1024];
			char *o = Line;

			for (char *s=In; *s; )
			{
				if (*s == '=')
				{
					s++; // skip '='
					if (*s == '\r' || *s == '\n')
					{
						if (*s == '\r')
							s++;
						if (*s == '\n')
							s++;
					}
					else if (*s)
					{
						char c = ConvHexToBin(*s++);
						*o++ = (c << 4) | (ConvHexToBin(*s++) & 0xF);
					}
					else break;
				}
				else
				{
					*o++ = *s++;
				}
			}

			size_t Len = o - Line;
			if (Out->Write(Line, CastInt(Len)) < (int64)Len)
			{
				// Error
				return 0;
			}
			Written += Len;
			o = Line;
		}

		return Written;
	}
};

#define BASE64_LINE_SZ			76
#define BASE64_READ_SZ			(BASE64_LINE_SZ*3/4)

class GMimeBase64Encode : public GCoderStream
{
	GMemQueue Buf;

public:
	GMimeBase64Encode(GStreamI *o) : GCoderStream(o) {}
	
	~GMimeBase64Encode()
	{
		uchar b[100];

		int64 Len = Buf.GetSize();
		LgiAssert(Len < sizeof(b));
		int r = Buf.Read(b, CastInt(Len));
		if (r > 0)
		{
			char t[256];
			int w = ConvertBinaryToBase64(t, sizeof(t), b, r);
			Out->Write(t, w);
			Out->Write(MimeEol, 2);
		}
	}

	int Write(const void *p, int size, int f = 0)
	{
		Buf.Write((uchar*)p, size);

		while (Buf.GetSize() >= BASE64_READ_SZ)
		{
			uchar b[100];
			int r = Buf.Read(b, BASE64_READ_SZ);
			if (r)
			{
				char t[256];
				int w = ConvertBinaryToBase64(t, sizeof(t), b, r);
				if (w > 0)
				{
					Out->Write(t, w);
					Out->Write(MimeEol, 2);
				}
				else LgiAssert(0);
			}
			else return 0;
		}

		return size;
	}
};


class GMimeBase64Decode : public GCoderStream
{
	GStringPipe Buf;
	uint8 Lut[256];

public:
	GMimeBase64Decode(GStreamI *o) : GCoderStream(o)
	{
		ZeroObj(Lut);
		memset(Lut+(int)'a', 1, 'z'-'a'+1);
		memset(Lut+(int)'A', 1, 'Z'-'A'+1);
		memset(Lut+(int)'0', 1, '9'-'0'+1);
		Lut['+'] = 1;
		Lut['/'] = 1;
		Lut['='] = 1;
	}

	int Write(const void *p, int size, int f = 0)
	{
		int Status = 0;

		// Push non whitespace into the memory buf
		char *s = (char*)p;
		char *e = s + size;
		while (s < e)
		{
			while (*s && s < e && !Lut[*s]) s++;
			char *Start = s;
			while (*s && s < e && Lut[*s]) s++;
			if (s-Start > 0)
				Buf.Push(Start, CastInt(s-Start));
			else
				break;
		}

		// While there is at least one run of base64 (4 bytes) convert it to text
		// and write it to the output stream
		int Size;
		while ((Size = CastInt(Buf.GetSize())) > 3)
		{
			Size &= ~3;

			char t[256];
			int r = min(sizeof(t), Size);
			if ((r = Buf.GMemQueue::Read((uchar*)t, r)) > 0)
			{
				uchar b[256];
				int w = ConvertBase64ToBinary(b, sizeof(b), t, r);
				Out->Write(b, w);
				Status += w;
			}
		}

		return Status;
	}
};

///////////////////////////////////////////////////////////////////////
GMimeBuf::GMimeBuf(GStreamI *src, GStreamEnd *end)
{
	Total = 0;
	Src = src;
	End = end;

	Src->SetPos(0);
}

int GMimeBuf::Pop(char *Str, int BufSize)
{
	int Ret = 0;

	while (!(Ret = GStringPipe::Pop(Str, BufSize)))
	{
		if (Src)
		{
			char Buf[1024];
			int r = Src ? Src->Read(Buf, sizeof(Buf)) : 0;
			if (r)
			{
				if (End)
				{
					int e = End->IsEnd(Buf, r);
					if (e >= 0)
					{
						// End of stream
						int s = e - Total;
						Push(Buf, s);
						Total += s;
						Src = 0; // no more data anyway
					}
					else
					{
						// Not the end
						Push(Buf, r);
						Total += r;
					}
				}
				else
				{
					Push(Buf, r);
					Total += r;
				}
			}
			else
			{
				Src = NULL; // Source data is finished
			}
		}
		else
		{
			// Is there any unterminated space in the string pipe?
			int64 Sz = GStringPipe::GetSize();
			if (Sz > 0)
			{
				Ret = GStringPipe::Read(Str, BufSize);
			}
			break;
		}
	}

	return Ret;
}

///////////////////////////////////////////////////////////////////////
// Mime Object
GMime::GMime(char *tmp)
{
	Parent = 0;
	TmpPath = NewStr(tmp);

	Headers = 0;
	DataPos = 0;
	DataSize = 0;
	DataLock = 0;
	DataStore = 0;
	OwnDataStore = 0;

	Text.Decode.Mime = this;
	Text.Encode.Mime = this;
	Binary.Read.Mime = this;
	Binary.Write.Mime = this;
}

GMime::~GMime()
{
	Remove();
	Empty();
	DeleteArray(TmpPath);
}

char *GMime::GetFileName()
{
	char *n = GetSub("Content-Type", "Name");
	if (!n)
		n = GetSub("Content-Disposition", "Filename");
	if (!n)
	{
		n = Get("Content-Location");
		if (n)
		{
			char *trim = TrimStr(n, "\'\"");
			if (trim)
			{
			    DeleteArray(n);
			    n = trim;
			}
		}
	}
	return n;
}

GMime *GMime::NewChild()
{
	GMime *n = new GMime(GetTmpPath());
	if (n)
		Insert(n);
	return n;
}

bool GMime::Insert(GMime *m, int Pos)
{
	LgiAssert(m != NULL);
	if (!m)
		return false;
	
	if (m->Parent)
	{
		LgiAssert(m->Parent->Children.HasItem(m));
		m->Parent->Children.Delete(m, true);
	}

	m->Parent = this;
	LgiAssert(!Children.HasItem(m));
	if (Pos >= 0)
		Children.AddAt(Pos, m);
	else
		Children.Add(m);

	return true;
}

void GMime::Remove()
{
	if (Parent)
	{
		LgiAssert(Parent->Children.HasItem(this));
		Parent->Children.Delete(this, true);
		Parent = 0;
	}
}

GMime *GMime::operator[](uint32 i)
{
	if (i >= Children.Length())
		return 0;

	return Children[i];
}

char *GMime::GetTmpPath()
{
	for (GMime *m = this; m; m = m->Parent)
	{
		if (m->TmpPath)
			return m->TmpPath;
	}
	return 0;
}

bool GMime::SetHeaders(const char *h)
{
	DeleteArray(Headers);
	Headers = NewStr(h);
	return Headers != 0;
}

bool GMime::Lock()
{
	bool Lock = true;
	if (DataLock)
	{
		Lock = DataLock->Lock(_FL);
	}
	return Lock;
}

void GMime::Unlock()
{
	if (DataLock)
	{
		DataLock->Unlock();
	}
}

bool GMime::CreateTempData()
{
	bool Status = false;

	DataPos = 0;
	DataSize = 0;
	OwnDataStore = true;
	if ((DataStore = new GTempStream(GetTmpPath(), 4 << 20)))
	{
		Status = true;
	}

	return Status;
}

GStreamI *GMime::GetData(bool Detach)
{
	GStreamI *Ds = DataStore;

	if (Ds)
	{		
		Ds->SetPos(DataPos);
		if (Detach)
			DataStore = 0;
	}

	return Ds;
}

bool GMime::SetData(bool OwnStream, GStreamI *d, int Pos, int Size, GMutex *l)
{
	if (DataStore && Lock())
	{
		DeleteObj(DataStore);
		Unlock();
	}

	if (d)
	{
		OwnDataStore = OwnStream;
		DataPos = Pos;
		DataSize = Size >= 0 ? Size : (int)d->GetSize();
		DataLock = l;
		DataStore = d;
	}

	return true;
}

bool GMime::SetData(char *Str, int Len)
{
	if (DataStore && Lock())
	{
		DeleteObj(DataStore);
		Unlock();
	}

	if (Str)
	{
		if (Len < 0)
		{
			Len = (int)strlen(Str);
		}

		DataLock = 0;
		DataPos = 0;
		DataSize = Len;
		DataStore = new GTempStream(GetTmpPath(), 4 << 20);
		if (DataStore)
		{
			DataStore->Write(Str, Len);
		}
	}

	return true;
}

void GMime::Empty()
{
	if (OwnDataStore)
	{
		if (Lock())
		{
			DeleteObj(DataStore);
			Unlock();
		}
	}

	while (Children.Length())
		delete Children[0];
	DeleteArray(Headers);

	DataPos = 0;
	DataSize = 0;
	DataLock = 0;
	DataStore = 0;
	OwnDataStore = 0;
}

char *GMime::NewValue(char *&s, bool Alloc)
{
	char *Status = 0;

	int Inc = 0;
	char *End;
	if (strchr(MimeStr, *s))
	{
		// Delimited string
		char Delim = *s++;
		End = strchr(s, Delim);
		Inc = 1;
	}
	else
	{
		// Raw string
		End = s;
		while (*End && *End != ';' && *End != '\n' && *End != '\r') End++;
		while (strchr(MimeWs, End[-1])) End--;
	}

	if (End)
	{
		if (Alloc)
		{
			Status = NewStr(s, End-s);
		}
		s = End + Inc;
	}

	SkipWs(s);

	return Status;
}

char *GMime::StartOfField(char *s, const char *Field)
{
	if (s && Field)
	{
		size_t FieldLen = strlen(Field);
		while (s && *s)
		{
			if (strchr(MimeWs, *s))
			{
				s = strchr(s, '\n');
				if (s) s++;
			}
			else
			{
				char *f = s;
				while (*s && *s != ':' && !strchr(MimeWs, *s)) s++;
				int fLen = CastInt(s - f);
				if (*s++ == ':' &&
					fLen == FieldLen &&
					_strnicmp(f, Field, FieldLen) == 0)
				{
					return f;
					break;
				}
				else
				{
					s = strchr(s, '\n');
					if (s) s++;
				}
			}
		}
	}

	return 0;
}

char *GMime::NextField(char *s)
{
	while (s)
	{
		while (*s && *s != '\n') s++;
		if (*s == '\n')
		{
			s++;
			if (!strchr(MimeWs, *s))
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	return s;
}

char *GMime::Get(const char *Name, bool Short, const char *Default)
{
	char *Status = 0;

	if (Name && Headers)
	{
		char *s = StartOfField(Headers, Name);
		if (s)
		{
			s = strchr(s, ':');
			if (s)
			{
				s++;
				SkipWs(s);
				if (Short)
				{
					Status = NewValue(s);
				}
				else
				{
					char *e = NextField(s);
					while (strchr(MimeWs, e[-1])) e--;
					Status = NewStr(s, e-s);
				}
			}
		}

		if (!Status && Default)
		{
			Status = NewStr(Default);
		}
	}

	return Status;
}

bool GMime::Set(const char *Name, const char *Value)
{
	if (!Name)
		return false;

	GStringPipe p;

	char *h = Headers;
	if (h)
	{
		char *f = StartOfField(h, Name);
		if (f)
		{
			// 'Name' exists, push out pre 'Name' header text
			p.Push(h, CastInt(f - Headers));
			h = NextField(f);
		}
		else
		{
			if (!Value)
			{
				// Nothing to do here...
				return true;
			}

			// 'Name' doesn't exist, push out all the headers
			p.Push(Headers);
			h = 0;
		}
	}

	if (Value)
	{
		// Push new field
		int Vlen = CastInt(strlen(Value));
		while (Vlen > 0 && strchr(MimeWs, Value[Vlen-1])) Vlen--;

		p.Push(Name);
		p.Push(": ");
		p.Push(Value, Vlen);
		p.Push(MimeEol);
	}
	// else we're deleting the feild

	if (h)
	{
		// Push out any header text post the 'Name' field.
		p.Push(h);
	}

	DeleteArray(Headers);
	Headers = (char*)p.New(sizeof(char));

	return Headers != NULL;
}

char *GMime::GetSub(const char *Field, const char *Sub)
{
	char *Status = 0;

	if (Field && Sub)
	{
		int SubLen = CastInt(strlen(Sub));
		char *v = Get(Field, false);
		if (v)
		{
			// Move past the field value into the sub fields
			char *s = v;
			SkipWs(s);
			while (*s && *s != ';' && !strchr(MimeWs, *s)) s++;
			SkipWs(s);
			while (s && *s++ == ';')
			{
				// Parse each name=value pair
				SkipWs(s);
				char *Name = s;
				while (*s && *s != '=' && !strchr(MimeWs, *s)) s++;
				int NameLen = CastInt(s - Name);
				SkipWs(s);
				if (*s++ == '=')
				{
					bool Found = SubLen == NameLen && _strnicmp(Name, Sub, NameLen) == 0;
					SkipWs(s);
					Status = NewValue(s, Found);
					if (Found) break;
				}
				else break;
			}

			DeleteArray(v);
		}
	}

	return Status;
}

bool GMime::SetSub(const char *Field, const char *Sub, const char *Value, const char *DefaultValue)
{
	if (Field && Sub)
	{
		char Buf[256];

		char *s = StartOfField(Headers, Field);
		if (s)
		{
			// Header already exists
			s = strchr(s, ':');
			if (s++)
			{
				SkipWs(s);

				GStringPipe p;

				// Push the field data
				char *e = s;
				while (*e && !strchr("; \t\r\n", *e)) e++;
				p.Push(s, CastInt(e-s));
				SkipWs(e);

				// Loop through the subfields and push all those that are not 'Sub'
				s = e;
				while (*s++ == ';')
				{
					SkipWs(s);
					char *e = s;
					while (*e && *e != '=' && !strchr(MimeWs, *e)) e++;
					char *Name = NewStr(s, e-s);
					if (Name)
					{
						s = e;
						SkipWs(s);
						if (*s++ == '=')
						{
							char *v = NewValue(s);
							if (_stricmp(Name, Sub) != 0)
							{
								sprintf_s(Buf, sizeof(Buf), ";\r\n\t%s=\"%s\"", Name, v);
								p.Push(Buf);
							}
							DeleteArray(v);
						}
						else break;
					}
					else break;
				}

				if (Value)
				{
					// Push the new sub field
					sprintf_s(Buf, sizeof(Buf), ";\r\n\t%s=\"%s\"", Sub, Value);
					p.Push(Buf);
				}

				char *Data = p.NewStr();
				if (Data)
				{
					Set(Field, Data);
					DeleteArray(Data);
				}
			}
		}
		else if (DefaultValue)
		{
			// Header doesn't exist at all
			if (Value)
			{
				// Set
				sprintf_s(Buf, sizeof(Buf), "%s;\r\n\t%s=\"%s\"", DefaultValue, Sub, Value);
				return Set(Field, Buf);
			}
			else
			{
				// Remove
				return Set(Field, DefaultValue);
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////
// Mime Text Conversion

// Rfc822 Text -> Object
int GMime::GMimeText::GMimeDecode::Pull(GStreamI *Source, GStreamEnd *End)
{
	GMimeBuf Buf(Source, End); // Stream -> Lines
	return Parse(&Buf);
}

class ParentState
{
public:
	char *Boundary;
	MimeBoundary Type;

	ParentState()
	{
		Boundary = 0;
		Type = MimeData;
	}
};

int GMime::GMimeText::GMimeDecode::Parse(GStringPipe *Source, ParentState *State)
{
	int Status = 0;

	if (Mime && Source)
	{
		Mime->Empty();

		if (!Mime->CreateTempData())
			LgiAssert(!"CreateTempData failed.");
		else
		{
			// Read the headers..
			char Buf[1024];
			GStringPipe HeaderBuf;
			int r;
			while ((r = Source->Pop(Buf, sizeof(Buf))) > 0)
			{
				if (!strchr(MimeEol, Buf[0]))
				{
					// Store part of the headers
					HeaderBuf.Push(Buf, r);
				}
				else break;
			}

			if (r < 0)
				return 0;


			// Not an error
			Mime->Headers = HeaderBuf.NewStr();

			// Get various bits out of the header
			char *Encoding = Mime->GetEncoding();
			char *Boundary = Mime->GetBoundary();
			// int BoundaryLen = Boundary ? strlen(Boundary) : 0;
			GStream *Decoder = 0;
			if (Encoding)
			{
				if (_stricmp(Encoding, MimeQuotedPrintable) == 0)
				{
					Decoder = new GMimeQuotedPrintableDecode(Mime->DataStore);
				}
				else if (_stricmp(Encoding, MimeBase64) == 0)
				{
					Decoder = new GMimeBase64Decode(Mime->DataStore);
				}
			}
			DeleteArray(Encoding);

			// Read in the rest of the MIME segment
			bool Done = false;
			// int64 StartPos = Mime->DataStore->GetPos();
			while (!Done)
			{
				// Process existing lines
				int Len;
				int Written = 0;

				Status = true;
				while ((Len = Source->Pop(Buf, sizeof(Buf))))
				{
					// Check for boundary
					MimeBoundary Type = MimeData;
					if (Boundary)
					{
						 Type = IsMimeBoundary(Boundary, Buf);
					}
					
					if (State)
					{
						State->Type = IsMimeBoundary(State->Boundary, Buf);
						if (State->Type)
						{
							Status = Done = true;
							break;
						}
					}

					DoSegment:
					if (Type == MimeNextSeg)
					{
						ParentState MyState;
						MyState.Boundary = Boundary;

						GMime *Seg = new GMime(Mime->GetTmpPath());
						if (Seg &&
							Seg->Text.Decode.Parse(Source, &MyState))
						{
							Mime->Insert(Seg);

							if (MyState.Type)
							{
								Type = MyState.Type;
								goto DoSegment;
							}
						}
						else break;
					}
					else if (Type == MimeEndSeg)
					{
						Done = true;
						break;
					}
					else
					{
						// Process data
						if (Decoder)
						{
							Written += Decoder->Write(Buf, Len);
						}
						else
						{
							int w = Mime->DataStore->Write(Buf, Len);
							if (w > 0)
							{
								Written += w;
							}
							else
							{
								Done = true;
								Status = false;
								break;
							}
						}
					}
				}

				Mime->DataSize = Written;
				if (Len == 0)
				{
					Done = true;
				}
			}

			DeleteObj(Decoder);
			DeleteArray(Boundary);
		}
	}

	return Status;
}

void GMime::GMimeText::GMimeDecode::Empty()
{
}

// Object -> Rfc822 Text
int GMime::GMimeText::GMimeEncode::Push(GStreamI *Dest, GStreamEnd *End)
{
	int Status = 0;

	if (Mime)
	{
		char Buf[1024];
		int Ch;

		// Check boundary
		char *Boundary = Mime->GetBoundary();
		if (Mime->Children.Length())
		{
			// Boundary required
			if (!Boundary)
			{
				// Create one
				char b[256];
				CreateMimeBoundary(b, sizeof(b));
				Mime->SetBoundary(b);
				Boundary = Mime->GetBoundary();
			}
		}
		else if (Boundary)
		{
			// Remove boundary
			Mime->SetBoundary(0);
			DeleteArray(Boundary);
		}

		// Check encoding
		char *Encoding = Mime->GetEncoding();
		if (!Encoding)
		{
			// Detect an appropriate encoding
			int MaxLine = 0;
			bool Has8Bit = false;
			bool HasBin = false;
			if (Mime->DataStore &&
				Mime->Lock())
			{
				Mime->DataStore->SetPos(Mime->DataPos);
				int x = 0;
				for (int i=0; i<Mime->DataSize; )
				{
					int m = min(Mime->DataSize - i, sizeof(Buf));
					int r = Mime->DataStore->Read(Buf, m);
					if (r > 0)
					{
						for (int n=0; n<r; n++)
						{
							if (Buf[n] == '\n')
							{
								MaxLine = max(x, MaxLine);
								x = 0;
							}
							else
							{
								x++;
							}

							if (Buf[n] & 0x80)
							{
								Has8Bit = true;
							}
							if (Buf[n] < ' ' &&
								Buf[n] != '\t' &&
								Buf[n] != '\r' &&
								Buf[n] != '\n')
							{
								HasBin = true;
							}
						}

						i += r;
					}
					else break;
				}
				
				Mime->Unlock();			
			}

			if (HasBin)
			{
				Encoding = NewStr(MimeBase64);
			}
			else if (Has8Bit || MaxLine > 70)
			{
				Encoding = NewStr(MimeQuotedPrintable);
			}

			if (Encoding)
			{
				Mime->SetEncoding(Encoding);
			}
		}

		// Write the headers
		GToken h(Mime->Headers, MimeEol);
		for (unsigned i=0; i<h.Length(); i++)
		{
			Dest->Write(h[i], CastInt(strlen(h[i])));
			Dest->Write(MimeEol, 2);
		}
		Dest->Write(MimeEol, 2);

		// Write data
		GStream *Encoder = 0;
		if (Encoding)
		{
			if (_stricmp(Encoding, MimeQuotedPrintable) == 0)
			{
				Encoder = new GMimeQuotedPrintableEncode(Dest);
			}
			else if (_stricmp(Encoding, MimeBase64) == 0)
			{
				Encoder = new GMimeBase64Encode(Dest);
			}
		}
		if (!Encoder)
		{
			Encoder = new GMimeTextEncode(Dest);
		}

		if (Mime->DataStore)
		{
			if (Mime->Lock())
			{
				Mime->DataStore->SetPos(Mime->DataPos);
				Status = Mime->DataSize == 0; // Nothing is a valid segment??
				for (int i=0; i<Mime->DataSize; )
				{
					int m = min(Mime->DataSize-i, sizeof(Buf));
					int r = Mime->DataStore->Read(Buf, m);
					if (r > 0)
					{
						Encoder->Write(Buf, r);
						Status = true;
					}
					else break;
				}
				Mime->Unlock();
			}
		}
		else
		{
			Status = true;
		}

		DeleteObj(Encoder);

		// Write children
		if (Mime->Children.Length() && Boundary)
		{
			for (unsigned i=0; i<Mime->Children.Length(); i++)
			{
				Ch = sprintf_s(Buf, sizeof(Buf), "--%s\r\n", Boundary);
				Dest->Write(Buf, Ch);

				if (!Mime->Children[i]->Text.Encode.Push(Dest, End))
				{
					break;
				}

				Status = 1;
			}

			Ch = sprintf_s(Buf, sizeof(Buf), "--%s--\r\n", Boundary);
			Dest->Write(Buf, Ch);
		}

		// Clean up
		DeleteArray(Encoding);
		DeleteArray(Boundary);
	}

	return Status;
}

void GMime::GMimeText::GMimeEncode::Empty()
{
}

/////////////////////////////////////////////////////////////////////////
// Mime Binary Serialization

// Source -> Object
int GMime::GMimeBinary::GMimeRead::Pull(GStreamI *Source, GStreamEnd *End)
{
	if (Source)
	{
		int32 Header[4];
		Mime->Empty();

		// Read header block (Magic, HeaderSize, DataSize, # of Children)
		// and check magic
		if (Source->Read(Header, sizeof(Header)) == sizeof(Header) &&
			Header[0] == MimeMagic)
		{
			// Read header data
			Mime->Headers = new char[Header[1]+1];
			if (Mime->Headers &&
				Source->Read(Mime->Headers, Header[1]) == Header[1])
			{
				// NUL terminate
				Mime->Headers[Header[1]] = 0;

				// Skip body data
				if (Source->SetPos(Source->GetPos() + Header[2]) > 0)
				{
					// Read the children in
					for (int i=0; i<Header[3]; i++)
					{
						GMime *c = new GMime(Mime->GetTmpPath());
						if (c &&
							c->Binary.Read.Pull(Source, End))
						{
							Mime->Insert(c);
						}
						else break;
					}
				}
			}

			return 1; // success
		}
	}

	return 0; // failure
}

void GMime::GMimeBinary::GMimeRead::Empty()
{
}

// Object -> Dest
int64 GMime::GMimeBinary::GMimeWrite::GetSize()
{
	int64 Size = 0;

	if (Mime)
	{
		Size =	(sizeof(int32) * 4) + // Header magic + block sizes
				(Mime->Headers ? strlen(Mime->Headers) : 0) + // Headers
				(Mime->DataStore ? Mime->DataSize : 0); // Data

		// Children
		for (unsigned i=0; i<Mime->Children.Length(); i++)
		{
			Size += Mime->Children[i]->Binary.Write.GetSize();
		}
	}

	return Size;
}

int GMime::GMimeBinary::GMimeWrite::Push(GStreamI *Dest, GStreamEnd *End)
{
	if (Dest && Mime)
	{
		int32 Header[4] =
		{
			MimeMagic,
			Mime->Headers ? (int32) strlen(Mime->Headers) : 0,
			Mime->DataStore ? Mime->DataSize : 0,
			(int32) Mime->Children.Length()
		};

		if (Dest->Write(Header, sizeof(Header)) == sizeof(Header))
		{
			if (Mime->Headers)
			{
				Dest->Write(Mime->Headers, Header[1]);
			}

			if (Mime->DataStore)
			{
				char Buf[1024];
				int Written = 0;
				int Read = 0;
				int r;
				while ((r = Mime->DataStore->Read(Buf, min(sizeof(Buf), Header[2]-Read) )) > 0)
				{
					int w;
					if ((w = Dest->Write(Buf, r)) <= 0)
					{
						// Write error
						break;
					}

					Written += w;
					Read += r;
				}

				// Check we've written out all the data
				LgiAssert(Written < Header[2]);
				if (Written < Header[2])
				{
					return 0;
				}
			}

			for (unsigned i=0; i<Mime->Children.Length(); i++)
			{
				Mime->Children[i]->Binary.Write.Push(Dest, End);
			}

			return 1;
		}
	}

	return 0;
}

void GMime::GMimeBinary::GMimeWrite::Empty()
{
}

