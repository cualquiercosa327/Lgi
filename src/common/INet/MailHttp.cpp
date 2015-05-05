#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "Mail.h"
#include "GToken.h"
#include "GDocView.h"

//////////////////////////////////////////////////////////////////////////
#include "IHttp.h"

static char PopOverHttpSep[] = "\n-----------------[EndPopOverHttp]-----------------";

class Msg
{
public:
	int Index;
	int Size;
	char *Uid;
	char *Headers;
	bool Delete;

	Msg()
	{
		Index = 0;
		Size = 0;
		Uid = 0;
		Headers = 0;
		Delete = false;
	}

	~Msg()
	{
		DeleteArray(Uid);
		DeleteArray(Headers);
	}
};

class MailPhpPrivate
{
public:
	int Messages;
	char Uri[256];
	bool HasDeletes;
	bool HeadersRetreived;
	List<class Msg> Msgs;
	char *ProxyServer;
	int ProxyPort;
	char *UserName;
	char *UserPass;

	MailPhpPrivate()
	{
		Uri[0] = 0;
		Messages = 0;
		HasDeletes = 0;
		ProxyServer = 0;
		ProxyPort = 0;
		UserName = 0;
		UserPass = 0;
		HeadersRetreived = false;
	}
	
	~MailPhpPrivate()
	{
		DeleteArray(ProxyServer);
		Msgs.DeleteObjects();
		DeleteArray(UserName);
		DeleteArray(UserPass);
	}
};

MailPhp::MailPhp()
{
	d = new MailPhpPrivate;
}

MailPhp::~MailPhp()
{
	DeleteObj(d);
}

class MailSocket : public GSocket
{
	GSocketI *S;
	MailProtocolProgress *T;
	MailProtocol *Log;
	GStringPipe ReadBuf, WriteBuf;
	bool InData;
	GFile *Temp;
	int SepLen;
	bool OwnSocket;

public:
	MailSocket(GSocketI *s, MailProtocolProgress *t, MailProtocol *l)
	{
		if (s)
		{
			S = s;
			OwnSocket = false;
		}
		else
		{
			S = new GSocket;
			OwnSocket = true;
		}
		T = t;

		InData = false;
		Log = l;
		Temp = 0;
		SepLen = strlen(PopOverHttpSep) - 1;
	}

	~MailSocket()
	{
		DeleteObj(Temp);
		if (OwnSocket)
		{
			DeleteObj(S);
		}
	}

	// Stream
	int Open(char *Str, int Int) { return S->Open(Str, Int); }
	bool IsOpen() { return S->IsOpen(); }
	int Close() { return S->Close(); }
	int64 GetSize() { return S->GetSize(); }
	int64 SetSize(int64 Size) { return S->SetSize(Size); }
	int64 GetPos() { return S->GetPos(); }
	int64 SetPos(int64 Pos) { return S->SetPos(Pos); }
	GStreamI *Clone() { return S->Clone(); }

	// Socket
	OsSocket Handle(OsSocket Set) { return S->Handle(Set); }
	bool GetLocalIp(char *IpAddr) { return S->GetLocalIp(IpAddr); }
	int GetLocalPort() { return S->GetLocalPort(); }
	bool GetRemoteIp(char *IpAddr) { return S->GetRemoteIp(IpAddr); }
	int GetRemotePort() { return S->GetRemotePort(); }
	bool IsReadable() { return S->IsReadable(); }
	bool Listen(int Port = 0) { return S->Listen(Port = 0); }
	bool Accept(GSocketI *c) { return S->Accept(c); }
	int Error(void *param) { return S->Error(param); }
	void OnDisconnect() { S->OnDisconnect(); }
	void OnError(int ErrorCode, char *ErrorDescription) { S->OnError(ErrorCode, ErrorDescription); }
	void OnInformation(char *Str) { S->OnInformation(Str); }
	void OnRead(char *Data, int Len)
	{
		S->OnRead(Data, Len);

		if (Temp)
		{
			Temp->Write(Data, Len);
		}
		else if (!Data)
		{
			ReadBuf.Push(Data, Len);

			char Buf[512];
			while (ReadBuf.Pop(Buf, sizeof(Buf)))
			{
				if (strchr("\r\n", Buf[0]))
				{
					InData = true;
					ReadBuf.Empty();
					break;
				}
				else
				{
					Log->Log(Buf, GSocketI::SocketMsgReceive);
				}
			}
		}
		else if (Log->Items)
		{
			ReadBuf.Push(Data, Len);

			char Buf[512];
			while (ReadBuf.Pop(Buf, sizeof(Buf)))
			{
				if (strncmp(Buf, PopOverHttpSep+1, SepLen) == 0)
				{
					Log->Items->Value++;
				}
			}
		}
	}

	void OnWrite(char *Data, int Len)
	{
		S->OnWrite(Data, Len);

		WriteBuf.Push(Data, Len);

		char Buf[256];
		while (WriteBuf.Pop(Buf, sizeof(Buf)))
		{
			if (!strchr("\r\n", Buf[0]))
			{
				Log->Log(Buf, GSocketI::SocketMsgSend);
			}
		}
	}

	bool SetValue(char *Which, GVariant &What)
	{
		int r = S->SetValue(Which, What);
		if (T && _stricmp(Which, GSocket_TransferSize) == 0)
		{
			T->Range = What.CastInt32();
		}
		return r != 0;
	}

	int Write(const void *Buffer, int Size, int Flags = 0)
	{
		// Remove null characters
		char *o = (char*) Buffer;
		char *i = o;
		while (i < (char*)Buffer+Size)
		{
			if (*i)
			{
				*o++ = *i;
			}
			i++;
		}

		return S->Write(Buffer, o - (char*)Buffer, Flags);
	}
	
	int Read(void *Buffer, int Size, int Flags = 0)
	{
		int s = S->Read(Buffer, Size, Flags);
		if (T && s > 0)
		{
			T->Value += s;
		}
		return s;
	}
};

void MailPhp::SetProxy(char *Server, int Port)
{
	d->ProxyServer = NewStr(Server);
	d->ProxyPort = Port;
}

bool MailPhp::Get(GSocketI *S, char *Uri, GStream &Out, bool MailTransfer)
{
	bool Status = false;

	if (S && Uri)
	{
		char *Start = Uri;
		if (_strnicmp(Start, "http://", 7) == 0)
			Start += 7;
		
		char *s = strchr(Start, '/');
		if (s)
		{
			char *Base = NewStr(Start, s-Start);

			IHttp Http;
			if (d->UserName && d->UserPass)
			{
				Http.SetAuth(d->UserName, d->UserPass);
			}
			if (d->ProxyServer)
			{
				Http.SetProxy(d->ProxyServer, d->ProxyPort);
			}
			
			GAutoPtr<GSocketI> s(new MailSocket(S, MailTransfer ? Transfer : 0, this));
			if (Http.Open(s, Base))
			{
				GStringPipe Buf;
				int Code = 0;
				IHttp::ContentEncoding Enc;
				if (Http.Get(Uri, 0, &Code, &Buf, &Enc))
				{
					char Start[256];
					if (Buf.Peek((uchar*)Start, sizeof(Start)))
					{
						Start[sizeof(Start)-1] = 0;
						if (_stricmp(Start, "Error:") == 0)
						{
							return false;
						}
					}
					
					GCopyStreamer s;
					Status = s.Copy(&Buf, &Out) > 0;
				}
			}
			
			DeleteArray(Base);
		}
	}

	return Status;
}

int MailPhp::GetMessages()
{
	return d->Messages;
}

bool MailPhp::Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password, GDom *SettingStore, int Flags)
{
	if (S &&
		RemoteHost)
	{
		d->HeadersRetreived = false;
		DeleteArray(d->UserName);
		d->UserName = NewStr(User);
		DeleteArray(d->UserPass);
		d->UserPass = NewStr(Password);

		uint64 Token = LgiCurrentTime();
		strcpy_s(d->Uri, sizeof(d->Uri), RemoteHost);

		char *e = d->Uri + strlen(d->Uri);
		sprintf_s(e, sizeof(d->Uri)-(e-d->Uri), "?token="LGI_PrintfInt64, Token);
		
		char *m = 0;
		GStringPipe Text;
		Socket.Reset(S);
		if (Get(Socket, d->Uri, Text, false))
		{
			m = Text.NewStr();
		}

		*e = 0;
		if (m)
		{
			bool PopOverHttp = false;
			bool GotToken = false;

			GToken Lines(m, "\r\n");
			if (_strnicmp(m, "error:", 6) == 0)
			{
				for (unsigned Line=0; Line<Lines.Length(); Line++)
				{
					Log(Lines[Line] + 7, GSocketI::SocketMsgError);
				}
			}
			else if (Lines.Length() > 1)
			{
				for (unsigned Line=0; Line<Lines.Length(); Line++)
				{
					char *Var = Lines[Line];
					char *Val = strchr(Var, ':');

					if (Val)
					{
						*Val++ = 0;
						while (strchr(" \t", *Val)) Val++;
					
						if (IsDigit(*Var))
						{
							if (PopOverHttp)
							{
								GToken p(Val, " ");
								if (p.Length() > 1)
								{
									Msg *m = new Msg;
									if (m)
									{
										m->Index = atoi(Var);
										m->Size = atoi(p[0]);
										m->Uid = NewStr(p[1]);

										d->Msgs.Insert(m);
									}
								}
							}
						}
						else if (_stricmp(Var, "protocol") == 0)
						{
							PopOverHttp = stristr(Val, "popoverhttp") != 0;
						}
						else if (_stricmp(Var, "messages") == 0)
						{
							d->Messages = atoi(Val);
						}
						else if (_stricmp(Var, "token") == 0)
						{
							int Tok = atoi(Val);
							GotToken = Tok == Token;
						}
					}
				}
			}
			else Log("Empty index page returned.", GSocketI::SocketMsgError);

			DeleteArray(m);
			
			if (!PopOverHttp)
			{
				Log("Page is not a PopOverHttp index.", GSocketI::SocketMsgError);
			}
			if (!GotToken)
			{
				Log("Missing or invalid token. Is your PopOverHttp page broken?", GSocketI::SocketMsgError);
			}

			return	PopOverHttp &&
					GotToken &&
					d->Messages == d->Msgs.Length();
		}
		else Log("No PopOverHttp index page.", GSocketI::SocketMsgError);
	}
	else Log("No remote host.", GSocketI::SocketMsgError);

	return false;
}

bool MailPhp::Close()
{
	if (d->HasDeletes &&
		d->Messages)
	{
		GStringPipe p;
		p.Print("%s?time=%i&delete=", d->Uri, LgiCurrentTime());
		
		bool First = true;
		for (Msg *m = d->Msgs.First(); m; m = d->Msgs.Next())
		{
			if (m->Delete)
			{
				if (!First) p.Push(",");
				else First = false;
				p.Print("%i", m->Index+1);
			}
		}
		
		bool Status = false;
		GSocketI *S = new MailSocket(Socket, 0, this);
		if (S)
		{
			GStringPipe Out;
			
			char *u = p.NewStr();
			if (u)
			{
				if (Get(S, u, Out, false))
				{
					char *m = Out.NewStr();
					if (m)
					{
						Status = stristr("Error:", m) == 0;
						DeleteArray(m);
					}
				}
				
				DeleteArray(u);
			}
		}		
	}
	
	d->Msgs.DeleteObjects();

	return true;
}

bool MailPhp::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	GStringPipe Cmd;

	Cmd.Push(d->Uri);
	Cmd.Print("?time=%i&msg=", LgiCurrentTime());
	for (unsigned i=0; i<Trans.Length(); i++)
	{
		if (i) Cmd.Push(",");
		Cmd.Print("%i", Trans[i]->Index + 1);
	}

	bool Status = false;
	GSocketI *S = new MailSocket(Socket, 0, this);
	if (S)
	{
		// Download all the messages
		GStringPipe Out;
		char *c = Cmd.NewStr();
		Status = Get(S, c, Out, true);
		DeleteArray(c);
		
		// Split them into individual transactions
		char *All = Out.NewStr();
		if (All)
		{
			char *In = All;
			char *Out = All;
			while (*In)
			{
				if (*In != '\r')
				{
					*Out++ = *In;
				}
				In++;
			}
			*Out = 0;

			char *s = All;
			unsigned i = 0;
			for (char *e = strstr(s, PopOverHttpSep); s && e; i++)
			{
				MailTransaction *t = Trans[i];
				if (t && t->Stream)
				{
					int Len = e-s;
					if (!strnstr(s, "Error: ", min(Len, 256)))
					{
						t->Stream->Write(s, Len - 2);
						t->Status = true;
						Status = true;
					}
					else
					{
						printf("%s:%i - Error in Mail.\n", __FILE__, __LINE__);
					}
					
					s = strchr(e + 1, '\n');
					if (s) s++;
					e = s ? strstr(s, PopOverHttpSep) : 0;
				}
				else
				{
					printf("%s:%i - Error: No Stream.\n", __FILE__, __LINE__);
					break;
				}
			}
			
			if (i < Trans.Length())
			{
				printf("%s:%i - Error: Only found %i of %i separators in %i bytes from server.\n",
					__FILE__, __LINE__,
					i, Trans.Length(),
					(int)strlen(All));
			}
			
			DeleteArray(All);
		}
	}		
	
	return Status;
}

bool MailPhp::Delete(int Message)
{
	Msg *m = d->Msgs[Message];
	if (m)
	{
		m->Delete = true;
		d->HasDeletes = true;
		return true;
	}
	
	return false;
}

int MailPhp::Sizeof(int Message)
{
	Msg *m = d->Msgs[Message];
	return m ? m->Size : 0;
}

bool MailPhp::GetSizes(GArray<int> &Sizes)
{
	for (Msg *m = d->Msgs.First(); m; m = d->Msgs.Next())
	{
		Sizes.Add(m->Size);
	}
	return Sizes.Length() == d->Msgs.Length();
}

bool MailPhp::GetUid(int Message, char *Id, int IdLen)
{
	Msg *m = d->Msgs[Message];
	if (!m)
		return false;
	
	strcpy_s(Id, IdLen, m->Uid);
	return true;
}

bool MailPhp::GetUidList(List<char> &Id)
{
	for (Msg *m=d->Msgs.First(); m; m=d->Msgs.Next())
	{
		if (m->Uid)
		{
			Id.Insert(NewStr(m->Uid));
		}
	}

	return Id.Length() > 0;
}

char *MailPhp::GetHeaders(int Message)
{
	Msg *TheMsg = d->Msgs[Message];
	if (TheMsg)
	{
		if (!d->HeadersRetreived)
		{
			d->HeadersRetreived = true;

			char *e = d->Uri + strlen(d->Uri);
			sprintf_s(e, sizeof(d->Uri)-(e-d->Uri), "?top=1");

			GStringPipe Text;
			if (Get(new GSocket, d->Uri, Text, false))
			{
				int n = 0;
				char *All = Text.NewStr();
				int AllLen = strlen(All);
				for (char *s = All; s && *s; )
				{
					Msg *m = d->Msgs[n++];
					if (m)
					{
						DeleteArray(m->Headers);

						char *e = stristr(s, "\r\n.\r\n");
						if (e)
						{
							m->Headers = NewStr(s, e-s);
							s = e + 5;
							while (*s == '\r' || *s == '\n') s++;
						}
						else
						{
							m->Headers = NewStr(s, strlen(s)-3);
							s = 0;
						}
					}
					else break;
				}

				DeleteArray(All);
			}
			*e = 0;
		}

		return NewStr(TheMsg->Headers);
	}

	return 0;
}

