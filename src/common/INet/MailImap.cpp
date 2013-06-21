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

////////////////////////////////////////////////////////////////////////////
#if GPL_COMPATIBLE
#include "AuthNtlm/Ntlm.h"
#else
#include "../../src/common/INet/libntlm-0.4.2/ntlm.h"
#endif
#if HAS_LIBGSASL
#include "gsasl.h"
#endif

#define Ws(s)			while (*s && strchr(WhiteSpace, *s)) s++
#define SkipWhite(s)	while ((s - Buffer) < Used && strchr(" \t", *s)) s++;
#define SkipNonWhite(s) while ((s - Buffer) < Used && !strchr(WhiteSpace, *s)) s++;
#define ExpectChar(ch)	{ if ((s - Buffer) >= Used || *s != ch) return 0; s++; }

struct StrRange
{
	int Start, End;
	
	void Set(int s, int e)
	{
		Start = s;
		End = e;
	}

	int Len() { return End - Start; }
};

int ParseImapResponse(GArray<char> &Buf, int Used, GArray<StrRange> &Ranges, int Names)
{
	Ranges.Length(0);

	if (Used <= 0)
		return 0;

	char *Buffer = &Buf[0];
	if (*Buffer != '*')
		return 0;

	char *s = Buffer + 1;
	char *Start;
	for (int n=0; n<Names; n++)
	{
		SkipWhite(s);
		Start = s;
		SkipNonWhite(s);
		if (s <= Start) return 0;
		Ranges.New().Set(Start - Buffer, s - Buffer); // The msg seq or UID
	}
	
	// Look for start of block
	SkipWhite(s);
	if (s[0] == '\r' &&
		s[1] == '\n')
	{
		s += 2;
		return s - Buffer;
	}

	if (*s != '(')
		return 0;
	s++;

	// Parse fields
	int MsgSize = 0;
	while (s - Buffer < Used)
	{
		// Field name
		SkipWhite(s);
		if (*s == ')')
		{
			ExpectChar(')');
			ExpectChar('\r');
			ExpectChar('\n');
			
			MsgSize = s - Buffer;
			LgiAssert(MsgSize <= Used);
			break;
		}

		Start = s;
		SkipNonWhite(s);
		if (s <= Start)
			return 0;
		Ranges.New().Set(Start - Buffer, s - Buffer);

		// Field value
		SkipWhite(s);
		if (*s == '{')
		{
			// Parse multiline
			s++;
			int Size = atoi(s);
			while ((s - Buffer) < Used && *s != '}') s++;
			ExpectChar('}');
			ExpectChar('\r');
			ExpectChar('\n');
			Start = s;
			s += Size;
			if ((s - Buffer) >= Used)
				return 0;
			Ranges.New().Set(Start - Buffer, s - Buffer);				
		}
		else
		{
			// Parse single
			if (*s == '(')
			{
				s++;
				Start = s;
				
				int Depth = 1;
				while ((s - Buffer) < Used)
				{
					if (*s == '\"')
					{
						s++;
						while ((s - Buffer) < Used && *s != '\"')
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
					char *Begin = s;
					char Delim = *s++;
					Start = s;
					while (*s && (s - Buffer) < Used && *s != Delim)
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
					while (*s && (s - Buffer) < Used)
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
	int HdrLen = 0;
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
			char *n = NewStr(s, e - s);
			s = e + (*e != 0);
			return GAutoString(n);
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

char *DecodeImapString(char *s)
{
	GStringPipe p;
	while (s && *s)
	{
		if (*s == '&')
		{
			char Escape = *s++;
			
			char *e = s;
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

							char *c8 = LgiNewUtf16To8((char16*)Bin, BinLen);
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

char *EncodeImapString(char *s)
{
	GStringPipe p;
	int Len = s ? strlen(s) : 0;
	
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

			for (int i=0; i<Str.Length(); i++)
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

void MailImapFolder::SetPath(char *s)
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

void MailImapFolder::SetName(char *s)
{
	if (s)
	{
		char Buf[256];
		strcpy(Buf, Path?Path:(char*)"");
		DeleteArray(Path);

		char *Last = strrchr(Buf, Sep);
		if (Last)
		{
			strcpy(Last+1, s);
			Path = NewStr(Buf);
		}
		else
		{
			Path = NewStr(s);
		}
	}
}

/////////////////////////////////////////////
class MailIMapPrivate
{
public:
	int NextCmd;
	bool Logging;
	bool ExpungeOnExit;
	char FolderSep;
	char *Current;
	char *Flags;
	GHashTable Capability;

	MailIMapPrivate()
	{
		FolderSep = '/';
		NextCmd = 1;
		Logging = true;
		ExpungeOnExit = true;
		Current = 0;
		Flags = 0;
	}

	~MailIMapPrivate()
	{
		DeleteArray(Current);
		DeleteArray(Flags);
	}
};

MailIMap::MailIMap() : GMutex("MailImapSem")
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

bool MailIMap::WriteBuf(bool ObsurePass, const char *Buffer)
{
	if (Socket)
	{
		if (!Buffer) Buffer = Buf;

		int Len = strlen(Buffer);

		if (Socket->Write((void*)Buffer, Len, 0) == Len)
		{
			if (ObsurePass)
			{
				char *Sp = (char*)strrchr(Buffer, ' ');
				if (Sp)
				{
					strcpy(Sp + 1, "********\r\n");
				}
			}

			Log(Buffer, MAIL_SEND_COLOUR);

			return true;
		}
		else Log("Failed to write data to socket.", MAIL_ERROR_COLOUR);
	}
	else Log("Not connected.", MAIL_ERROR_COLOUR);

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
				COLOUR c = MAIL_RECEIVE_COLOUR;
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
		else break;
	}

	return Lines > 0;
}

bool MailIMap::ReadResponse(int Cmd, GStringPipe *Out, bool Plus)
{
	bool Done = false;
	bool Status = false;
	if (Socket)
	{
		int64 Last = LgiCurrentTime();
		int Pos = Dialog.Length();
		while (!Done)
		{
			if (Read(Out))
			{
				COLOUR c = MAIL_RECEIVE_COLOUR;

				for (char *Dlg = Dialog[Pos]; !Done && Dlg; Dlg = Dialog.Next())
				{
					Pos++;
					if (Cmd < 0 || (Plus && stricmp(Dlg, "+") == 0))
					{
						Status = Done = true;
					}

					char Num[8];
					sprintf(Num, "A%4.4i ", Cmd);
					if (strnicmp(Dlg, Num, 6) == 0)
					{
						Done = true;
						Status = strnicmp(Dlg+6, "OK", 2) == 0;
						if (!Status)
							ServerMsg.Reset(NewStr(Dlg+6));
					}
					
					if (d->Logging)
						Log(Dlg, MAIL_RECEIVE_COLOUR);
				}
			}
			else break;
		}
	}

	return Status;
}

void Hex(char *Out, uchar *In, int Len = -1)
{
	if (Out && In)
	{
		if (Len < 0)
			Len = strlen((char*)In);

		for (int i=0; i<Len; i++)
		{
			sprintf(Out, "%2.2x", *In++);
			Out += 2;
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

	Log(Buf, MAIL_RECEIVE_COLOUR);
	return true;
}

#if HAS_LIBGSASL
int GsaslCallback(Gsasl *ctx, Gsasl_session *sctx, Gsasl_property prop)
{
	return 0;
}
#endif

bool MailIMap::Open(GSocketI *s, char *RemoteHost, int Port, char *User, char *Password, char *&Cookie, int Flags)
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
		strcpy(Remote, RemoteHost);
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
			List<char> Auths;

			// check capability
			int CapCmd = d->NextCmd++;
			sprintf(Buf, "A%4.4i CAPABILITY\r\n", CapCmd);
			if (WriteBuf())
			{
				if (ReadResponse(CapCmd))
				{
					for (char *r=Dialog.First(); r; r=Dialog.Next())
					{
						GToken T(r, " ");
						if (T.Length() > 1 &&
							stricmp(T[1], "CAPABILITY") == 0)
						{
							for (int i=2; i<T.Length(); i++)
							{
								if (stricmp(T[i], "IMAP4") == 0 ||
									stricmp(T[i], "IMAP4rev1") == 0)
								{
									IMAP4Server = true;
								}
								if (strnicmp(T[i], "AUTH=", 5) == 0)
								{
									char *Type = T[i] + 5;
									Auths.Insert(NewStr(Type));
								}

								d->Capability.Add(T[i]);
							}
						}
					}

					ClearDialog();
				}
				else Log("Read CAPABILITY response failed", MAIL_ERROR_COLOUR);
			}
			else Log("Write CAPABILITY cmd failed", MAIL_ERROR_COLOUR);

			if (!IMAP4Server)
				Log("CAPABILITY says not an IMAP4Server", MAIL_ERROR_COLOUR);
			else
			{
				char *DefaultAuthType = 0;
				bool TryAllAuths = Auths.Length() == 0 &&
									!(Flags & (MAIL_USE_PLAIN | MAIL_USE_LOGIN | MAIL_USE_NTLM));

				if (TryAllAuths)
				{
					Auths.Insert(NewStr("DIGEST-MD5"));
				}
				if (TestFlag(Flags, MAIL_USE_PLAIN) || TryAllAuths)
				{
					Auths.Insert(DefaultAuthType = NewStr("PLAIN"), 0);
				}
				if (TestFlag(Flags, MAIL_USE_LOGIN) || TryAllAuths)
				{
					Auths.Insert(DefaultAuthType = NewStr("LOGIN"), 0);
				}
				if (TestFlag(Flags, MAIL_USE_NTLM) || TryAllAuths)
				{
					Auths.Insert(DefaultAuthType = NewStr("NTLM"), 0);
				}

				// SSL
				bool TlsError = false;
				if (TestFlag(Flags, MAIL_USE_STARTTLS))
				{
					int CapCmd = d->NextCmd++;
					sprintf(Buf, "A%4.4i STARTTLS\r\n", CapCmd);
					if (WriteBuf())
					{
						if (ReadResponse(CapCmd))
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
						Log("STARTTLS failed", MAIL_ERROR_COLOUR);
					}
				}

				// login
				bool LoggedIn = false;
				char AuthTypeStr[256] = "";
				char *AuthType = 0;
				for (AuthType=Auths.First(); !TlsError && AuthType && !LoggedIn; AuthType=Auths.Next())
				{
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
					if (!stricmp(AuthType, "GSSAPI"))
					{
						int AuthCmd = d->NextCmd++;
						sprintf(Buf, "A%04.4i AUTHENTICATE GSSAPI\r\n", AuthCmd);
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
									Log("gsasl_client_start failed", MAIL_ERROR_COLOUR);
								}

								// gsasl_step(ctx, 
								gsasl_done(ctx);
							}
							else Log("gsasl_init failed", MAIL_ERROR_COLOUR);
						}						
						else Log("AUTHENTICATE GSSAPI failed", MAIL_ERROR_COLOUR);
					}
					else
					#endif
					if (stricmp(AuthType, "LOGIN") == 0 ||
							 stricmp(AuthType, "OTP") == 0)
					{
						// clear text authentication
						int AuthCmd = d->NextCmd++;
						sprintf(Buf, "A%4.4i LOGIN %s %s\r\n", AuthCmd, User, Password);
						if (WriteBuf(true))
						{
							LoggedIn = ReadResponse(AuthCmd);
						}
					}
					else if (stricmp(AuthType, "PLAIN") == 0)
					{
						// plain auth type
						char s[256];
						char *e = s;
						*e++ = 0;
						strcpy(e, User);
						e += strlen(e);
						e++;
						strcpy(e, Password);
						e += strlen(e);
						*e++ = '\r';
						*e++ = '\n';
						int Len = e - s - 2;					

						int AuthCmd = d->NextCmd++;
						sprintf(Buf, "A%4.4i AUTHENTICATE PLAIN\r\n", AuthCmd);
						if (WriteBuf())
						{
							if (ReadResponse(AuthCmd, 0, true))
							{
								int b = ConvertBinaryToBase64(Buf, sizeof(Buf), (uchar*)s, Len);
								strcpy(Buf + b, "\r\n");

								if (WriteBuf())
								{
									LoggedIn = ReadResponse(AuthCmd);
								}
							}
						}						
					}
					#if (GPL_COMPATIBLE || defined(_LIBNTLM_H)) && defined(WIN32NATIVE)
					else if (stricmp(AuthType, "NTLM") == 0)
					{
						// NT Lan Man authentication
						OSVERSIONINFO ver;
						ZeroObj(ver);
						ver.dwOSVersionInfoSize = sizeof(ver);
						if (!GetVersionEx(&ver))
						{
							DWORD err = GetLastError();
							Log("Couldn't get OS version", MAIL_ERROR_COLOUR);
						}
						else
						{
							// Username is in the format: User[@Domain]
							char UserDom[256];
							strcpy(UserDom, User);
							char *Domain = strchr(UserDom, '@');
							if (Domain)
								*Domain++ = 0;

							int AuthCmd = d->NextCmd++;
							sprintf(Buf, "A%04.4i AUTHENTICATE NTLM\r\n", AuthCmd);
							if (WriteBuf())
							{
								if (ReadResponse(AuthCmd, 0, true))
								{
									tSmbNtlmAuthNegotiate	negotiate;              
									tSmbNtlmAuthChallenge	challenge;
									tSmbNtlmAuthResponse	response;
									char tmpstr[32];

									buildSmbNtlmAuthNegotiate(&negotiate, 0, 0);
									if (NTLM_VER(&negotiate) == 2)
									{
										negotiate.v2.version.major = ver.dwMajorVersion;
										negotiate.v2.version.minor = ver.dwMinorVersion;
										negotiate.v2.version.buildNumber = ver.dwBuildNumber;
										negotiate.v2.version.ntlmRevisionCurrent = 0x0f;
									}

									ZeroObj(Buf);
									int negotiateLen = SmbLength(&negotiate);
									int c = ConvertBinaryToBase64(Buf, sizeof(Buf), (uchar*)&negotiate, negotiateLen);
									strcpy(Buf + c, "\r\n");
									WriteBuf();

									/* read challange data from server, convert from base64 */

									Buf[0] = 0;
									ClearDialog();
									if (ReadResponse())
									{
										/* buffer should contain the string "+ [base 64 data]" */

										#if 1
											ZeroObj(challenge);
											char *Line = Dialog.First();
											LgiAssert(Line);
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
											response.v2.version.major = ver.dwMajorVersion;
											response.v2.version.minor = ver.dwMinorVersion;
											response.v2.version.buildNumber = ver.dwBuildNumber;
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
										WriteBuf();

										/* read line from server, it should be "[seq] OK blah blah blah" */
										LoggedIn = ReadResponse(AuthCmd);
									}
								}
							}
						}

						ClearDialog();
					}
					#endif
					else if (stricmp(AuthType, "DIGEST-MD5") == 0)
					{
						int AuthCmd = d->NextCmd++;
						sprintf(Buf, "A%4.4i AUTHENTICATE DIGEST-MD5\r\n", AuthCmd);
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

									int32 CnonceI[2] = { LgiRand(), LgiRand() };
									char Cnonce[32];
									if (TestCnonce)
										strcpy(Cnonce, TestCnonce);
									else
										Cnonce[ConvertBinaryToBase64(Cnonce, sizeof(Cnonce), (uchar*)&CnonceI, sizeof(CnonceI))] = 0;
									s = strchr(Cnonce, '=');
									if (s) *s = 0;

									int Nc = 1;
									char *Realm = Map.Find("realm");
									char DigestUri[256];
									if (Realm)
									{
										sprintf(DigestUri, "imap/%s", Realm);
									}
									else
									{
										sprintf(DigestUri, "imap/%s", RemoteHost);
									}

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
									sprintf(Buf, "%s:%s:%s", User, Realm ? Realm : (char*)"", Password);
									MDStringToDigest((uchar*)a1, Buf);
									char *Authzid = Map.Find("authzid");
									char *a1end = a1 + 16;
									if (Authzid)
									{										
										a1end += sprintf(a1end, ":%s:%s:%s", Nonce, Cnonce, Authzid);
									}
									else
									{
										a1end += sprintf(a1end, ":%s:%s", Nonce, Cnonce);
									}
									MDStringToDigest(md5, a1, a1end - a1);
									char a1hex[256];
									Hex(a1hex, (uchar*)md5, sizeof(md5));

									// Calculate 
									char a2[256];
									if (Qop && (stricmp(Qop, "auth-int") == 0 || stricmp(Qop, "auth-conf") == 0))
									{
										sprintf(a2, "AUTHENTICATE:%s:00000000000000000000000000000000", DigestUri);
									}
									else
									{
										sprintf(a2, "AUTHENTICATE:%s", DigestUri);
									}
									MDStringToDigest(md5, a2);
									char a2hex[256];
									Hex(a2hex, (uchar*)md5, sizeof(md5));

									// Calculate the final response
									sprintf(Buf, "%s:%s:%8.8i:%s:%s:%s", a1hex, Nonce, Nc, Cnonce, Qop, a2hex);
									MDStringToDigest(md5, Buf);
									Hex(Buf, (uchar*)md5, sizeof(md5));
									p.Print(",response=%s", Buf);
									if ((s = p.NewStr()))
									{
										int Chars = ConvertBinaryToBase64(Buf, sizeof(Buf) - 4, (uchar*)s, strlen(s));
										LgiAssert(Chars < sizeof(Buf));
										strcpy(Buf + Chars, "\r\n");
										
										if (WriteBuf() && Read())
										{
											for (char *Dlg = Dialog.First(); Dlg; Dlg=Dialog.Next())
											{
												if (Dlg[0] == '+' && Dlg[1] == ' ')
												{
													Log(Dlg, MAIL_RECEIVE_COLOUR);
													strcpy(Buf, "\r\n");
													if (WriteBuf())
													{
														LoggedIn = ReadResponse(AuthCmd);
													}
												}
												else
												{
													Log(Dlg, MAIL_ERROR_COLOUR);
													break;
												}
											}
										}
										
										DeleteArray(s);
									}
								}
							}
						}
					}
					else if (!stricmp(AuthType, "XOAUTH"))
					{
						GAutoPtr<GSocketI> Ssl(dynamic_cast<GSocketI*>(Socket->Clone()));
						if (Ssl)
						{
							GStringPipe p;
							GAutoString Hdr, Body;
							int StatusCode = 0;
							char Url[256];
							sprintf(Url, "http://mail.google.com/mail/b/%s/imap/", User);
							if (Http(Ssl, &Hdr, &Body, &StatusCode, "POST", Url, 0, 0))
							{
								bool Status = true;
								for (int i=0; i<5 && StatusCode == 302; i++)
								{
									GAutoString Redirect(InetGetHeaderField(Hdr, "Location"));
									if (Redirect)
									{
										Status = Http(Ssl, &Hdr, &Body, &StatusCode, "POST", Redirect, 0, 0);
										if (Status)
											break;
									}
									else break;									
								}

								if (Status)
								{
									GUri u;
									GAutoString UriUserName = u.Encode(User);
									
									GAutoString Nonce, Signature, Timestamp, Token;
									p.Print("%x", LgiRand());
									Nonce.Reset(p.NewStr());
									p.Print("%i", time(0));
									Timestamp.Reset(p.NewStr());

									p.Print("GET https://mail.google.com/mail/b/%s/imap/?xoauth_requestor_id=%s "
											"oauth_consumer_key=\"anonymous\", "
											"oauth_nonce=\"%s\", "
											"oauth_signature=\"%s\", "
											"oauth_signature_method=\"PLAINTEXT\", "
											"oauth_timestamp=\"%s\", "
											"oauth_token=\"%s\", "
											"oauth_version=\"1.0\"",
											User,
											UriUserName.Get(),
											Nonce.Get(),
											Signature.Get(),
											Timestamp.Get(),
											Token.Get());

									GAutoString Bin(p.NewStr());
									int BinLen = strlen(Bin);
									int B64Len = BufferLen_BinTo64(BinLen);
									GAutoString B64(new char[B64Len+1]);
									int c = ConvertBinaryToBase64(B64, B64Len, (uchar*)Bin.Get(), BinLen);
									B64[B64Len] = 0;

									int AuthCmd = d->NextCmd++;
									p.Print("A%04.4i AUTHENTICATE XOAUTH %s\r\n", AuthCmd, B64.Get());
									GAutoString Cmd(p.NewStr());
									int w = Socket->Write(Cmd, strlen(Cmd));
									Log(Cmd, MAIL_SEND_COLOUR);
									if (w > 0)
									{
										LoggedIn = ReadResponse(AuthCmd);							
									}
								}
							}
						}
					}
					else
					{
						char s[256];
						sprintf(s, "Warning: Unsupport auth type '%s'", AuthType);
						Log(s, MAIL_ERROR_COLOUR);
					}
				}

				if (LoggedIn)
				{
					Status = true;

					// Ask server for it's heirarchy (folder) separator.
					int Cmd = d->NextCmd++;
					sprintf(Buf, "A%4.4i LIST \"\" \"\"\r\n", Cmd);
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
									stricmp(t[1], "list") == 0)
								{
									for (int i=2; i<t.Length(); i++)
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
					}
				}
				else
				{
					char Str[300];
					sprintf(Str,
							"Authentication failed, types available:\n%s",
							(strlen(AuthTypeStr)>0) ? AuthTypeStr : "(none)");

					ServerMsg.Reset(NewStr(Str));
				}
			}

			Auths.DeleteArrays();
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
		sprintf(Buf, "A%4.4i LOGOUT\r\n", Cmd);
		if (WriteBuf())
		{
			Status = true;
		}
		Unlock();
	}
	return Status;
}

bool MailIMap::GetCapabilities(GArray<char*> &s)
{
	char *k = 0;
	for (void *p=d->Capability.First(&k); p; p=d->Capability.Next(&k))
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
	int Status = 0;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		char *Enc = EncodePath(Path);
		sprintf(Buf, "A%4.4i SELECT \"%s\"\r\n", Cmd, Enc);
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
						if (!stricmp(t[0], "*") && t.Length() > 2)
						{
							if (stricmp(t[2], "exists") == 0)
							{
								Values->Add(t[2], atoi(t[1]));
							}
							else if (stricmp(t[2], "recent") == 0)
							{
								Values->Add(t[2], atoi(t[1]));
							}
							else if (stricmp(t[2], "unseen") == 0)
							{
								Values->Add(t[2], atoi(t[3]));
							}
						}
					}
				}

				Status = true;
				d->Current = NewStr(Path);
				ClearDialog();
			}
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
		GHashTbl<const char*,int> f;
		if (SelectFolder(Path, &f))
		{
			Status = f.Find("exists");
		}
		
		Unlock();
	}
	
	return Status;
}

int MailIMap::GetMessages()
{
	return GetMessages("INBOX");
}

char *MailIMap::GetHeaders(int Message)
{
	GStringPipe Text;
	if (Lock(_FL))
	{
		GetParts(Message, Text, "RFC822.HEADER");
		Unlock();
	}
	
	return Text.NewStr();
}

char *MailIMap::SequenceToString(GArray<int> *Seq)
{
	if (!Seq)
		return NewStr("1:*");

	GStringPipe p;			
	int Last = 0;
	for (int s=0; s<Seq->Length(); )
	{
		int e = s;
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

static void RemoveBytes(GArray<char> &a, int &Used, int Bytes)
{
	int Remain = Used - Bytes;
	if (Remain > 0)
		memmove(&a[0], &a[Bytes], Remain);
	Used -= Bytes;
}

static bool PopLine(GArray<char> &a, int &Used, GAutoString &Line)
{
	for (int i=0; i<Used; i++)
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

bool MailIMap::Fetch(bool ByUid, char *Seq, const char *Parts, FetchCallback Callback, void *UserData, GStreamI *RawCopy)
{
	bool Status = false;

	if (!Parts || !Callback || !Seq)
		return false;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		if (snprintf(Buf, sizeof(Buf), "A%4.4i %sFETCH %s (%s)\r\n", Cmd, ByUid ? "UID " : "", Seq, Parts) > 0 &&
			WriteBuf())
		{
			ClearDialog();

			GArray<char> Buf;
			Buf.Length(1024);
			int Used = 0, MsgSize;
			int64 Start = LgiCurrentTime();
			int64 Bytes = 0;
			bool Done = false;

			while (!Done && Socket->IsOpen())
			{
				// Extend the buffer if getting used up
				if (Buf.Length()-Used < 256)
				{
					Buf.Length(Buf.Length() + 1024);
				}

				// Try and read bytes from server.
				int r = Socket->Read(&Buf[Used], Buf.Length()-Used);
				if (r > 0)
				{
					if (RawCopy)
						RawCopy->Write(&Buf[Used], r);

					Used += r;
					Bytes += r;

					#if 0
					int64 Time = LgiCurrentTime() - Start;
					double Rate = (double)Bytes / ((double)Time / 1000.0);
					LgiTrace("Starting fetch loop used=%ikb (%.0fkb/s)\n", Used>>10, Rate/1024.0);
					#endif

					// See if we can parse out a single response
					GArray<StrRange> Ranges;
					while ((MsgSize = ParseImapResponse(Buf, Used, Ranges, 2)))
					{
						char *b = &Buf[0];
						LgiAssert(Ranges.Length() >= 2);
						
						// Setup strings for callback
						char *Param = b + Ranges[0].Start;
						Param[Ranges[0].Len()] = 0;
						char *Name = b + Ranges[1].Start;
						Name[Ranges[1].Len()] = 0;

						if (stricmp(Name, "FETCH"))
						{
							// Not the response we're looking for, save it in the untagged data.
							/*
							Untagged *u = new Untagged;
							u->Name = Name;
							u->Param = Param;
							Untag.Add(u);
							*/
						}
						else
						{
							// Process ranges into a hash table
							GHashTbl<const char*, char*> Parts;
							for (int i=2; i<Ranges.Length()-1; i += 2)
							{
								char *Name = b + Ranges[i].Start;
								Name[Ranges[i].Len()] = 0;							
								char *Value = NewStr(b + Ranges[i+1].Start, Ranges[i+1].Len());
								Parts.Add(Name, Value);
							}
							
							// Call the callback function
							if (Callback(this, Param, Parts, UserData))
							{
								Status = true;
							}
							else
							{
								Parts.DeleteArrays();
								Status = false;
								break;
							}

							// Clean up mem
							Parts.DeleteArrays();
						}

						// Remove this msg from buffer
						RemoveBytes(Buf, Used, MsgSize);
					}

					// Look for the end marker
					if (Used > 0 && Buf[0] != '*')
					{
						GAutoString Line;
						
						while (PopLine(Buf, Used, Line))
						{
							GToken t(Line, " \r\n");
							if (t.Length() >= 2)
							{
								char *r = t[0];
								if (*r == 'A')
								{
									bool Status = !stricmp(t[1], "Ok");
									int Response = atoi(r + 1);
									Log(Line, Status ? MAIL_RECEIVE_COLOUR : MAIL_ERROR_COLOUR);
									if (Response == Cmd)
									{
										Done = true;
										break;
									}
								}
								else Log(&Buf[0], MAIL_ERROR_COLOUR);
							}
							else
							{
								LgiAssert(!"What happened?");
								Done = true;
								break;
							}
						}
					}
				}
				else break;
			}
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::GetParts(int Message, GStreamI &Out, const char *Parts, char **Flags)
{
	bool Status = false;

	if (Parts)
	{
		int Cmd = d->NextCmd++;
		sprintf(Buf, "A%4.4i FETCH %i (%s)\r\n", Cmd, Message+1, Parts);
		if (WriteBuf())
		{
			ClearDialog();
			Buf[0] = 0;
			d->Logging = false;

			GStringPipe Buf(4096);
			
			/////////////////////////////////
			// Debug
			int64 Start = LgiCurrentTime();
			/////////////////////////////////
			
			bool ReadOk = ReadResponse(Cmd, &Buf);
			
			/////////////////////////////////
			// Debug
			char Size[64];
			LgiFormatSize(Size, Buf.GetSize());
			// LgiTrace("ReadResponse time %i for %s\n", (int) (LgiCurrentTime() - Start), Size);
			/////////////////////////////////

			if (ReadOk)
			{
				int Chars = 0;
				int DialogItems = Dialog.Length();
				int Pos = 0;

				char *Response = Buf.NewStr();
				if (Response)
				{
					// FIXME..
					// Log(d, MAIL_RECEIVE_COLOUR);

					bool Error = false;
					char *s = Response;
					int Depth = 0;
					for (int i=0; i<10 && s[i]; i++, s++);

					while (!Error && s && *s)
					{
						Ws(s);
						if (*s == '(')
						{
							s++;
							Depth++;
						}
						else if (*s == ')')
						{
							s++;
							Depth--;
						}
						else
						{
							char *Field = LgiTokStr((const char*&)s);
							if (!Field) break;

							if (stricmp(Field, "Flags") == 0)
							{
								char *Start = strchr(s, '(');
								if (Start)
								{
									char *End = strchr(++Start, ')');
									if (End)
									{
										if (Flags)
										{
											*Flags = NewStr(Start, End-Start);
										}
										s = End + 1;
									}
									else Error = true;
								}
								else Error = true;
							}
							else
							{
								char *Start = strchr(s, '{');
								if (Start)
								{
									char *End = strchr(Start, '}');
									if (End)
									{
										*End = 0;
										Chars = atoi(++Start);
										s = End + 1;
										Ws(s);
										
										int i = 0;
										for (; i<Chars && s[i]; i++);
										Out.Write(s, i);
										Out.Write((char*)"\r\n\r\n", 4);
										s += i;
									}
									else Error = true;
								}
								else if (Depth > 0)
								{
									Ws(s);
									Start = s;
									char *End = strchr(Start, ' ');
									if (End)
									{
										Out.Write(s, End - s);
										s = End;
									}
									else Error = true;
								}
							}

							DeleteArray(Field);
						}
					}

					DeleteArray(Response);
				}

				Status = Out.GetSize() > 0;

				char *Last = Dialog.Last();
				if (Last)
				{
					Log(Last, MAIL_RECEIVE_COLOUR);
				}
			}

			d->Logging = true;
		}
	}

	return Status;
}

/*
bool MailIMap::ReceiveFlags(MailMessage *Msg)
{
	bool Status = false;

	if (Msg && d->Flags)
	{
		MailImapMsg *p = (MailImapMsg*)Msg->Private;
		if (!p)
			Msg->Private = p = new MailImapMsg;
		if (p)
		{
			p->Set(d->Flags);
			Status = true;
		}
	}
	DeleteArray(d->Flags);

	return Status;
}
*/

bool MailIMap::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	if (Lock(_FL))
	{
		for (int i=0; i<Trans.Length(); i++)
		{
			char *Flags = 0;
			if ((Trans[i]->Status = GetParts(Trans[i]->Index, *Trans[i]->Stream, "FLAGS RFC822.HEADER BODY[TEXT]", &Flags)))
			{
				Trans[i]->Imap.Set(Flags);
				DeleteArray(Flags);
				
				if (Callbacks)
				{
					Callbacks->OnReceive(Trans[i], Callbacks->CallbackData);
				}

				Status = true;
			}
		}
		Unlock();
	}

	return Status;
}

bool MailIMap::Append(char *Folder, ImapMailFlags *Flags, char *Msg, GAutoString &NewUid)
{
	bool Status = false;

	if (Folder && Msg && Lock(_FL))
	{
		GAutoString Flag(Flags ? Flags->Get() : 0);
		GAutoString Path(EncodePath(Folder));

		int Cmd = d->NextCmd++;
		int Len = 0;
		for (char *m = Msg; *m; m++)
		{
			if (*m == '\n') Len += 2;
			else if (*m != '\r') Len++;
		}

		// Append on the end of the mailbox
		int c = sprintf(Buf, "A%4.4i APPEND \"%s\"", Cmd, Path.Get());
		if (Flag)
			c += sprintf(Buf+c, " (%s)", Flag.Get());
		c += sprintf(Buf+c, " {%i}\r\n", Len);
		
		if (WriteBuf() && Read())
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
				for (char *m = Msg; *m; )
				{
					while (*m == '\r' || *m == '\n')
					{
						if (*m == '\n')
						{
							Wrote += Socket->Write((char*)"\r\n", 2);
						}
						m++;
					}

					char *e = m;
					while (*e && *e != '\r' && *e != '\n')
						e++;
					if (e > m)
					{
						Wrote += Socket->Write(m, e-m);
						m = e + (*e != 0);
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
					sprintf(Tmp, "A%4.4i", Cmd);
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
								if (t.Length() > 2 && !stricmp(t[0], "APPENDUID"))
								{
									NewUid.Reset(NewStr(t[2]));
									break;
								}
							}
						}
					}
				}
			}
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
		sprintf(Buf, "A%4.4i STORE %i FLAGS (\\deleted)\r\n", Cmd, Message+1);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
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
		const  char *Tag = "RFC822.SIZE";

		int Cmd = d->NextCmd++;
		sprintf(Buf, "A%4.4i FETCH %i (%s)\r\n", Cmd, Message+1, Tag);
		if (WriteBuf())
		{
			ClearDialog();
			Buf[0] = 0;
			if (ReadResponse(Cmd))
			{
				char *d = Dialog.First();
				if (d)
				{
					char *t = strstr(d, Tag);
					if (t)
					{
						t += strlen(Tag) + 1;
						Status = atoi(t);
					}
				}
			}
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::GetUid(int Message, char *Id)
{
	bool Status = false;
	
	if (Lock(_FL))
	{
		if (FillUidList())
		{
			char *s = Uid.ItemAt(Message);
			if (s && Id)
			{
				strcpy(Id, s);
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
			sprintf(Buf, "A%4.4i UID SEARCH ALL\r\n", Cmd);
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
							for (int i=2; i<T.Length(); i++)
							{
								Uid.Insert(NewStr(T[i]));
							}
						}
					}

					Status = true;
				}
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

bool MailIMap::GetFolders(List<MailImapFolder> &Folders)
{
	int Status = 0;

	if (Socket && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		sprintf(Buf, "A%4.4i LIST \"\" \"*\"\r\n", Cmd);
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
							stricmp(t[1], "LIST") == 0)
						{
							char *Folder = t[t.Length()-1];

							MailImapFolder *f = new MailImapFolder;
							if (f)
							{
								Folders.Insert(f);

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
		sprintf(Buf, "A%4.4i CREATE \"%s\"\r\n", Cmd, Enc);
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

bool MailIMap::DeleteFolder(char *Path)
{
	bool Status = false;

	if (Path && Lock(_FL))
	{
		// Close the current folder if required.
		if (d->Current &&
			stricmp(Path, d->Current) == 0)
		{
			int Cmd = d->NextCmd++;
			sprintf(Buf, "A%4.4i CLOSE\r\n", Cmd);
			if (WriteBuf())
			{
				ClearDialog();
				ReadResponse(Cmd);
				DeleteArray(d->Current);
			}
		}

		// Delete the folder
		int Cmd = d->NextCmd++;
		char *NativePath = EncodePath(Path);
		sprintf(Buf, "A%4.4i DELETE \"%s\"\r\n", Cmd, NativePath);
		DeleteArray(NativePath);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
		}
		
		Unlock();
	}

	return Status;
}

bool MailIMap::RenameFolder(char *From, char *To)
{
	bool Status = false;

	if (From && To && Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		char *f = EncodeImapString(From);
		char *t = EncodeImapString(To);
		sprintf(Buf, "A%4.4i RENAME \"%s\" \"%s\"\r\n", Cmd, f, t);
		DeleteArray(f);
		DeleteArray(t);
		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
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
		sprintf(Buf, "A%4.4i RENAME \"%s\" \"%s\"\r\n", Cmd);
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

	if (Flags && Lock(_FL))
	{
		int Cmd = d->NextCmd++;

		GStringPipe p;
		p.Print("A%04.4i UID STORE ", Cmd);
		if (Uids.Length())
		{
			for (int i=0; i<Uids.Length(); i++)
			{
				p.Print("%s%s", i?",":"", Uids[i]);
			}
		}
		else
			p.Print("1:*");
		p.Print(" FLAGS (%s)\r\n", Flags);

		char *Buffer = p.NewStr();
		if (Buffer)
		{
			if (WriteBuf(false, Buffer))
			{
				ClearDialog();
				Status = ReadResponse(Cmd);
			}
			DeleteArray(Buffer);
		}

		Unlock();
	}

	return Status;
}

bool MailIMap::CopyByUid(GArray<char*> &InUids, char *DestFolder)
{
	bool Status = false;

	if (Lock(_FL))
	{
		int Cmd = d->NextCmd++;
		GAutoString Dest(EncodePath(DestFolder));

		GStringPipe p(1024);
		p.Print("A%04.4i UID COPY ", Cmd);
		for (int i=0; i<InUids.Length(); i++)
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
		sprintf(Buf, "A%4.4i EXPUNGE\r\n", Cmd);

		if (WriteBuf())
		{
			ClearDialog();
			Status = ReadResponse(Cmd);
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
		sprintf(Buf, "A%4.4i %sSEARCH %s\r\n", Cmd, Uids?"UID ":"", Filter);
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
					if (!s || stricmp(s, "Search"))
						continue;

					while (s.Reset(Tok(d)))
					{
						SeqNumbers.New() = s;
						Status = true;
					}
				}
			}
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
		sprintf(Buf, "A%4.4i STATUS %s (RECENT)\r\n", Cmd, Dest.Get());
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
						!stricmp(Cmd, "status") &&
						!stricmp(Folder, Dest))
					{
						char *f = Fields;
						GAutoString Field = ImapBasicTokenize(f);
						GAutoString Value = ImapBasicTokenize(f);
						if (Field &&
							Value &&
							!stricmp(Field, "recent"))
						{
							*Recent = atoi(Value);
							Status = true;
							break;
						}
					}
				}
			}
			else LgiTrace("%s:%i - STATUS cmd failed.\n", _FL);
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
		sprintf(Buf, "A%4.4i NOOP\r\n", Cmd);
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
		sprintf(Buf, "A%4.4i IDLE\r\n", Cmd);
		Status = WriteBuf();
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
			Log(Dlg, MAIL_RECEIVE_COLOUR);

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
			Status = ReadResponse();
		Unlock();
	}

	return Status;
}

