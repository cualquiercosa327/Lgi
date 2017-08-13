#include <time.h>
#ifdef LINUX
#include <unistd.h>
#endif

#include "Lgi.h"
#include "GToken.h"
#include "Mail.h"
#include "Base64.h"
#include "INetTools.h"
#include "GUtf8.h"
#include "GDocView.h"
#include "IHttp.h"
#include "HttpTools.h"
#include "OpenSSLSocket.h"

#define DEBUG_OAUTH2				1
#define DEBUG_FETCH					0
#define OPT_ImapOAuth2AccessToken	"OAuth2AccessTok"


////////////////////////////////////////////////////////////////////////////
#if GPL_COMPATIBLE
#include "AuthNtlm/Ntlm.h"
#else
#include "../../src/common/INet/libntlm-0.4.2/ntlm.h"
#endif
#if HAS_LIBGSASL
#include "gsasl.h"
#endif

static const char *sRfc822Header	= "RFC822.HEADER";
static const char *sRfc822Size		= "RFC822.SIZE";

bool Base64Str(GString &s)
{
	GString b64;
	ssize_t Base64Len = BufferLen_BinTo64(s.Length());
	if (!b64.Set(NULL, Base64Len))
		return false;
	
	ssize_t Ch = ConvertBinaryToBase64(b64.Get(), b64.Length(), (uchar*)s.Get(), s.Length());
	LgiAssert(Ch == b64.Length());
	s = b64;
	return true;
}

bool UnBase64Str(GString &s)
{
	GString Bin;
	ssize_t BinLen = BufferLen_64ToBin(s.Length());
	if (!Bin.Set(NULL, BinLen))
		return false;
	
	ssize_t Ch = ConvertBase64ToBinary((uchar*)Bin.Get(), Bin.Length(), s.Get(), s.Length());
	LgiAssert(Ch <= (int)Bin.Length());
	s = Bin;
	s.Get()[Ch] = 0;
	return true;
}

#define SkipWhiteSpace(s)			while (*s && IsWhiteSpace(*s)) s++;
bool JsonDecode(GXmlTag &t, const char *s)
{
	if (*s != '{')
		return false;
	s++;	
	while (*s)
	{
		SkipWhiteSpace(s);
		if (*s != '\"')
			break;
		GAutoString Variable(LgiTokStr(s));
		SkipWhiteSpace(s);
		if (*s != ':')
			return false;
		s++;
		SkipWhiteSpace(s);
		GAutoString Value(LgiTokStr(s));
		SkipWhiteSpace(s);
		
		t.SetAttr(Variable, Value);
		if (*s != ',')
			break;
		s++;
	}
		
	if (*s != '}')
		return false;
	s++;
	return true;
}

struct StrRange
{
	ssize_t Start, End;
	
	void Set(ssize_t s, ssize_t e)
	{
		Start = s;
		End = e;
	}

	ssize_t Len() { return End - Start; }
};

#define SkipWhite(s)		while (*s && strchr(WhiteSpace, *s)) s++
#define SkipSpaces(s)		while (*s && strchr(" \t", *s)) s++
#define SkipNonWhite(s)		while (*s && !strchr(WhiteSpace, *s)) s++;
#define ExpectChar(ch)		if (*s != ch) return 0; s++

ssize_t ParseImapResponse(char *Buffer, ssize_t BufferLen, GArray<StrRange> &Ranges, int Names)
{
	Ranges.Length(0);

	if (!*Buffer || *Buffer != '*')
		return 0;

	char *End = Buffer + BufferLen;
	char *s = Buffer + 1;
	char *Start;
	for (int n=0; n<Names; n++)
	{
		SkipSpaces(s);
		Start = s;
		SkipNonWhite(s);
		if (s <= Start) return 0;
		Ranges.New().Set(Start - Buffer, s - Buffer); // The msg seq or UID
	}
	
	// Look for start of block
	SkipSpaces(s);
	if (*s != '(')
	{
		char *Eol = strnstr(Buffer, "\r\n", BufferLen);
		if (Eol)
		{
			s = Eol + 2;
			return s - Buffer;
		}
		return 0;
	}
	s++;

	// Parse fields
	int MsgSize = 0;
	while (*s)
	{
		LgiAssert(s < End);
		
		// Field name
		SkipWhite(s);
		if (*s == ')')
		{
			ExpectChar(')');
			ExpectChar('\r');
			ExpectChar('\n');
			
			MsgSize = s - Buffer;
			break;
		}

		Start = s;
		SkipNonWhite(s);
		if (!*s || s <= Start)
			return 0;
		Ranges.New().Set(Start - Buffer, s - Buffer);

		// Field value
		SkipWhite(s);
		if (*s == '{')
		{
			// Parse multiline
			s++;
			int Size = atoi(s);
			while (*s && *s != '}')
				s++;
			ExpectChar('}');
			ExpectChar('\r');
			ExpectChar('\n');
			Start = s;
			int CurPos = s - Buffer;
			if (CurPos + Size <= BufferLen)
			{
				s += Size;
				Ranges.New().Set(Start - Buffer, s - Buffer);
			}
			else
				return 0;
		}
		else
		{
			// Parse single
			if (*s == '(')
			{
				s++;
				Start = s;
				
				int Depth = 1;
				while (*s)
				{
					if (*s == '\"')
					{
						s++;
						while (*s && *s != '\"')
						{
							s++;
						}
					}
					if (*s == '(')
						Depth++;
					else if (*s == ')')
					{
						Depth--;
						if (Depth == 0)
							break;
					}
					else if (!*s)
						break;
					s++;
				}			
				
				if (*s != ')')
					return 0;
				Ranges.New().Set(Start - Buffer, s - Buffer);
				s++;
			}
			else
			{
				if (*s == '\'' || *s == '\"')
				{
					//char *Begin = s;
					char Delim = *s++;
					Start = s;
					while (*s && *s != Delim)
						s++;
					if (*s == Delim)
					{
						Ranges.New().Set(Start - Buffer, s - Buffer);
						s++;
					}
					else
					{
						// Parse error;
						return 0;
					}
				}
				else
				{
					Start = s;
					while (*s)
					{
						if (strchr(WhiteSpace, *s) || *s == ')')
							break;

						s++;
					}
					Ranges.New().Set(Start - Buffer, s - Buffer);
				}
			}
		}
	}

	LgiAssert(s <= End);
	return MsgSize;
}

////////////////////////////////////////////////////////////////////////////
bool MailIMap::Http(GSocketI *S,
					GAutoString *OutHeaders,
					GAutoString *OutBody,
					int *StatusCode,
					const char *InMethod,
					const char *InUri,
					const char *InHeaders,
					const char *InBody)
{
	if (!S || !InUri)
		return false;

	GStringPipe p(256);
	GUri u(InUri);

	p.Print("POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-length: %i\r\n"
			"\r\n%s",
			u.Path,
			u.Host,
			InBody?strlen(InBody):0,
			InBody?InBody:"");

	GAutoString Req(p.NewStr());
	int ReqLen = strlen(Req);

	if (!S->Open(u.Host, u.Port?u.Port:443))
		return false;

	int w = S->Write(Req, ReqLen);
	if (w != ReqLen)
		return false;

	char Buf[256];
	GArray<char> Res;
	int r;
	int ContentLen = 0;
	uint32 HdrLen = 0;
	while ((r = S->Read(Buf, sizeof(Buf))) > 0)
	{
		int Old = Res.Length();
		Res.Length(Old + r);
		memcpy(&Res[Old], Buf, r);

		if (ContentLen)
		{
			if (Res.Length() >= HdrLen + ContentLen)
				break;
		}
		else
		{
			char *Eoh = strnstr(&Res[0], "\r\n\r\n", Res.Length());
			if (Eoh)
			{
				HdrLen = Eoh - &Res[0];
				GAutoString c(InetGetHeaderField(&Res[0], "Content-Length", HdrLen));
				if (c)
				{
					ContentLen = atoi(c);
				}
			}
		}
	}

	char *Rp = &Res[0];
	char *Eoh = strnstr(Rp, "\r\n\r\n", Res.Length());
	if (Eoh)
	{
		if (OutHeaders)
			OutHeaders->Reset(NewStr(Rp, Eoh-Rp));
		if (OutBody)
			OutBody->Reset(NewStr(Eoh + 4, Res.Length() - (Eoh-Rp) - 4));
		if (StatusCode)
		{
			*StatusCode = 0;

			char *Eol = strchr(Rp, '\n');
			if (Eol)
			{
				GToken t(Rp, " \t\r\n", true, Eol - Rp);
				if (t.Length() > 2)
				{
					*StatusCode = atoi(t[1]);
				}
			}
		}
	}
	else return false;

	#ifndef _DEBUG
	GFile f;
	if (f.Open("c:\\temp\\http.html", O_WRITE))
	{
		f.SetSize(0);
		f.Write(&Res[0], Res.Length());
		f.Close();
	}
	#endif

	return true;
}

GAutoString ImapBasicTokenize(char *&s)
{
	if (s)
	{
		while (*s && strchr(WhiteSpace, *s)) s++;
		
		char start = 0, end = 0;
		if (*s == '\'' || *s == '\"') start = end = *s;
		else if (*s == '[') { start = '['; end = ']'; }
		else if (*s == '(') { start = '('; end = ')'; }
		else if (*s == '{') { start = '{'; end = '}'; }

		if (start && end)
		{
			s++;
			char *e = strchr(s, end);
			if (e)
			{
				char *n = NewStr(s, e - s);
				s = e + 1;
				return GAutoString(n);
			}
		}
		else
		{
			char *e = s;
			while (*e && !strchr(WhiteSpace, *e)) e++;
			if (e > s)
			{
				char *n = NewStr(s, e - s);
				s = e + (*e != 0);
				return GAutoString(n);
			}
		}
	}

	s += strlen(s);
	return GAutoString();
}

char *Tok(char *&s)
{
	char *Ret = 0;

	while (*s && strchr(WhiteSpace, *s))
		s++;

	if (*s == '=' || *s == ',')
	{
		Ret = NewStr(s++, 1);
	}
	else if (*s == '\'' || *s == '\"')
	{
		char d = *s++;
		char *e = strchr(s, d);
		if (e)
		{
			Ret = NewStr(s, e - s);
			s = e + 1;
		}
	}
	else if (*s)
	{
		char *e;
		for (e=s; *e && (IsDigit(*e) || IsAlpha(*e) || *e == '-'); e++);
		Ret = NewStr(s, e - s);
		s = e;
	}

	return Ret;
}

char *DecodeImapString(const char *s)
{
	GStringPipe p;
	while (s && *s)
	{
		if (*s == '&')
		{
			char Escape = *s++;
			
			const char *e = s;
			while (*e && *e != '-')
			{
				e++;
			}

			int Len = e - s;
			if (Len)
			{
				char *Base64 = new char[Len + 4];
				if (Base64)
				{
					memcpy(Base64, s, Len);
					char *n = Base64 + Len;
					for (int i=Len; i%4; i++)
						*n++ = '=';
					*n++ = 0;

					Len = strlen(Base64);
					int BinLen = BufferLen_64ToBin(Len);
					uint16 *Bin = new uint16[(BinLen/2)+1];
					if (Bin)
					{
						BinLen = ConvertBase64ToBinary((uchar*)Bin, BinLen, Base64, Len);
						if (BinLen)
						{
							int Chars = BinLen / 2;
							Bin[Chars] = 0;
							for (int i=0; i<Chars; i++)
							{
								Bin[i] = (Bin[i]>>8) | ((Bin[i]&0xff)<<8);
							}

							char *c8 = WideToUtf8((char16*)Bin, BinLen);
							if (c8)
							{
								p.Push(c8);
								DeleteArray(c8);
							}
						}
						DeleteArray(Bin);
					}
					
					DeleteArray(Base64);
				}
			}
			else
			{
				p.Push(&Escape, 1);
			}

			s = e + 1;
		}
		else
		{
			p.Push(s, 1);
			s++;
		}
	}
	return p.NewStr();
}

char *EncodeImapString(const char *s)
{
	GStringPipe p;
	ssize_t Len = s ? strlen(s) : 0;
	
	while (s && *s)
	{
		int c = LgiUtf8To32((uint8*&)s, Len);

		DoNextChar:
		if ((c >= ' ' && c < 0x80) ||
			c == '\n' ||
			c == '\t' ||
			c == '\r')
		{
			// Literal
			char ch = c;
			p.Push(&ch, 1);
		}
		else
		{
			// Encoded
			GArray<uint16> Str;
			Str[0] = c;
			while ((c = LgiUtf8To32((uint8*&)s, Len)))
			{
				if ((c >= ' ' && c < 0x80) ||
					c == '\n' ||
					c == '\t' ||
					c == '\r')
				{
					break;
				}
				else
				{
					Str[Str.Length()] = c;
				}
			}

			for (uint32 i=0; i<Str.Length(); i++)
			{
				Str[i] = (Str[i]>>8) | ((Str[i]&0xff)<<8);
			}
			int BinLen = Str.Length() << 1;
			int BaseLen = BufferLen_BinTo64(BinLen);
			char *Base64 = new char[BaseLen+1];
			if (Base64)
			{
				int Bytes = ConvertBinaryToBase64(Base64, BaseLen, (uchar*)&Str[0], BinLen);
				while (Bytes > 0 && Base64[Bytes-1] == '=')
				{
					Base64[Bytes-1] = 0;
					Bytes--;
				}
				Base64[Bytes] = 0;

				p.Print("&%s-", Base64);
				DeleteArray(Base64);
			}

			goto DoNextChar;
		}
	}

	return p.NewStr();
}

void ChopNewLine(char *Str)
{
	char *End = Str+strlen(Str)-1;
	if (*End == '\n')
	{
		*End-- = 0;
	}
	if (*End == '\r')
	{
		*End-- = 0;
	}
}

MailImapFolder::MailImapFolder()
{
	Sep = '/';
	Path = 0;
	NoSelect = false;
	NoInferiors = false;
	Marked = false;
	Exists = -1;
	Recent = -1;
	// UnseenIndex = -1;
	Deleted = 0;
}

MailImapFolder::~MailImapFolder()
{
	DeleteArray(Path);
}

void MailImapFolder::operator =(GHashTbl<const char*,int> &v)
{
	int o = v.Find("exists");
	if (o >= 0) Exists = o;
	o = v.Find("recent");
	if (o >= 0) Recent = o;
}

char *MailImapFolder::GetPath()
{
	return Path;
}

void MailImapFolder::SetPath(const char *s)
{
	char *NewPath = DecodeImapString(s);
	DeleteArray(Path);
	Path = NewPath;
}

char *MailImapFolder::GetName()
{
	if (Path)
	{
		char *s = strrchr(Path, Sep);
		if (s)
		{
			return s + 1;
		}
		else
		{
			return Path;
		}
	}

	return 0;
}

void MailImapFolder::SetName(const char *s)
{
	if (s)
	{
		char Buf[256];
		strcpy_s(Buf, sizeof(Buf), Path?Path:(char*)"");
		DeleteArray(Path);

		char *Last = strrchr(Buf, Sep);
		if (Last)
		{
			Last++;
			strcpy_s(Last, sizeof(Buf)-(Last-Buf), s);
			Path = NewStr(Buf);
		}
		else
		{
			Path = NewStr(s);
		}
	}
}

/////////////////////////////////////////////
class MailIMapPrivate : public LMutex
{
public:
	int NextCmd;
	bool Logging;
	bool ExpungeOnExit;
	char FolderSep;
	char *Current;
	char *Flags;
	GHashTbl<char*,bool> Capability;
	GString WebLoginUri;
	MailIMap::OAuthParams OAuth;
	GViewI *ParentWnd;
	bool *LoopState;
	OsThread InCommand;
	GString LastWrite;

	MailIMapPrivate() : LMutex("MailImapSem")
	{
		ParentWnd = NULL;
		LoopState = NULL;
		FolderSep = '/';
		NextCmd = 1;
		Logging = true;
		ExpungeOnExit = true;
		Current = 0;
		Flags = 0;
		InCommand = 0;
	}

	~MailIMapPrivate()
	{
		DeleteArray(Current);
		DeleteArray(Flags);
	}
};

MailIMap::MailIMap()
{
	d = new MailIMapPrivate;
	Buffer[0] = 0;
}

MailIMap::~MailIMap()
{
	if (Lock(_FL))
	{
		ClearDialog();
		ClearUid();
		DeleteObj(d);
	}
}

bool MailIMap::Lock(const char *file, int line)
{
	if (!d->Lock(file, line))
		return false;
	return true;
}

bool MailIMap::LockWithTimeout(int Timeout, const char *file, int line)
{
	if (!d->LockWithTimeout(Timeout, file, line))
		return false;
	return true;
}

void MailIMap::Unlock()
{
	d->Unlock();
	d->InCommand = 0;
}

void MailIMap::SetLoopState(bool *LoopState)
{
	d->LoopState = LoopState;
}

void MailIMap::SetParentWindow(GViewI *wnd)
{
	d->ParentWnd = wnd;
}

void MailIMap::SetOAuthParams(OAuthParams &p)
{
	d->OAuth = p;
}

const char *MailIMap::GetWebLoginUri()
{
	return d->WebLoginUri;
}

bool MailIMap::IsOnline()
{
	return Socket ? Socket->IsOpen() : 0;
}

char MailIMap::GetFolderSep()
{
	return d->FolderSep;
}

char *MailIMap::GetCurrentPath()
{
	return d->Current;
}

bool MailIMap::GetExpungeOnExit()
{
	return d->ExpungeOnExit;
}

void MailIMap::SetExpungeOnExit(bool b)
{
	d->ExpungeOnExit = b; 
}

void MailIMap::ClearUid()
{
	if (Lock(_FL))
	{
		Uid.DeleteArrays();
		Unlock();
	}
}

void MailIMap::ClearDialog()
{
	if (Lock(_FL))
	{
		Dialog.DeleteArrays();
		Unlock();
	}
}

bool MailIMap::WriteBuf(bool ObsurePass, const char *Buffer, bool Continuation)
{
	if (Socket)
	{
		if (!Buffer)
			Buffer = Buf;

		int Len = strlen(Buffer);
		d->LastWrite = Buffer;
		if (!Continuation && d->InCommand)
		{
			GString Msg;
			Msg.Printf("%s:%i - WriteBuf failed(%s)\n", LgiGetLeaf(__FILE__), __LINE__, d->LastWrite.Strip().Get());
			Socket->OnInformation(Msg);

			LgiAssert(!"Can't be issuing new commands while others are still running.");
			return false;
		}
		/*
		else
		{
			GString Msg;
			Msg.Printf("%s:%i - WriteBuf ok(%s)\n", LgiGetLeaf(__FILE__), __LINE__, d->LastWrite.Strip().Get());
			Socket->OnInformation(Msg);
		}
		*/

		if (Socket->Write((void*)Buffer, Len, 0) == Len)
		{
			if (ObsurePass)
			{
				char *Sp = (char*)strrchr(Buffer, ' ');
				if (Sp)
				{
					Sp++;
					GString s;
					s.Printf("%.*s********\r\n", Sp - Buffer, Buffer);
					Log(s.Get(), GSocketI::SocketMsgSend);
				}
			}
			else Log(Buffer, GSocketI::SocketMsgSend);
			
			d->InCommand = LgiGetCurrentThread();

			return true;
		}
		else Log("Failed to write data to socket.", GSocketI::SocketMsgError);
	}
	else Log("Not connected.", GSocketI::SocketMsgError);

	return false;
}

bool MailIMap::Read(GStreamI *Out)
{
	int Lines = 0;

	while (!Lines && Socket)
	{
		int r = Socket->Read(Buffer, sizeof(Buffer));
		if (r > 0)
		{
			ReadBuf.Push(Buffer, r);
			while (ReadBuf.Pop(Buffer, sizeof(Buffer)))
			{
				// Trim trailing whitespace
				char *e = Buffer + strlen(Buffer) - 1;
				while (e > Buffer && strchr(WhiteSpace, *e))
					*e-- = 0;

				Lines++;

				if (Out)
				{
					Out->Write(Buffer, strlen(Buffer));
					Out->Write((char*)"\r\n", 2);
				}
				else
				{
					Dialog.Add(NewStr(Buffer));
				}
			}
		}
		else
		{
			// LgiTrace("%s:%i - Socket->Read failed: %i\n", _FL, r);
			break;
		}
	}

	return Lines > 0;
}

bool MailIMap::IsResponse(const char *Buf, int Cmd, bool &Ok)
{
	char Num[8];
	int Ch = sprintf_s(Num, sizeof(Num), "A%4.4i ", Cmd);
	if (!Buf || _strnicmp(Buf, Num, Ch) != 0)
		return false;

	Ok = _strnicmp(Buf+Ch, "OK", 2) == 0;
	if (!Ok)
		SetError(L_ERROR_GENERIC, "Error: %s", Buf+Ch);
	
	return true;
}

bool MailIMap::ReadResponse(int Cmd, bool Plus)
{
	bool Done = false;
	bool Status = false;
	if (Socket)
	{
		int Pos = Dialog.Length();
		while (!Done)
		{
			if (Read(NULL))
			{
				for (char *Dlg = Dialog[Pos]; !Done && Dlg; Dlg = Dialog.Next())
				{
					Pos++;
					if (Cmd < 0 || (Plus && *Dlg == '+'))
					{
						Status = Done = true;
					}

					if (IsResponse(Dlg, Cmd, Status))
						Done = true;
					
					if (d->Logging)
						Log(Dlg, GSocketI::SocketMsgReceive);
				}
			}
			else
			{
				// LgiTrace("%s:%i - 'Read' failed.\n", _FL);
				break;
			}
		}
	}

	return Status;
}

void Hex(char *Out, int OutLen, uchar *In, int InLen = -1)
{
	if (Out && In)
	{
		if (InLen < 0)
			InLen = strlen((char*)In);

		for (int i=0; i<InLen; i++)
		{
			int ch = sprintf_s(Out, OutLen, "%2.2x", *In++);
			if (ch > 0)
			{
				Out += ch;
				OutLen -= ch;
			}
			else break;
		}
	}
}

void _unpack(void *ptr, int ptrsize, char *b64)
{
	int c = ConvertBase64ToBinary((uchar*) ptr, ptrsize, b64, strlen(b64));
}

bool MailIMap::ReadLine()
{
	int Len = 0;
	Buf[0] = 0;
	do
	{
		int r = Socket->Read(Buf+Len, sizeof(Buf)-Len);
		if (r < 1)
			return false;
		Len += r;
	}
	while (!stristr(Buf, "\r\n"));

	Log(Buf, GSocketI::SocketMsgReceive);
	return true;
}

#if HAS_LIBGSASL
int GsaslCallback(Gsasl *ctx, Gsasl_session *sctx, Gsasl_property prop)
{
	return 0;
}
#endif

class OAuthWebServer : public LThread, public LMutex
{
	bool Loop;
	int Port;
	GSocket Listen;
	GAutoString Req;
	GString Resp;
	bool Finished;

public:
	OAuthWebServer(int DesiredPort = 0) :
		LThread("OAuthWebServerThread"),
		LMutex("OAuthWebServerMutex")
	{
		Loop = true;
		if (Listen.Listen(DesiredPort))
		{
			Port = Listen.GetLocalPort();
			Run();
		}
		else Port = 0;
		Finished = false;
	}
	
	~OAuthWebServer()
	{
		Loop = false;
		while (!IsExited())
			LgiSleep(10);
	}
	
	int GetPort()
	{
		return Port;
	}

	GString GetRequest(bool *Loop)
	{
		GString r;
		
		while (!r && (!Loop || *Loop))
		{
			if (Lock(_FL))
			{
				if (Req)
					r = Req;
				Unlock();
			}
			
			if (!r)
				LgiSleep(50);
		}
		
		return r;
	}
	
	void SetResponse(const char *r)
	{
		if (Lock(_FL))
		{
			Resp = r;
			Unlock();
		}
	}
	
	bool IsFinished()
	{
		return Finished;
	}
	
	int Main()
	{
		GAutoPtr<GSocket> s;
		while (Loop)
		{
			if (Listen.CanAccept(100))
			{
				s.Reset(new GSocket);
				
				if (!Listen.Accept(s))
					s.Reset();
				else
				{
					GArray<char> Mem;
					int r;
					char buf[512];
					do 
					{
						r = s->Read(buf, sizeof(buf));
						if (r > 0)
						{
							Mem.Add(buf, r);
							bool End = strnstr(&Mem[0], "\r\n\r\n", Mem.Length()) != NULL;
							if (End)
								break;
						}	
					}
					while (r > 0);
					
					if (Lock(_FL))
					{
						Mem.Add(0);
						Req.Reset(Mem.Release());
						Unlock();
					}
					
					// Wait for the response...
					GString Response;
					do
					{
						if (Lock(_FL))
						{
							if (Resp)
								Response = Resp;
							Unlock();
						}
						if (!Response)
							LgiSleep(10);
					}
					while (Loop && !Response);
					
					if (Response)
						s->Write(Response, Response.Length());
					Loop = false;
				}
			}
			else LgiSleep(10);
		}

		Finished = true;
		return 0;
	}
};

static void AddIfMissing(GArray<GString> &Auths, const char *a, GString *DefaultAuthType = NULL)
{
	for (unsigned i=0; i<Auths.Length(); i++)
	{
		if (!_stricmp(Auths[i], a))
		{
			if (DefaultAuthType) *DefaultAuthType = Auths[i];
			return;
		}
	}
	Auths.Add(a);
	if (DefaultAuthType) *DefaultAuthType = Auths.Last();
}

void MailIMap::CommandFinished()
{
	/*
	GString Msg;
	Msg.Printf("%s:%i - CommandFinished(%s)\n", LgiGetLeaf(__FILE__), __LINE__, d->LastWrite.Strip().Get());
	Socket->OnInformation(Msg);
	*/

	d->InCommand = 0;
	d->LastWrite.Empty();
}

bool MailIMap::Open(GSocketI *s, const char *RemoteHost, int Port, const char *User, const char *Password, GDom *SettingStore, int Flags)
{
	bool Status = false;

	if (SocketLock.Lock(_FL))
	{
		Socket.Reset(s);
		SocketLock.Unlock();
	}
	if (Socket &&
		ValidStr(RemoteHost) &&
		ValidStr(User) &&
		ValidStr(Password) &&
		Lock(_FL))
	{
		// prepare address
		if (Port < 1)
		{
			if (Flags & MAIL_SSL)
				Port = IMAP_SSL_PORT;
			else
				Port = IMAP_PORT;
		}

		char Remote[256];
		strcpy_s(Remote, sizeof(Remote), RemoteHost);
		char *Colon = strchr(Remote, ':');
		if (Colon)
		{
			*Colon++ = 0;
			Port = atoi(Colon);
		}

		// Set SSL mode
		GVariant v;
		if (Flags == MAIL_SSL)
			v = "SSL";
		Socket->SetValue(GSocket_Protocol, v);

		// connect
		if (Socket->Open(Remote, Port))
		{
			bool IMAP4Server = false;
			GArray<GString> Auths;

			// check capability
			int CapCmd = d->NextCmd++;
			sprintf_s(Buf, sizeof(Buf), "A%4.4i CAPABILITY\r\n", CapCmd);
			if (WriteBuf())
			{
				bool Rd = ReadResponse(CapCmd);
				CommandFinished();
				if (Rd)
				{
					for (char *r=Dialog.First(); r; r=Dialog.Next())
					{
						GToken T(r, " ");
						if (T.Length() > 1 &&
							_stricmp(T[1], "CAPABILITY") == 0)
						{
							for (unsigned i=2; i<T.Length(); i++)
							{
								if (_stricmp(T[i], "IMAP4") == 0 ||
									_stricmp(T[i], "IMAP4rev1") == 0)
								{
									IMAP4Server = true;
								}
								if (_strnicmp(T[i], "AUTH=", 5) == 0)
								{
									char *Type = T[i] + 5;
									AddIfMissing(Auths, Type);
								}

								d->Capability.Add(T[i], true);
							}
						}
					}

					ClearDialog();
				}
				else Log("Read CAPABILITY response failed", GSocketI::SocketMsgError);
			}
			else Log("Write CAPABILITY cmd failed", GSocketI::SocketMsgError);

			if (!IMAP4Server)
				Log("CAPABILITY says not an IMAP4Server", GSocketI::SocketMsgError);
			else
			{
				GString DefaultAuthType;
				bool TryAllAuths =	Auths.Length() == 0
									&&
									!(
										Flags &
										(
											MAIL_USE_PLAIN |
											MAIL_USE_LOGIN |
											MAIL_USE_NTLM | 
											MAIL_USE_OAUTH2
										)
									);

				if (TryAllAuths)
					AddIfMissing(Auths, "DIGEST-MD5");
				if (TestFlag(Flags, MAIL_USE_PLAIN) || TryAllAuths)
					AddIfMissing(Auths, "PLAIN", &DefaultAuthType);
				if (TestFlag(Flags, MAIL_USE_LOGIN) || TryAllAuths)
					AddIfMissing(Auths, "LOGIN", &DefaultAuthType);
				if (TestFlag(Flags, MAIL_USE_NTLM) || TryAllAuths)
					AddIfMissing(Auths, "NTLM", &DefaultAuthType);
				if (TestFlag(Flags, MAIL_USE_OAUTH2) || TryAllAuths)
					AddIfMissing(Auths, "XOAUTH2", &DefaultAuthType);
					
				// Move the default to the start of the list
				for (unsigned n=0; n<Auths.Length(); n++)
				{
					if (Auths[n] == DefaultAuthType && n > 0)
					{
						Auths.DeleteAt(n);
						Auths.AddAt(0, DefaultAuthType);
						break;
					}
				}

				// SSL
				bool TlsError = false;
				if (TestFlag(Flags, MAIL_USE_STARTTLS))
				{
					int CapCmd = d->NextCmd++;
					sprintf_s(Buf, sizeof(Buf), "A%4.4i STARTTLS\r\n", CapCmd);					
					if (WriteBuf())
					{
						bool Rd = ReadResponse(CapCmd);
						CommandFinished();
						if (Rd)
						{
							GVariant v;
							TlsError = !Socket->SetValue(GSocket_Protocol, v="SSL");
						}
						else
						{
							TlsError = true;
						}
					}
					else LgiAssert(0);
					if (TlsError)
					{
						Log("STARTTLS failed", GSocketI::SocketMsgError);
					}
				}

				// login
				bool LoggedIn = false;
				char AuthTypeStr[256] = "";
				for (unsigned i=0; i<Auths.Length() && !TlsError && !LoggedIn; i++)
				{
					const char *AuthType = Auths[i];

					if (DefaultAuthType != AuthType)
					{
						// put a string of auth types together
						if (strlen(AuthTypeStr) > 0)
						{
							strconcat(AuthTypeStr, ", ");
						}
						strconcat(AuthTypeStr, AuthType);
					}

					// Do auth
					#if HAS_LIBGSASL
					if (!_stricmp(AuthType, "GSSAPI"))
					{
						int AuthCmd = d->NextCmd++;
						sprintf_s(Buf, sizeof(Buf), "A%04.4i AUTHENTICATE GSSAPI\r\n", AuthCmd);
						if (WriteBuf() &&
							ReadLine() &&
							Buf[0] == '+')
						{
							// Start GSSAPI
							Gsasl *ctx = NULL;
							Gsasl_session *sess = NULL;
							int rc = gsasl_init(&ctx);
							if (rc == GSASL_OK)
							{
								char *mechs;
								rc = gsasl_client_mechlist(ctx, &mechs);
  								gsasl_callback_set(ctx, GsaslCallback);
								rc = gsasl_client_start(ctx, AuthType, &sess);
								if (rc != GSASL_OK)
								{
									Log("gsasl_client_start failed", GSocketI::SocketMsgError);
								}

								// gsasl_step(ctx, 
								gsasl_done(ctx);
							}
							else Log("gsasl_init failed", GSocketI::SocketMsgError);
						}						
						else Log("AUTHENTICATE GSSAPI failed", GSocketI::SocketMsgError);
					}
					else
					#endif
					if (_stricmp(AuthType, "LOGIN") == 0 ||
						_stricmp(AuthType, "OTP") == 0)
					{
						// clear text authentication
						int AuthCmd = d->NextCmd++;
						sprintf_s(Buf, sizeof(Buf), "A%4.4i LOGIN %s %s\r\n", AuthCmd, User, Password);
						if (WriteBuf(true))
						{
							LoggedIn = ReadResponse(AuthCmd);
							CommandFinished();
						}
					}
					else if (_stricmp(AuthType, "PLAIN") == 0)
					{
						// plain auth type
						char s[256];
						char *e = s;
						*e++ = 0;
						strcpy_s(e, sizeof(s)-(e-s), User);
						e += strlen(e);
						e++;
						strcpy_s(e, sizeof(s)-(e-s), Password);
						e += strlen(e);
						*e++ = '\r';
						*e++ = '\n';
						int Len = e - s - 2;					

						int AuthCmd = d->NextCmd++;
						sprintf_s(Buf, sizeof(Buf), "A%4.4i AUTHENTICATE PLAIN\r\n", AuthCmd);
						if (WriteBuf())
						{
							if (ReadResponse(AuthCmd, true))
							{
								int b = ConvertBinaryToBase64(Buf, sizeof(Buf), (uchar*)s, Len);
								strcpy_s(Buf+b, sizeof(Buf)-b, "\r\n");
								if (WriteBuf(false, NULL, true))
								{
									bool Rd = ReadResponse(AuthCmd);
									CommandFinished();
									if (Rd)
									{
										LoggedIn = true;
									}
									else
									{
										// Look for WEBALERT from Google
										for (char *s = Dialog.First(); s; s = Dialog.Next())
										{
											char *start = strchr(s, '[');
											char *end = start ? strrchr(start, ']') : NULL;
											if (start && end)
											{
												start++;
												if (_strnicmp(start, "WEBALERT", 8) == 0)
												{
													start += 8;
													while (*start && strchr(WhiteSpace, *start))
														start++;
													
													d->WebLoginUri.Set(start, end - start);
												}
											}
										}
									}
								}
							}
						}						
					}
					#if (GPL_COMPATIBLE || defined(_LIBNTLM_H)) && defined(WINNATIVE)
					else if (_stricmp(AuthType, "NTLM") == 0)
					{
						// NT Lan Man authentication
						OSVERSIONINFO ver;
						ZeroObj(ver);
						ver.dwOSVersionInfoSize = sizeof(ver);
						if (!GetVersionEx(&ver))
						{
							DWORD err = GetLastError();
							Log("Couldn't get OS version", GSocketI::SocketMsgError);
						}
						else
						{
							// Username is in the format: User[@Domain]
							char UserDom[256];
							strcpy_s(UserDom, sizeof(UserDom), User);
							char *Domain = strchr(UserDom, '@');
							if (Domain)
								*Domain++ = 0;

							int AuthCmd = d->NextCmd++;
							sprintf_s(Buf, sizeof(Buf), "A%04.4i AUTHENTICATE NTLM\r\n", AuthCmd);
							if (WriteBuf())
							{
								if (ReadResponse(AuthCmd, true))
								{
									tSmbNtlmAuthNegotiate	negotiate;              
									tSmbNtlmAuthChallenge	challenge;
									tSmbNtlmAuthResponse	response;

									buildSmbNtlmAuthNegotiate(&negotiate, 0, 0);
									if (NTLM_VER(&negotiate) == 2)
									{
										negotiate.v2.version.major = (uint8) ver.dwMajorVersion;
										negotiate.v2.version.minor = (uint8) ver.dwMinorVersion;
										negotiate.v2.version.buildNumber = (uint16) ver.dwBuildNumber;
										negotiate.v2.version.ntlmRevisionCurrent = 0x0f;
									}

									ZeroObj(Buf);
									int negotiateLen = SmbLength(&negotiate);
									int c = ConvertBinaryToBase64(Buf, sizeof(Buf), (uchar*)&negotiate, negotiateLen);
									strcpy_s(Buf+c, sizeof(Buf)-c, "\r\n");
									WriteBuf(false, NULL, true);

									/* read challange data from server, convert from base64 */

									Buf[0] = 0;
									ClearDialog();
									if (ReadResponse())
									{
										/* buffer should contain the string "+ [base 64 data]" */

										#if 1
											ZeroObj(challenge);
											char *Line = Dialog.First();
											LgiAssert(Line != NULL);
											ChopNewLine(Line);
											int LineLen = strlen(Line);
											int challengeLen = sizeof(challenge);
											c = ConvertBase64ToBinary((uchar*) &challenge, sizeof(challenge), Line+2, LineLen-2);
											if (NTLM_VER(&challenge) == 2)
												challenge.v2.bufIndex = c - (challenge.v2.buffer-(uint8*)&challenge);
											else
												challenge.v1.bufIndex = c - (challenge.v1.buffer-(uint8*)&challenge);

										#endif


										/* prepare response, convert to base64, send to server */
										ZeroObj(response);
										FILETIME time = {0, 0};
										SYSTEMTIME stNow;
										GetSystemTime(&stNow);
										SystemTimeToFileTime(&stNow, &time);

									    char HostName[256] = "";
										gethostname(HostName, sizeof(HostName));

										buildSmbNtlmAuthResponse(&challenge,
																&response,
																UserDom,
																HostName,
																Domain,
																Password,
																(uint8*)&time);
										if (NTLM_VER(&response) == 2)
										{
											response.v2.version.major = (uint8) ver.dwMajorVersion;
											response.v2.version.minor = (uint8) ver.dwMinorVersion;
											response.v2.version.buildNumber = (uint16) ver.dwBuildNumber;
											response.v2.version.ntlmRevisionCurrent = 0x0f;
										}
										
										#if 0
										{
											uint8 *r1 = (uint8*)&response;
											uint8 *r2 = (uint8*)&response_good;
											for (int i=0; i<sizeof(response); i++)
											{
												if (r1[i] != r2[i])
												{
													int asd=0;
												}
											}
										}
										#endif
										
										ZeroObj(Buf);
										c = ConvertBinaryToBase64(Buf, sizeof(Buf), (uchar*) &response, SmbLength(&response));
										strcat(Buf, "\r\n");
										WriteBuf(false, NULL, true);

										/* read line from server, it should be "[seq] OK blah blah blah" */
										LoggedIn = ReadResponse(AuthCmd);
									}
								}

								CommandFinished();
							}
						}

						ClearDialog();
					}
					#endif
					else if (_stricmp(AuthType, "DIGEST-MD5") == 0)
					{
						int AuthCmd = d->NextCmd++;
						sprintf_s(Buf, sizeof(Buf), "A%4.4i AUTHENTICATE DIGEST-MD5\r\n", AuthCmd);
						if (WriteBuf())
						{
							if (ReadResponse(AuthCmd))
							{
								char *TestCnonce = 0;
								
								#if 0
								// Test case
								strcpy(Buf, "+ cmVhbG09ImVsd29vZC5pbm5vc29mdC5jb20iLG5vbmNlPSJPQTZNRzl0RVFHbTJoaCIscW9wPSJhdXRoIixhbGdvcml0aG09bWQ1LXNlc3MsY2hhcnNldD11dGYtOA==");
								RemoteHost = "elwood.innosoft.com";
								User = "chris";
								Password = "secret";
								TestCnonce = "OA6MHXh6VqTrRk";
								#endif

								char *In = (char*)Buf;
								if (In[0] == '+' && In[1] == ' ')
								{
									In += 2;

									uchar Out[2048];
									int b = ConvertBase64ToBinary(Out, sizeof(Out), In, strlen(In));
									Out[b] = 0;
									
									GHashTbl<const char*, char*> Map;
									char *s = (char*)Out;
									while (s && *s)
									{
										char *Var = Tok(s);
										char *Eq = Tok(s);
										char *Val = Tok(s);
										char *Comma = Tok(s);

										if (Var && Eq && Val && strcmp(Eq, "=") == 0)
										{
											Map.Add(Var, Val);
											Val = 0;
										}

										DeleteArray(Var);
										DeleteArray(Eq);
										DeleteArray(Val);
										DeleteArray(Comma);
									}

									int32 CnonceI[2] = { (int32)LgiRand(), (int32)LgiRand() };
									char Cnonce[32];
									if (TestCnonce)
										strcpy_s(Cnonce, sizeof(Cnonce), TestCnonce);
									else
										Cnonce[ConvertBinaryToBase64(Cnonce, sizeof(Cnonce), (uchar*)&CnonceI, sizeof(CnonceI))] = 0;
									s = strchr(Cnonce, '=');
									if (s) *s = 0;

									int Nc = 1;
									char *Realm = Map.Find("realm");
									char DigestUri[256];
									sprintf_s(DigestUri, sizeof(DigestUri), "imap/%s", Realm ? Realm : RemoteHost);

									GStringPipe p;
									p.Print("username=\"%s\"", User);
									p.Print(",nc=%08.8i", Nc);
									p.Print(",digest-uri=\"%s\"", DigestUri);
									p.Print(",cnonce=\"%s\"", Cnonce);
									char *Nonce = Map.Find("nonce");
									if (Nonce)
									{
										p.Print(",nonce=\"%s\"", Nonce);
									}
									if (Realm)
									{
										p.Print(",realm=\"%s\"", Realm);
									}
									char *Charset = Map.Find("charset");
									if (Charset)
									{
										p.Print(",charset=%s", Charset);
									}
									char *Qop = Map.Find("qop");
									if (Qop)
									{
										p.Print(",qop=%s", Qop);
									}

									// Calculate A1
									char a1[256];
									uchar md5[16];
									sprintf_s(Buf, sizeof(Buf), "%s:%s:%s", User, Realm ? Realm : (char*)"", Password);
									MDStringToDigest((uchar*)a1, Buf);
									char *Authzid = Map.Find("authzid");
									int ch = 16;
									if (Authzid)
										ch += sprintf_s(a1+ch, sizeof(a1)-ch, ":%s:%s:%s", Nonce, Cnonce, Authzid);
									else
										ch += sprintf_s(a1+ch, sizeof(a1)-ch, ":%s:%s", Nonce, Cnonce);
									MDStringToDigest(md5, a1, ch);
									char a1hex[256];
									Hex(a1hex, sizeof(a1hex), (uchar*)md5, sizeof(md5));

									// Calculate 
									char a2[256];
									if (Qop && (_stricmp(Qop, "auth-int") == 0 || _stricmp(Qop, "auth-conf") == 0))
										sprintf_s(a2, sizeof(a2), "AUTHENTICATE:%s:00000000000000000000000000000000", DigestUri);
									else
										sprintf_s(a2, sizeof(a2), "AUTHENTICATE:%s", DigestUri);
									MDStringToDigest(md5, a2);
									char a2hex[256];
									Hex(a2hex, sizeof(a2hex), (uchar*)md5, sizeof(md5));

									// Calculate the final response
									sprintf_s(Buf, sizeof(Buf), "%s:%s:%8.8i:%s:%s:%s", a1hex, Nonce, Nc, Cnonce, Qop, a2hex);
									MDStringToDigest(md5, Buf);
									Hex(Buf, sizeof(Buf), (uchar*)md5, sizeof(md5));
									p.Print(",response=%s", Buf);
									if ((s = p.NewStr()))
									{
										int Chars = ConvertBinaryToBase64(Buf, sizeof(Buf) - 4, (uchar*)s, strlen(s));
										LgiAssert(Chars < sizeof(Buf));
										strcpy_s(Buf+Chars, sizeof(Buf)-Chars, "\r\n");
										
										if (WriteBuf(false, NULL, true) && Read())
										{
											for (char *Dlg = Dialog.First(); Dlg; Dlg=Dialog.Next())
											{
												if (Dlg[0] == '+' && Dlg[1] == ' ')
												{
													Log(Dlg, GSocketI::SocketMsgReceive);
													strcpy_s(Buf, sizeof(Buf), "\r\n");
													if (WriteBuf(false, NULL, true))
													{
														LoggedIn = ReadResponse(AuthCmd);
													}
												}
												else
												{
													Log(Dlg, GSocketI::SocketMsgError);
													break;
												}
											}
										}
										
										DeleteArray(s);
									}
								}
							}
							
							CommandFinished();
						}
					}
					else if (!_stricmp(AuthType, "XOAUTH2"))
					{
						if (!d->OAuth.IsValid())
						{
							sprintf_s(Buf, sizeof(Buf), "Error: Unknown OAUTH2 server '%s' (ask fret@memecode.com to fix)", RemoteHost);
							Log(Buf, GSocketI::SocketMsgError);
							continue;
						}

						GString Uri;
						GString RedirUri;
						GVariant AuthCode;
						if (SettingStore)
						{
							GVariant v;
							if (SettingStore->GetValue(OPT_ImapOAuth2AccessToken, v))
							{
								d->OAuth.AccessToken = v.Str();
							}
							
							#if DEBUG_OAUTH2
							LgiTrace("%s:%i - AuthCode=%s\n", _FL, AuthCode.Str());
							#endif
						}
						
						if (!d->OAuth.AccessToken)
						{						
							OAuthWebServer WebServer(55220);
						
							// Launch browser to get Access Token
							bool UsingLocalhost = WebServer.GetPort() > 0;
							#if DEBUG_OAUTH2
							LgiTrace("%s:%i - UsingLocalhost=%i\n", _FL, UsingLocalhost);
							#endif
							if (UsingLocalhost)
							{
								// In this case the local host webserver is successfully listening
								// on a port and ready to receive the redirect.
								RedirUri.Printf("http://localhost:%i", WebServer.GetPort());
								#if DEBUG_OAUTH2
								LgiTrace("%s:%i - RedirUri=%s\n", _FL, RedirUri.Get());
								#endif
							}
							else
							{
								// Something went wrong with the localhost web server and we need to
								// provide an alternative way of getting the AuthCode
								GString::Array a = d->OAuth.RedirURIs.Split("\n");
								for (unsigned i=0; i<a.Length(); i++)
								{
									if (a[i].Find("localhost") < 0)
									{
										RedirUri = a[i];
										break;
									}
								}
								#if DEBUG_OAUTH2
								LgiTrace("%s:%i - RedirUri=%s\n", _FL, RedirUri.Get());
								#endif
							}
							
							GUri u;
							GAutoString RedirEnc = u.Encode(RedirUri, ":/");
							
							Uri.Printf("%s?client_id=%s&redirect_uri=%s&response_type=code&scope=%s",
								d->OAuth.AuthUri.Get(),
								d->OAuth.ClientID.Get(),
								RedirEnc.Get(),
								d->OAuth.Scope.Get());
							#if DEBUG_OAUTH2
							LgiTrace("%s:%i - Uri=%s\n", _FL, Uri.Get());
							#endif
							bool ExResult = LgiExecute(Uri);
							#if DEBUG_OAUTH2
							LgiTrace("%s:%i - ExResult=%i\n", _FL, ExResult);
							#endif
							
							if (UsingLocalhost)
							{
								// Wait for localhost web server to receive the response
								GString Req = WebServer.GetRequest(d->LoopState);
								if (Req)
								{
									GXmlTag t;
									GString::Array a = Req.Split("\r\n");
									if (a.Length() > 0)
									{
										GString::Array p = a[0].Split(" ");
										if (p.Length() > 1)
										{
											int Q = p[1].Find("?");
											if (Q >= 0)
											{
												GString Params = p[1](Q+1, -1);
												a = Params.Split("&");
												for (unsigned i=0; i<a.Length(); i++)
												{
													GString::Array v = a[i].Split("=");
													if (v.Length() == 2)
													{
														t.SetAttr(v[0], v[1]);
													}												
												}
											}
										}
									}
									
									AuthCode = t.GetAttr("code");
									#if DEBUG_OAUTH2
									LgiTrace("%s:%i - AuthCode=%s\n", _FL, AuthCode.Str());
									#endif
									
									GString Resp;
									Resp.Printf("HTTP/1.1 200 OK\r\n"
												"Content-Type: text/html\r\n"
												"\r\n"
												"<html>\n"
												"<head>\n"
												"<script type=\"text/javascript\">\n"
												"    window.close();\n"
												"</script>\n"
												"</head>\n"
												"<body>OAuth2Client: %s</body>\n"
												"</html>\n",
												AuthCode.Str() ? "Received auth code OK" : "Failed to get auth code");
									
									WebServer.SetResponse(Resp);
									
									// Wait for the response to get sent...
									uint64 Start = LgiCurrentTime();
									while (!WebServer.IsFinished())
									{
										if (LgiCurrentTime() - Start > 5000)
											break;
										LgiSleep(50);
									}
								}
							}
							else
							{
								// Allow the user to paste the Auth Token in.
								GInput Dlg(d->ParentWnd, "", "Enter Authorization Token:", "IMAP OAuth2 Authentication");
								if (Dlg.DoModal())
								{
									AuthCode = Dlg.Str.Get();
									#if DEBUG_OAUTH2
									LgiTrace("%s:%i - AuthCode=%s\n", _FL, AuthCode.Str());
									#endif
								}
							}

							if (ValidStr(AuthCode.Str()) &&
								ValidStr(RedirUri))
							{
								// Now exchange the Auth Token for an Access Token (omg this is so complicated).
								Uri = d->OAuth.ApiUri;
								GUri u(Uri);
								
								IHttp Http;
								GStringPipe In, Out;
								In.Print("code=");
								StrFormEncode(In, AuthCode.Str(), true);
								In.Print("&redirect_uri=");
								StrFormEncode(In, RedirUri, true);
								In.Print("&client_id=");
								StrFormEncode(In, d->OAuth.ClientID, true);
								In.Print("&scope=");
								In.Print("&client_secret=");
								StrFormEncode(In, d->OAuth.ClientSecret, true);
								In.Print("&grant_type=authorization_code");
								
								if (d->OAuth.Proxy.Host)
								{
									Http.SetProxy(	d->OAuth.Proxy.Host,
													d->OAuth.Proxy.Port ? d->OAuth.Proxy.Port : HTTP_PORT);
									#if DEBUG_OAUTH2
									LgiTrace("%s:%i - d->OAuth.Proxy=%s:%i\n", _FL,
										d->OAuth.Proxy.Host,
										d->OAuth.Proxy.Port);
									#endif
								}
								
								SslSocket *ssl;
								GStringPipe OutputLog;
								GAutoPtr<GSocketI> Ssl(ssl = new SslSocket(&OutputLog));
								if (Ssl)
								{
									ssl->SetSslOnConnect(true);
									Ssl->SetTimeout(10 * 1000);
									if (Http.Open(	Ssl,
													u.Host,
													u.Port ? u.Port : HTTPS_PORT))
									{										
										int StatusCode = 0;
										int ContentLength = (int)In.GetSize();
										char Hdrs[256];
										sprintf_s(Hdrs, sizeof(Hdrs),
												"Content-Type: application/x-www-form-urlencoded\r\n"
												"Content-Length: %i\r\n",
												ContentLength);
										bool Result = Http.Post(Uri, &In, &StatusCode, &Out, NULL, Hdrs);
										GAutoString sOut(Out.NewStr());
										GXmlTag t;
										if (Result && JsonDecode(t, sOut))
										{
											d->OAuth.AccessToken = t.GetAttr("access_token");
											if (d->OAuth.AccessToken)
											{
												d->OAuth.RefreshToken = t.GetAttr("refresh_token");
												d->OAuth.ExpiresIn = t.GetAsInt("expires_in");
												#if DEBUG_OAUTH2
												LgiTrace("%s:%i - OAuth(AccessToken=%s, RefreshToken=%s, Expires=%i)\n",
													_FL,
													d->OAuth.AccessToken.Get(),
													d->OAuth.RefreshToken.Get(),
													d->OAuth.ExpiresIn);
												#endif
											}
											else
											{
												GString Err = t.GetAttr("error");
												if (Err)
												{
													GString Description = t.GetAttr("error_description");
													#if DEBUG_OAUTH2
													LgiTrace("%s:%i - Error: %s (%s)\n",
														_FL,
														Err.Get(),
														Description.Get());
													#endif
													sprintf_s(Buf, sizeof(Buf), "Error: %s / %s", Err.Get(), Description.Get());
													Log(Buf, GSocketI::SocketMsgWarning);
												}
											}
										}
										else
										{
											#if DEBUG_OAUTH2
											LgiTrace("%s:%i - Error getting JSON\n", _FL);
											#endif
										}
									}
								}
							}
						}

						
						// Bail if there is no access token
						if (!ValidStr(d->OAuth.AccessToken))
						{
							sprintf_s(Buf, sizeof(Buf), "Warning: No OAUTH2 Access Token.");
							#if DEBUG_OAUTH2
							LgiTrace("%s:%i - %s.\n", _FL, Buf);
							#endif
							Log(Buf, GSocketI::SocketMsgWarning);
							continue;
						}
						
						// Construct the XOAUTH2 parameter
						GString s;
						s.Printf("user=%s\001auth=Bearer %s\001\001", User, d->OAuth.AccessToken.Get());
						#if DEBUG_OAUTH2
						LgiTrace("%s:%i - s=%s.\n", _FL, s.Get());
						#endif
						Base64Str(s);						
					
						// Issue the IMAP command
						int AuthCmd = d->NextCmd++;
						GString AuthStr;
						AuthStr.Printf("A%4.4i AUTHENTICATE XOAUTH2 %s\r\n", AuthCmd, s.Get());
						if (WriteBuf(false, AuthStr))
						{
							Dialog.DeleteArrays();
							if (Read(NULL))
							{
								for (char *l = Dialog.First(); l; l = Dialog.Next())
								{
									if (*l == '+')
									{
										l++;
										while (*l && strchr(WhiteSpace, *l))
											l++;
										s = l;
										UnBase64Str(s);
										Log(s, GSocketI::SocketMsgError);
										
										GXmlTag t;
										JsonDecode(t, s);
										int StatusCode = t.GetAsInt("status");

										sprintf_s(Buf, sizeof(Buf), "\r\n");
										WriteBuf(false, NULL, true);
										
										if (StatusCode == 400)
										{
											// Refresh the token...?
										}
									}
									else if (*l == '*')
									{
									}
									else
									{
										if (IsResponse(l, AuthCmd, LoggedIn) &&
											LoggedIn)
										{
											if (SettingStore)
											{
												// Login successful, so persist the AuthCode for next time
												GVariant v = d->OAuth.AccessToken.Get();
												bool b = SettingStore->SetValue(OPT_ImapOAuth2AccessToken, v);
												if (!b)
												{
													Log("Couldn't store access token.", GSocketI::SocketMsgWarning);
												}
											}
											break;
										}
									}
								}
							}
							CommandFinished();
						}
						
						if (!LoggedIn && SettingStore)
						{
							GVariant v;
							SettingStore->SetValue(OPT_ImapOAuth2AccessToken, v);
						}
					}
					else
					{
						char s[256];
						sprintf_s(s, sizeof(s), "Warning: Unsupport auth type '%s'", AuthType);
						Log(s, GSocketI::SocketMsgWarning);
					}
				}

				if (LoggedIn)
				{
					Status = true;

					// Ask server for it's heirarchy (folder) separator.
					int Cmd = d->NextCmd++;
					sprintf_s(Buf, sizeof(Buf), "A%4.4i LIST \"\" \"\"\r\n", Cmd);
					if (WriteBuf())
					{
						ClearDialog();
						Buf[0] = 0;
						if (ReadResponse(Cmd))
						{
							for (char *Dlg = Dialog.First(); Dlg; Dlg=Dialog.Next())
							{
								GArray<GAutoString> t;
								char *s = Dlg;
								while (*s)
								{
									GAutoString a = ImapBasicTokenize(s);
									if (a)
										t.New() = a;
									else
										break;
								}								

								if (t.Length() >= 5 &&
									strcmp(t[0], "*") == 0 &&
									_stricmp(t[1], "list") == 0)
								{
									for (unsigned i=2; i<t.Length(); i++)
									{
										s = t[i];
										if (strlen(s) == 1)
										{
											d->FolderSep = *s;
											break;
										}
									}
									break;
								}
							}
						}
						CommandFinished();
					}
				}
				else
				{
					SetError(L_ERROR_UNSUPPORTED_AUTH,
							"Authentication failed, types available:\n\t%s",
							ValidStr(AuthTypeStr) ? AuthTypeStr : "(none)");
				}
			}
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::Close()
{
	bool Status = false;
	if (Socket && Lock(_FL))
	{
		if (d->ExpungeOnExit)
		{
			ExpungeFolder();
		}
		
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i LOGOUT\r\n", Cmd);
		if (WriteBuf())
		{
			Status = true;
		}
		CommandFinished();
		Unlock();
	}
	return Status;
}

bool MailIMap::GetCapabilities(GArray<char*> &s)
{
	char *k = 0;
	for (bool p=d->Capability.First(&k); p; p=d->Capability.Next(&k))
	{
		s.Add(k);
	}

	return s.Length() > 0;
}

bool MailIMap::ServerOption(char *Opt)
{
	return d->Capability.Find(Opt) != 0;
}

char *MailIMap::GetSelectedFolder()
{
	return d->Current;
}

bool MailIMap::SelectFolder(const char *Path, GHashTbl<const char*,int> *Values)
{
	bool Status = false;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		char *Enc = EncodePath(Path);
		sprintf_s(Buf, sizeof(Buf), "A%4.4i SELECT \"%s\"\r\n", Cmd, Enc);
		DeleteArray(Enc);
		if (WriteBuf())
		{
			DeleteArray(d->Current);
			ClearDialog();
			if (ReadResponse(Cmd))
			{
				Uid.DeleteArrays();

				if (Values)
				{
					Values->IsCase(false);
					for (char *Dlg = Dialog.First(); Dlg; Dlg=Dialog.Next())
					{
						GToken t(Dlg, " []");
						if (!_stricmp(t[0], "*") && t.Length() > 2)
						{
							char *key = t[2];
							char *sValue = t[1];
							int iValue = atoi(sValue);
							if (_stricmp(key, "exists") == 0)
							{
								Values->Add(key, iValue);
							}
							else if (_stricmp(key, "recent") == 0)
							{
								Values->Add(key, iValue);
							}
							else if (_stricmp(key, "unseen") == 0)
							{
								Values->Add(key, iValue);
							}
						}
					}
				}

				Status = true;
				d->Current = NewStr(Path);
				ClearDialog();
			}
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

int MailIMap::GetMessages(const char *Path)
{
	int Status = 0;

	if (Socket && Lock(_FL))
	{
		GHashTbl<const char*,int> f(0, false, NULL, -1);
		if (SelectFolder(Path, &f))
		{
			int Exists = f.Find("exists");
			if (Exists >= 0)
				Status = Exists;
			else
				LgiTrace("%s:%i - Failed to get 'exists' value.\n", _FL);
		}
		
		Unlock();
	}
	
	return Status;
}

int MailIMap::GetMessages()
{
	return GetMessages("INBOX");
}

char *MailIMap::SequenceToString(GArray<int> *Seq)
{
	if (!Seq)
		return NewStr("1:*");

	GStringPipe p;			
	int Last = 0;
	for (unsigned s=0; s<Seq->Length(); )
	{
		unsigned e = s;
		while (e<Seq->Length()-1 && (*Seq)[e] == (*Seq)[e+1]-1)
			e++;

		if (s) p.Print(",");
		if (e == s)
			p.Print("%i", (*Seq)[s]);
		else
			p.Print("%i:%i", (*Seq)[s], (*Seq)[e]);

		s = e + 1;			
	}

	return p.NewStr();
}

static void RemoveBytes(GArray<char> &a, unsigned &Used, unsigned Bytes)
{
	if (Used >= Bytes)
	{
		unsigned Remain = Used - Bytes;
		if (Remain > 0)
			memmove(&a[0], &a[Bytes], Remain);
		Used -= Bytes;
	}
	else LgiAssert(0);
}

static bool PopLine(GArray<char> &a, unsigned &Used, GAutoString &Line)
{
	for (unsigned i=0; i<Used; i++)
	{
		if (a[i] == '\n')
		{
			i++;
			Line.Reset(NewStr(&a[0], i));
			RemoveBytes(a, Used, i);
			return true;
		}
	}

	return false;
}


void NullCheck(char *Ptr, unsigned Len)
{
	// Check for NULLs
	for (unsigned i=0; i<Len; i++)
	{
		LgiAssert(Ptr[i] != 0);
	}
}

extern void DeNullText(char *in, int &len);

int MailIMap::Fetch(bool ByUid,
					const char *Seq,
					const char *Parts,
					FetchCallback Callback,
					void *UserData,
					GStreamI *RawCopy,
					int64 SizeHint)
{
	if (!Parts || !Callback || !Seq)
	{
		LgiTrace("%s:%i - Invalid FETCH argument.\n", _FL);
		return false;
	}

	if (!Lock(_FL))
	{
		LgiTrace("%s:%i - Failed to get lock.\n", _FL);
		return false;
	}
	
	int Status = 0;
	int Cmd = d->NextCmd++;
	GStringPipe p(256);
	p.Print("A%4.4i %sFETCH ", Cmd, ByUid ? "UID " : "");
	p.Write(Seq, strlen(Seq));
	p.Print(" (%s)\r\n", Parts);
	GAutoString WrBuf(p.NewStr());
	if (WriteBuf(false, WrBuf))
	{
		ClearDialog();

		GArray<char> Buf;
		Buf.Length(1024 + (SizeHint>0?(uint32)SizeHint:0));
		unsigned Used = 0;
		unsigned MsgSize;
		int64 Start = LgiCurrentTime();
		int64 Bytes = 0;
		bool Done = false;
		
		uint64 TotalTs = 0;

		bool Blocking = Socket->IsBlocking();
		Socket->IsBlocking(false);

		#if DEBUG_FETCH
		LgiTrace("%s:%i - Fetch: Starting loop\n", _FL);
		#endif

		uint64 LastActivity = LgiCurrentTime();
		bool Debug = false;
		while (!Done && Socket->IsOpen())
		{
			int r;
			do				
			{
				// Extend the buffer if getting used up
				if (Buf.Length()-Used <= 256)
				{
					Buf.Length(Buf.Length() + (64<<10));
					#if DEBUG_FETCH
					LgiTrace("%s:%i - Fetch: Ext buf: %i\n", _FL, Buf.Length());
					#endif
				}

				// Try and read bytes from server.
				r = Socket->Read(&Buf[Used], Buf.Length()-Used-1); // -1 for NULL terminator
				#if DEBUG_FETCH
				LgiTrace("%s:%i - Fetch: r=%i, used=%i, buf=%i\n", _FL, r, Used, Buf.Length());
				#endif
				if (r > 0)
				{
					if (RawCopy)
						RawCopy->Write(&Buf[Used], r);

					Used += r;
					Bytes += r;
					
					LastActivity = LgiCurrentTime();
				}
				else if (!Debug)
				{
					/*
					if (LgiCurrentTime() - LastActivity > 10000)
						Debug = true;
					*/
				}
				
				if (Debug)
					LgiTrace("%s:%i - Recv=%i\n", _FL, r);
			}
			while (r > 0);
			
			// See if we can parse out a single response
			GArray<StrRange> Ranges;
			LgiAssert(Used < Buf.Length());
			Buf[Used] = 0; // NULL terminate before we parse

			while (true)
			{
				MsgSize = ParseImapResponse(&Buf[0], Used, Ranges, 2);
				#if DEBUG_FETCH
				LgiTrace("%s:%i - Fetch: MsgSize=%i\n", _FL, MsgSize);
				#endif
				
				if (Debug)
					LgiTrace("%s:%i - ParseImapResponse=%i\n", _FL, MsgSize);
				
				if (!MsgSize)
					break;

				if (!Debug)
					LastActivity = LgiCurrentTime();
				
				char *b = &Buf[0];
				if (MsgSize > Used)
				{
					// This is an error... ParseImapResponse should always return <= Used.
					// If this triggers, ParseImapResponse is skipping a NULL that it shouldn't.
					#if DEBUG_FETCH
					LgiTrace("%s:%i - Fetch: Wrong size %i, %i\n", _FL, MsgSize, Used);
					#endif
					Ranges.Length(0);
					LgiAssert(0);
					#if _DEBUG
					ParseImapResponse(&Buf[0], Used, Ranges, 2);
					#endif
					Done = true;
					break;
				}
				
				LgiAssert(Ranges.Length() >= 2);
				
				// Setup strings for callback
				char *Param = b + Ranges[0].Start;
				Param[Ranges[0].Len()] = 0;
				char *Name = b + Ranges[1].Start;
				Name[Ranges[1].Len()] = 0;

				if (_stricmp(Name, "FETCH"))
				{
					// Not the response we're looking for.
					#if DEBUG_FETCH
					LgiTrace("%s:%i - Fetch: Wrong response: %s\n", _FL, Name);
					#endif
				}
				else
				{
					// Process ranges into a hash table
					GHashTbl<const char*, char*> Parts;
					for (unsigned i=2; i<Ranges.Length()-1; i += 2)
					{
						char *Name = b + Ranges[i].Start;
						Name[Ranges[i].Len()] = 0;							
						char *Value = NewStr(b + Ranges[i+1].Start, Ranges[i+1].Len());
						Parts.Add(Name, Value);
					}
					
					// Call the callback function
					if (Callback(this, Param, Parts, UserData))
					{
						#if DEBUG_FETCH
						LgiTrace("%s:%i - Fetch: Callback OK\n", _FL);
						#endif
						Status++;
					}
					else
					{
						#if DEBUG_FETCH
						LgiTrace("%s:%i - Fetch: Callback return FALSE?\n", _FL);
						#endif
					}

					// Clean up mem
					Parts.DeleteArrays();
				}

				// Remove this msg from buffer
				RemoveBytes(Buf, Used, MsgSize);
				Buf[Used] = 0; // 'Used' changed... so NULL terminate before we parse
			}

			// Look for the end marker
			#if DEBUG_FETCH
			LgiTrace("%s:%i - Fetch: End, Used=%i, Buf=%.12s\n", _FL, Used, Buf);
			#endif
			if (Used > 0 && Buf[0] != '*')
			{
				GAutoString Line;
				
				while (PopLine(Buf, Used, Line))
				{
					#if DEBUG_FETCH
					LgiTrace("%s:%i - Fetch: Line='%s'\n", _FL, Line.Get());
					#endif
					GToken t(Line, " \r\n");
					if (t.Length() >= 2)
					{
						char *r = t[0];
						if (*r == 'A')
						{
							Status = !_stricmp(t[1], "Ok");
							int Response = atoi(r + 1);
							Log(Line, Status ? GSocketI::SocketMsgReceive : GSocketI::SocketMsgError);
							if (Response == Cmd)
							{
								Done = true;
								break;
							}
						}
						else Log(&Buf[0], GSocketI::SocketMsgError);
					}
					else
					{
						// This is normal behaviour... just don't have the end marker yet.
						Done = true;
						break;
					}
				}
			}
		}

		Socket->IsBlocking(Blocking);			
		CommandFinished();
	}
		
	Unlock();
	return Status;
}

bool IMapHeadersCallback(MailIMap *Imap, char *Msg, GHashTbl<const char*, char*> &Parts, void *UserData)
{
	char *s = Parts.Find(sRfc822Header);
	if (s)
	{
		Parts.Delete(sRfc822Header);

		GAutoString *Hdrs = (GAutoString*)UserData;
		Hdrs->Reset(s);
	}
	
	Parts.DeleteArrays();
	return true;
}

char *MailIMap::GetHeaders(int Message)
{
	GAutoString Text;
	
	if (Lock(_FL))
	{
		char Seq[64];
		sprintf_s(Seq, sizeof(Seq), "%i", Message + 1);

		Fetch(	false,
				Seq,
				sRfc822Header,
				IMapHeadersCallback,
				&Text,
				NULL);
		
		Unlock();
	}
	
	return Text.Release();
}

struct ReceiveCallbackState
{
	MailTransaction *Trans;
	MailCallbacks *Callbacks;
};

static bool IMapReceiveCallback(MailIMap *Imap, char *Msg, GHashTbl<const char*, char*> &Parts, void *UserData)
{
	ReceiveCallbackState *State = (ReceiveCallbackState*) UserData;
	char *Flags = Parts.Find("FLAGS");
	if (Flags)
	{
		State->Trans->Imap.Set(Flags);
	}
	
	char *Hdrs = Parts.Find(sRfc822Header);
	if (Hdrs)
	{
		int Len = strlen(Hdrs);
		State->Trans->Stream->Write(Hdrs, Len);
	}

	char *Body = Parts.Find("BODY[TEXT]");
	if (Body)
	{
		int Len = strlen(Body);
		State->Trans->Stream->Write(Body, Len);
	}
	
	State->Trans->Status = Hdrs != NULL || Body != NULL;
	if (Imap->Items)
		Imap->Items->Value++;
	
	Parts.DeleteArrays();

	if (State->Callbacks)
	{
		bool Ret = State->Callbacks->OnReceive(State->Trans, State->Callbacks->CallbackData);
		if (!Ret)
			return false;
	}

	return true;
}

bool MailIMap::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	if (Lock(_FL))
	{
		int Errors = 0;
		ReceiveCallbackState State;
		State.Callbacks = Callbacks;
		
		for (unsigned i=0; i<Trans.Length(); i++)
		{
			State.Trans = Trans[i];			
			State.Trans->Status = false;

			char Seq[64];
			sprintf_s(Seq, sizeof(Seq), "%i", State.Trans->Index + 1);
			
			Fetch
			(
				false,
				Seq,
				"FLAGS RFC822.HEADER BODY[TEXT]",
				IMapReceiveCallback,
				&State,
				NULL
			);

			if (State.Trans->Status)
			{
				Status = true;
			}
			else if (Errors++ > 5)
			{
				// Yeah... not feelin' it
				Status = false;
				break;
			}
		}
		Unlock();
	}

	return Status;
}

bool MailIMap::Append(const char *Folder, ImapMailFlags *Flags, const char *Msg, GString &NewUid)
{
	bool Status = false;

	if (Folder && Msg && Lock(_FL))
	{
		GAutoString Flag(Flags ? Flags->Get() : 0);
		GAutoString Path(EncodePath(Folder));

		int Cmd = d->NextCmd++;
		int Len = 0;
		for (const char *m = Msg; *m; m++)
		{
			if (*m == '\n')
			{
				Len += 2;
			}
			else if (*m != '\r')
			{
				Len++;
			}
		}

		// Append on the end of the mailbox
		int c = sprintf_s(Buf, sizeof(Buf), "A%4.4i APPEND \"%s\"", Cmd, Path.Get());
		if (Flag)
			c += sprintf_s(Buf+c, sizeof(Buf)-c, " (%s)", Flag.Get());
		c += sprintf_s(Buf+c, sizeof(Buf)-c, " {%i}\r\n", Len);
		
		if (WriteBuf())
		{
			if (Read())
			{
				bool GotPlus = false;
				for (char *Dlg = Dialog.First(); Dlg; Dlg = Dialog.Next())
				{
					if (Dlg[0] == '+')
					{
    					Dialog.Delete(Dlg);
    					DeleteArray(Dlg);
						GotPlus = true;
						break;
					}
				}
				
				if (GotPlus)
				{
					int Wrote = 0;
					for (const char *m = Msg; *m; )
					{
						while (*m == '\r' || *m == '\n')
						{
							if (*m == '\n')
							{
								Wrote += Socket->Write((char*)"\r\n", 2);
							}
							m++;
						}

						const char *e = m;
						while (*e && *e != '\r' && *e != '\n')
							e++;
						if (e > m)
						{
							Wrote += Socket->Write(m, e-m);
							m = e;
						}
						else break;
					}

					LgiAssert(Wrote == Len);
					Wrote += Socket->Write((char*)"\r\n", 2);

					// Read response..
					ClearDialog();
					if ((Status = ReadResponse(Cmd)))
					{
						char Tmp[16];
						sprintf_s(Tmp, sizeof(Tmp), "A%4.4i", Cmd);
						for (char *Line = Dialog.First(); Line; Line = Dialog.Next())
						{
							GAutoString c = ImapBasicTokenize(Line);
							if (!c) break;
							if (!strcmp(Tmp, c))
							{
								GAutoString a;

								while ((a = ImapBasicTokenize(Line)).Get())
								{
									GToken t(a, " ");
									if (t.Length() > 2 && !_stricmp(t[0], "APPENDUID"))
									{
										NewUid = t[2];
										break;
									}
								}
							}
						}
					}
				}
			}
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::Delete(int Message)
{
	bool Status = false;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i STORE %i FLAGS (\\deleted)\r\n", Cmd, Message+1);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			CommandFinished();
 		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::Delete(bool ByUid, const char *Seq)
{
	bool Status = false;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i %sSTORE %s FLAGS (\\deleted)\r\n", Cmd, ByUid?"UID ":"", Seq);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			CommandFinished();
 		}
		
		Unlock();
	}

	return Status;
}

int MailIMap::Sizeof(int Message)
{
	int Status = 0;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i FETCH %i (%s)\r\n", Cmd, Message+1, sRfc822Size);
		if (WriteBuf())
		{
			ClearDialog();
			Buf[0] = 0;
			if (ReadResponse(Cmd))
			{
				char *d = Dialog.First();
				if (d)
				{
					char *t = strstr(d, sRfc822Size);
					if (t)
					{
						t += strlen(sRfc822Size) + 1;
						Status = atoi(t);
					}
				}
			}
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool ImapSizeCallback(MailIMap *Imap, char *Msg, GHashTbl<const char*, char*> &Parts, void *UserData)
{
	GArray<int> *Sizes = (GArray<int>*) UserData;
	int Index = atoi(Msg);
	if (Index < 1)
		return false;

	char *Sz = Parts.Find(sRfc822Size);
	if (!Sz)
		return false;

	(*Sizes)[Index - 1] = atoi(Sz);
	return true;
}

bool MailIMap::GetSizes(GArray<int> &Sizes)
{
	return Fetch(false, "1:*", sRfc822Size, ImapSizeCallback, &Sizes) != 0;	
}

bool MailIMap::GetUid(int Message, char *Id, int IdLen)
{
	bool Status = false;
	
	if (Lock(_FL))
	{
		if (FillUidList())
		{
			char *s = Uid.ItemAt(Message);
			if (s && Id)
			{
				strcpy_s(Id, IdLen, s);
				Status = true;
			}
		}
	
		Unlock();
	}

	return Status;
}

bool MailIMap::FillUidList()
{
	bool Status = false;

	if (Socket && Lock(_FL))
	{
		if (Uid.Length() == 0)
		{
			int Cmd = d->NextCmd++;
			sprintf_s(Buf, sizeof(Buf), "A%4.4i UID SEARCH ALL\r\n", Cmd);
			if (WriteBuf())
			{
				ClearDialog();
				if (ReadResponse(Cmd))
				{
					for (char *d = Dialog.First(); d && !Status; d=Dialog.Next())
					{
						GToken T(d, " ");
						if (T[1] && strcmp(T[1], "SEARCH") == 0)
						{
							for (unsigned i=2; i<T.Length(); i++)
							{
								Uid.Insert(NewStr(T[i]));
							}
						}
					}

					Status = true;
				}
				CommandFinished();
			}
		}
		else
		{
			Status = true;
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::GetUidList(List<char> &Id)
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (FillUidList())
		{
			for (char *s=Uid.First(); s; s=Uid.Next())
			{
				Id.Insert(NewStr(s));
			}

			Status = true;
		}
		Unlock();
	}

	return Status;
}

bool MailIMap::GetFolders(GArray<MailImapFolder*> &Folders)
{
	bool Status = false;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i LIST \"\" \"*\"\r\n", Cmd);
		if (WriteBuf())
		{
			ClearDialog();
			Buf[0] = 0;
			if (ReadResponse(Cmd))
			{
				char Sep[] = { GetFolderSep(), 0 };

				for (char *d = Dialog.First(); d; d=Dialog.Next())
				{
					GArray<GAutoString> t;
					char *s;
					while ((s = LgiTokStr((const char*&)d)))
					{
						t[t.Length()].Reset(s);
					}
					
					if (t.Length() >= 5)
					{
						if (strcmp(t[0], "*") == 0 &&
							_stricmp(t[1], "LIST") == 0)
						{
							char *Folder = t[t.Length()-1];

							MailImapFolder *f = new MailImapFolder();
							if (f)
							{
								Folders.Add(f);
								f->Sep = Sep[0];

								// Check flags
								f->NoSelect = stristr(t[2], "NoSelect") != 0;
								f->NoInferiors = stristr(t[2], "NoInferiors") != 0;

								// LgiTrace("Imap folder '%s' %s\n", Folder, t[2].Get());

								// Alloc name
								if (Folder[0] == '\"')
								{
									char *p = TrimStr(Folder, "\"");
									f->Path = DecodeImapString(p);
									DeleteArray(p);
								}
								else
								{
									f->Path = DecodeImapString(Folder);
								}
							}
						}						
					}
				}

				Status = true;
				ClearDialog();
			}
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::CreateFolder(MailImapFolder *f)
{
	bool Status = false;
	char Dir[2] = { d->FolderSep, 0 };

	if (f && f->GetPath() && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		char *Enc = EncodePath(f->GetPath());
		sprintf_s(Buf, sizeof(Buf), "A%4.4i CREATE \"%s\"\r\n", Cmd, Enc);
		DeleteArray(Enc);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			if (Status)
			{
				char *End = f->Path + strlen(f->Path) - 1;
				if (*End == GetFolderSep())
				{
					f->NoSelect = true;
					*End = 0;
				}
				else
				{
					f->NoInferiors = true;
				}
			}
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

char *MailIMap::EncodePath(const char *Path)
{
	if (!Path)
		return 0;

	char Sep = GetFolderSep();
	char Native[MAX_PATH], *o = Native, *e = Native + sizeof(Native) - 1;
	for (const char *i = *Path == '/' ? Path + 1 : Path; *i && o < e; i++)
	{
		if (*i == '/')
			*o++ = Sep;
		else
			*o++ = *i;
	}
	*o++ = 0;

	return EncodeImapString(Native);
}

bool MailIMap::DeleteFolder(const char *Path)
{
	bool Status = false;

	if (Path && Lock(_FL))
	{
		// Close the current folder if required.
		if (d->Current &&
			_stricmp(Path, d->Current) == 0)
		{
			int Cmd = d->NextCmd++;
			sprintf_s(Buf, sizeof(Buf), "A%4.4i CLOSE\r\n", Cmd);
			if (WriteBuf())
			{
				ClearDialog();
				ReadResponse(Cmd);
				DeleteArray(d->Current);
				CommandFinished();
			}
		}

		// Delete the folder
		int Cmd = d->NextCmd++;
		char *NativePath = EncodePath(Path);
		sprintf_s(Buf, sizeof(Buf), "A%4.4i DELETE \"%s\"\r\n", Cmd, NativePath);
		DeleteArray(NativePath);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::RenameFolder(const char *From, const char *To)
{
	bool Status = false;

	if (From && To && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		GAutoString f(EncodePath(From));
		GAutoString t(EncodePath(To));
		sprintf_s(Buf, sizeof(Buf), "A%4.4i RENAME \"%s\" \"%s\"\r\n", Cmd, f.Get(), t.Get());
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::SetFolderFlags(MailImapFolder *f)
{
	bool Status = false;

	if (f && Lock(_FL))
	{
		/*
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i RENAME \"%s\" \"%s\"\r\n", Cmd);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
		}
		*/
		
		Unlock();
	}

	return Status;
}

bool MailIMap::SetFlagsByUid(GArray<char*> &Uids, const char *Flags)
{
	bool Status = false;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;

		GStringPipe p;
		p.Print("A%04.4i UID STORE ", Cmd);
		if (Uids.Length())
		{
			for (unsigned i=0; i<Uids.Length(); i++)
			{
				p.Print("%s%s", i?",":"", Uids[i]);
			}
		}
		else
			p.Print("1:*");
		p.Print(" FLAGS (%s)\r\n", Flags?Flags:"");

		char *Buffer = p.NewStr();
		if (Buffer)
		{
			if (WriteBuf(false, Buffer))
			{
				ClearDialog();
				Status = ReadResponse(Cmd);
				CommandFinished();
			}
			DeleteArray(Buffer);
		}

		Unlock();
	}

	return Status;
}

bool MailIMap::CopyByUid(GArray<char*> &InUids, const char *DestFolder)
{
	bool Status = false;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		GAutoString Dest(EncodePath(DestFolder));

		GStringPipe p(1024);
		p.Print("A%04.4i UID COPY ", Cmd);
		for (unsigned i=0; i<InUids.Length(); i++)
		{
			if (i) p.Write((char*)",", 1);
			p.Write(InUids[i], strlen(InUids[i]));
		}
		p.Print(" \"%s\"\r\n", Dest.Get());

		GAutoString Buffer(p.NewStr());
		if (Buffer && WriteBuf(false, Buffer))
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			CommandFinished();
		}

		Unlock();
	}

	return Status;
}

bool MailIMap::ExpungeFolder()
{
	bool Status = false;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i EXPUNGE\r\n", Cmd);

		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::Search(bool Uids, GArray<GAutoString> &SeqNumbers, const char *Filter)
{
	bool Status = false;

	if (ValidStr(Filter) && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i %sSEARCH %s\r\n", Cmd, Uids?"UID ":"", Filter);
		if (WriteBuf())
		{
			ClearDialog();
			if (ReadResponse(Cmd))
			{
				for (char *d = Dialog.First(); d; d = Dialog.Next())
				{
					if (*d != '*')
						continue;

					d++;
					GAutoString s(Tok(d));
					if (!s || _stricmp(s, "Search"))
						continue;

					while (s.Reset(Tok(d)))
					{
						SeqNumbers.New() = s;
						Status = true;
					}
				}
			}
			CommandFinished();
		}

		Unlock();
	}

	return Status;
}

bool MailIMap::Status(char *Path, int *Recent)
{
	bool Status = false;

	if (Path && Recent && Lock(_FL))
	{
		GAutoString Dest(EncodePath(Path));

		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i STATUS %s (RECENT)\r\n", Cmd, Dest.Get());
		if (WriteBuf())
		{
			ClearDialog();
			if (ReadResponse(Cmd))
			{
				for (char *d=Dialog.First(); d; d=Dialog.Next())
				{
					if (*d != '*')
						continue;

					d++;
					GAutoString Cmd = ImapBasicTokenize(d);
					GAutoString Folder = ImapBasicTokenize(d);
					GAutoString Fields = ImapBasicTokenize(d);
					if (Cmd &&
						Folder &&
						Fields &&
						!_stricmp(Cmd, "status") &&
						!_stricmp(Folder, Dest))
					{
						char *f = Fields;
						GAutoString Field = ImapBasicTokenize(f);
						GAutoString Value = ImapBasicTokenize(f);
						if (Field &&
							Value &&
							!_stricmp(Field, "recent"))
						{
							*Recent = atoi(Value);
							Status = true;
							break;
						}
					}
				}
			}
			else LgiTrace("%s:%i - STATUS cmd failed.\n", _FL);
			CommandFinished();
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::Poll(int *Recent, GArray<GAutoString> *New)
{
	bool Status = true;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i NOOP\r\n", Cmd);
		if (WriteBuf())
		{
			ClearDialog();
			if ((Status = ReadResponse(Cmd)))
			{
				int LocalRecent;
				if (!Recent)
					Recent = &LocalRecent;

				*Recent = 0;
				for (char *Dlg=Dialog.First(); Dlg; Dlg=Dialog.Next())
				{
					if (Recent && stristr(Dlg, " RECENT"))
					{
						*Recent = atoi(Dlg + 2);
					}
				}

				if (*Recent && New)
				{
					Search(false, *New, "new");
				}
			}
			CommandFinished();
		}
		Unlock();
	}

	return Status;
}

bool MailIMap::StartIdle()
{
	bool Status = false;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf_s(Buf, sizeof(Buf), "A%4.4i IDLE\r\n", Cmd);
		Status = WriteBuf();
		CommandFinished();
		Unlock();
	}


	return Status;
}

bool MailIMap::OnIdle(int Timeout, GArray<Untagged> &Resp)
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (Socket->IsReadable(Timeout))
		{
			Read();
		}

		char *Dlg;
		while ((Dlg = Dialog.First()))
		{
			Dialog.Delete(Dlg);
			Log(Dlg, GSocketI::SocketMsgReceive);

			if (Dlg[0] == '*' &&
				Dlg[1] == ' ')
			{
				char *s = Dlg + 2;
				GAutoString a = ImapBasicTokenize(s);
				GAutoString b = ImapBasicTokenize(s);
				if (a && b)
				{
					Untagged &u = Resp.New();
					if (IsDigit(a[0]))
					{
						u.Cmd = b;
						u.Id = atoi(a);
						if (ValidStr(s))
							u.Param.Reset(NewStr(s));
					}
					else
					{
						u.Param.Reset(NewStr(Dlg+2));
					}

					Status = true;
				}
			}

			DeleteArray(Dlg);
		}

		Unlock();
	}

	return Status;
}

bool MailIMap::FinishIdle()
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (WriteBuf(false, "DONE\r\n"))
		{
			Status = ReadResponse();
			CommandFinished();
		}
		Unlock();
	}

	return Status;
}

