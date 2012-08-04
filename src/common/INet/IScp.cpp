#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Lgi.h"
#include "IScp.h"
#include "INetTools.h"
#include "GToken.h"

char *CmdNames[] =
{
	"(error)",
	"Response",
	"Login",
	"Quit",
	"Search",
	"Result",
	"Create",
	"Load",
	"Store",
	"Delete",
	"Poll",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)",
	"(error)"
};

//////////////////////////////////////////////////////////
IScp::IScp(bool server)
{
	Server = server;

	if (!Proxy.Port)
		Proxy.Port = 80;
	if (!Host.Port)
		Host.Port = SCP_PORT;
}

IScp::~IScp()
{
}

bool IScp::WriteData(GSocketI *&s, IScpData *d)
{
	bool Status = false;

	// LgiTrace("SCP: Write(%p, %p)", s, d);
	if (s AND d)
	{
		if (Server)
		{
			// HTML response
			int Code = d->GetInt("Status");
			int Len = d->Data ? strlen(d->Data) : 0;
			char *Err = d->GetStr("Error");
			char Buf[256];
			sprintf(Buf,
					"HTTP/%.1f %i %s\r\n"
					"Content-Length: %i\r\n"
					"Content-Type: %s\r\n",
					(double)SCP_HTTP_VER/100,
					Code > 0 ? 200 : 500,
					Code > 0 ? (char*)"Ok" : Err ? Err : (char*)"Failed",
					Len,
					d->Cmd == SCP_NOT_SCP ? "text/html" : "text/plain");
			Status = s->Write(Buf, strlen(Buf), 0) == (int)strlen(Buf);
			
			Prop *p;
			for (d->Props.FirstKey(); p=d->Props.GetProp(); d->Props.NextKey())
			{
				char Buf[256];
				switch (p->Type)
				{
					case OBJ_INT:
					{
						sprintf(Buf, "X-%s: %i\r\n", p->Name, p->Value.Int);
						s->Write(Buf, strlen(Buf), 0);
						break;
					}
					case OBJ_STRING:
					{
						sprintf(Buf, "%s: %s\r\n", p->Name, p->Value.Cp);
						s->Write(Buf, strlen(Buf), 0);
						break;
					}
				}
			}
			s->Write((char*)"\r\n", 2, 0);

			if (d->Data)
			{
				Status = s->Write(d->Data, Len, 0) == Len;
			}
		}
		else
		{
			// HTML request
			char Buf[512];
			sprintf(Buf, "%s http://%s:%i/%s", d->Data ? "PUT" : "GET", Host.Host, Host.Port, CmdNames[d->Cmd]);

			if (d->Type != SCP_NONE)
			{
				d->Props.Set("Type", d->Type);
			}
			if (d->UserId)
			{
				d->Props.Set("User", d->UserId);
			}
			if (d->Uid)
			{
				d->Props.Set("Uid", d->Uid);
			}

			bool First = true;
			Prop *p;
			for (d->Props.FirstKey(); p=d->Props.GetProp(); d->Props.NextKey())
			{
				switch (p->Type)
				{
					case OBJ_INT:
					{
						sprintf(Buf+strlen(Buf), "%c%s=%i", First ? '?' : '&', p->Name, p->Value.Int);
						break;
					}
					case OBJ_STRING:
					{
						sprintf(Buf+strlen(Buf), "%c%s=%s", First ? '?' : '&', p->Name, p->Value.Cp);
						break;
					}
				}

				First = false;
			}

			sprintf(Buf+strlen(Buf), " HTTP/%.1f\r\n", (double)SCP_HTTP_VER/100);
			int DataLen = d->Data ? strlen(d->Data) : 0;
			if (d->Data)
			{
				sprintf(Buf+strlen(Buf), "Content-Length: %i\r\n", DataLen);
			}
			if (Proxy.Host)
			{
				sprintf(Buf+strlen(Buf), "Host: %s:%i\r\n", Host.Host, Host.Port);
			}
			strcat(Buf, "\r\n");

			int Len = strlen(Buf);
			Status = s->Write(Buf, Len, 0) == Len;

			// LgiTrace("\n%s", Buf);

			if (d->Data)
			{
				Status |= (s->Write(d->Data, DataLen, 0) == DataLen);
			}
		}
	}

	return Status;
}

bool IsHttpEnd(GStringPipe &p)
{
	bool Status = false;
	int Len = (int)p.GetSize();
	char *Buf = new char[Len+1];
	if (Buf)
	{
		p.Peek((uchar*)Buf, Len);
		Buf[Len] = 0;
		Status = strstr(Buf, "\r\n\r\n") != 0;
		DeleteArray(Buf);
	}

	return Status;
}

bool IScp::ReadData(GSocketI *&s, IScpData *&d, int *HttpErr)
{
	bool Status = false;

	// LgiTrace("SCP: Read(%p, %p)\n", s, d);
	if (s AND s->IsOpen())
	{
		GStringPipe In;
		char RawBuf[1024];
		do
		{
			bool Readable = true;
			int64 Start = LgiCurrentTime();
			while (	s->IsOpen() AND
					!s->IsReadable())
			{
				// LgiTrace("SCP: Socket not readable: %.1fs\n", ((double)(int64)(LgiCurrentTime()-Start)/1000));

				if ( ((LgiCurrentTime()-Start)/1000) > SCP_SOCKET_TIMEOUT)
				{
					// LgiTrace("SCP: Timeout waiting for data.\n");
					
					Readable = false;
					break;
				}

				LgiSleep(10);
			}

			int r = Readable ? s->Read(RawBuf, sizeof(RawBuf), 0) : 0;
			// LgiTrace("SCP: Got %i bytes (%i)\n", r, Readable);
			if (r > 0)
			{
				In.Push(RawBuf, r);
			}
			else
			{
				break;
			}
		}
		while (!IsHttpEnd(In));

		char *Buf = In.NewStr();
		if (Buf)
		{
			bool Error = false;
			char *EndHeaders = strstr(Buf, "\r\n\r\n");
			if (EndHeaders)
			{
				char *Headers = NewStr(Buf, EndHeaders-Buf + 2);
				char *StrContentLen = InetGetHeaderField(Headers, "Content-Length");
				int ContentLen = StrContentLen ? atoi(StrContentLen) : 0;

				GStringPipe DataPipe;
				DataPipe.Push(EndHeaders + 4);
				while (ContentLen > DataPipe.GetSize())
				{
					int r = s->Read(RawBuf, sizeof(RawBuf), 0);
					if (r > 0)
					{
						DataPipe.Push(RawBuf, r);
					}
					else
					{
						Error = true;
						break;
					}
				}

				DeleteArray(StrContentLen);
				DeleteArray(Buf);
				
				char *Data = DataPipe.NewStr();

				if (!Error)
				{
					if (Server)
					{
						// HTML request
						if (strnicmp(Headers, "GET ", 4) == 0 ||
							strnicmp(Headers, "PUT ", 4) == 0)
						{
							char *s = Headers + 4;
							char Uri[256] = "";
							char *e = strchr(s, ' ');
							if (e)
							{
								*e = 0;
								strcpy(Uri, s);
								*e = ' ';
							}

							ScpCommand Cmd = SCP_NULL;
							for (int i=1; i<CountOf(CmdNames); i++)
							{
								if (stristr(Uri, CmdNames[i]))
								{
									Cmd = (ScpCommand)i;
									break;
								}
							}

							if (Cmd)
							{
								d = new IScpData(Cmd);
								if (d)
								{
									s = strchr(s, '?');
									if (s AND *s++ == '?')
									{
										do
										{
											char *eq = strchr(s, '=');
											if (eq)
											{
												*eq++ = 0;
												char *end = strchr(eq, '&');
												if (!end) end = strchr(eq, ' ');
												if (!end) break;
												*end++ = 0;

												if (*eq == '-' || isdigit(*eq))
												{
													int i=atoi(eq);
													d->Props.Set(s, i);
												}
												else
												{
													d->Props.Set(s, eq);
												}

												s = end;
											}
											else break;
										}
										while (true);
									}

									d->UserId = 0;
									d->Data = Data;
									Data = 0;
									d->Props.Get("Type", (int&)d->Type);
									d->Props.Get("User", d->UserId);
									d->Props.Get("Uid", d->Uid);
								}
							}
							else
							{
								d = new IScpData(SCP_NOT_SCP);
							}

							Status = true;
						}
					}
					else
					{
						// HTML response
						GToken T(Headers, "\r\n");
						if (T.Length() > 0)
						{
							bool Http11 = strnicmp("http/1.1", T[0], 8) == 0;
							bool Http10 = strnicmp("http/1.0", T[0], 8) == 0;

							if (Http11 || Http10)
							{
								char *s = strchr(T[0], ' ');
								if (s)
								{
									int Http = atoi(s+1);
									if (Http / 100 == 2) //  || Http == 500)
									{
										if (!d)
										{
											d = new IScpData(SCP_RESPONSE);
										}
										else
										{
											d->Cmd = SCP_RESPONSE;
										}
										if (d)
										{
											for (int i=1; i<T.Length(); i++)
											{
												char *Line = T[i];
												if (Line[0] == 'X' AND
													Line[1] == '-')
												{
													char *Var = Line + 2;
													char *Val = strchr(Var, ':');
													if (Val)
													{
														char *Colon = Val;
														*Val++ = 0;
														Val++;
														if (*Val == '-' || isdigit(*Val))
														{
															d->Props.Set(Var, atoi(Val));
														}
														else
														{
															d->Props.Set(Var, Val);
														}

														*Colon = ':';
													}
												}
											}

											d->Data = Data;
											Data = 0;

											Status = true;
										}
									}
									else
									{
										char b[256];
										sprintf(b, "IScp::ReadData(...) HTTP error: %s\n", s+1);
										Info.Push(b);

										if (HttpErr)
										{
											*HttpErr = Http;
										}
									}
								}
							}

							char *Connection = InetGetHeaderField(Headers, "Connection");
							// char *ProxyConnection = InetGetHeaderField(Headers, "Proxy-Connection");

							if (Http10 ||
								(Connection AND stricmp(Connection, "close") == 0))
							{
								s->Close();
							}
						}

						/*
						#ifdef _DEBUG
						if (!Status)
						{
							LgiMsg(0, "%s\n\n%s", "Error", MB_OK, Headers, Data);
						}
						#endif
						*/
					}

					DeleteArray(Headers);
					DeleteArray(Data);
				}
			}
			else
			{
				LgiTrace("%s:%i - No end of headers.\n", __FILE__, __LINE__);
			}
		}
		else
		{
			Info.Push("IScp::ReadData(...) No data on socket.\n");
			s->Close();
		}
	}

	return Status;
}

//////////////////////////////////////////////////////////
IScpSearch::IScpSearch()
{
	UserId = 0;
	Param = 0;
	Type = SCP_NONE;
}

IScpSearch::~IScpSearch()
{
	DeleteArray(Param);
}

//////////////////////////////////////////////////////////
List<IScpData> IScpData::Uids;

IScpData::IScpData(ScpCommand c)
{
	Cmd = c;
	Uid = 0;
	Type = SCP_NONE;
	Data = 0;
	UserId = 0;
	Uids.Insert(this);
}

IScpData::~IScpData()
{
	Uids.Delete(this);
	DeleteArray(Data);
}

IScpData *IScpData::GetObject(int id)
{
	for (IScpData *d=Uids.First(); d; d=Uids.Next())
	{
		if (d->Uid == id) return d;
	}

	return 0;
}

char *IScpData::GetStr(char *Field)
{
	char *s = 0;;
	Props.Get(Field, s);
	return s;
}

void IScpData::SetStr(char *Field, char *s)
{
	if (s)
	{
		Props.Set(Field, s);
	}
	else
	{
		Props.DeleteKey(Field);
	}
}

int IScpData::GetInt(char *Field)
{
	int s = 0;
	Props.Get(Field, s);
	return s;
}

void IScpData::SetInt(char *Field, int i)
{
	Props.Set(Field, i);
}

//////////////////////////////////////////////////////////
IScpClient::IScpClient() : IScp(false)
{
	s = 0;
	Loop = 0;
}

IScpClient::~IScpClient()
{
}

GSocketI *&IScpClient::Socket(bool Op)
{
	if (!s)
	{
		s = new GSocket;
		if (s)
		{
			s->IsDelayed(false);
		}
	}

	if (s AND Op AND !s->IsOpen())
	{
		GUri *Connect = (Proxy.Host) ? &Proxy : &Host;
		s->Open(Connect->Host, Connect->Port);
	}

	return s;
}

bool IScpClient::Request(IScpData *out, IScpData *&in)
{
	bool Status = false;

	for (int i=0; (!Loop || *Loop) AND i<SCP_RETRIES; i++)
	{
		if (WriteData(Socket(), out))
		{
			int HttpErr = 0;
			if (ReadData(Socket(false), in, &HttpErr))
			{
				Status = true;
				break;
			}
			else
			{
				// printf("%s:%i - SCP read failed.\n", __FILE__, __LINE__);

				if (HttpErr == 500 || HttpErr < 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("%s:%i - SCP write failed.\n", __FILE__, __LINE__);
		}
	}
	
	return Status;
}

bool IScpClient::Connect(GSocketI *sock, char *Server, char *User, char *Pass)
{
	bool Status = false;

	s = sock;
	if (Server AND User)
	{
		bool Open = false;

		Host = Server;
		s->IsDelayed(true);
		if (Proxy.Host)
		{
			Open = s->Open(Proxy.Host, Proxy.Port) != 0;
		}
		else if (Host.Host)
		{
			Open = s->Open(Host.Host, Host.Port) != 0;
		}

		if (Open)
		{
			IScpData Login(SCP_LOGIN);

			Login.SetStr("User", User);
			Login.SetStr("Pass", Pass);

			IScpData *Dec = 0;
			if (Request(&Login, Dec))
			{
				Status = Dec->GetInt("Status") > 0;
				Session = Dec->GetInt("Session");
				Info.Push("IScpClient::Connect login ok.\n");
			}
			else
			{
				Info.Push("IScpClient::Connect login failed.\n");
			}
		}
	}

	return Status;
}

bool IScpClient::Quit()
{
	bool Status = false;

	IScpData Quit(SCP_QUIT);
	Quit.SetInt("Session", Session);
	if (WriteData(Socket(), &Quit))
	{
		Status = true;
	}

	return Status;
}

bool IScpClient::Search(IScpSearch *Param, IScpData *&Results)
{
	bool Status = false;

	if (Param)
	{
		IScpData Search(SCP_SEARCH);

		char Start[32];
		char End[32];
		Param->Start.Get(Start);
		Param->End.Get(End);

		Search.SetInt("Session", Session);
		Search.SetInt("UserId", Param->UserId);
		Search.SetInt("Type", Param->Type);
		Search.SetStr("Start", Param->Start.Year() ? Start : 0);
		Search.SetStr("End", Param->End.Year() ? End : 0);
		if (Param->Param)
		{
			Search.SetStr("Param", Param->Param);
		}

		if (Request(&Search, Results = new IScpData(SCP_RESULT)))
		{
			if (Results AND
				Results->GetInt("Status") > 0)
			{
				Status = true;
			}
			else
			{
				if (Results)
				{
					char s[256];
					sprintf(s, "IScpClient::Search(SCP_SEARCH): Read status=%i\n", Results->GetInt("Status"));
					Info.Push(s);
				}
				else
				{
					Info.Push("IScpClient::Search(SCP_SEARCH): No results.\n");
				}
			}
		}
		else
		{
			Info.Push("IScpClient::Search(SCP_SEARCH): Http request failed.\n");
		}

		if (!Status)
		{
			DeleteObj(Results);
		}
	}

	return Status;
}

bool IScpClient::Load(int Uid, IScpData *Obj)
{
	bool Status = false;

	IScpData Data(SCP_LOAD);
	Data.SetInt("Uid", Uid);
	Data.SetInt("Session", Session);
	if (Obj AND Request(&Data, Obj))
	{
		if (Obj->Cmd == SCP_RESPONSE)
		{
			int i = Obj->GetInt("Status");
			if (i > 0)
			{
				Status = true;
			}
		}
	}

	return Status;
}

bool IScpClient::Save(IScpData *Data)
{
	bool Status = false;

	if (Data)
	{
		Data->Cmd = SCP_STORE;
		Data->SetInt("Session", Session);
		IScpData *Dec = 0;

		if (Request(Data, Dec))
		{
			if (Dec->Cmd == SCP_RESPONSE)
			{
				int Code = Dec->GetInt("Status");
				if (Code)
				{
					if (Data->GetUid() == 0)
					{
						Data->SetUid(Code);
					}
					
					Status = Code > 0;
				}
			}
		}
	}

	return Status;
}

bool IScpClient::Delete(IScpData *Data)
{
	bool Status = false;

	if (Data)
	{
		Data->Cmd = SCP_DELETE;
		Data->SetInt("Session", Session);

		IScpData *Dec = 0;
		if (Request(Data, Dec))
		{
			if (Dec->Cmd == SCP_RESPONSE)
			{
				int i = Dec->GetInt("Status");
				Status = i > 0;
			}

			DeleteObj(Dec);
		}
	}

	return Status;
}

bool IScpClient::Poll(List<int> &Changed)
{
	bool Status = false;

	IScpData Data(SCP_POLL);
	Data.SetInt("Session", Session);

	IScpData *Dec = 0;
	if (Request(&Data, Dec))
	{
		if (Dec->Cmd == SCP_RESPONSE)
		{
			int i = Dec->GetInt("Status");
			if (i > 0)
			{
				GToken T(Dec->Data, "\r\n");
				for (int i=0; i<T.Length(); i++)
				{
					Changed.Insert(new int(atoi(T[i])));
				}

				Status = true;
			}
		}

		DeleteObj(Dec);
	}

	return Status;
}

//////////////////////////////////////////////////////////
IScpServer::IScpServer(IScpUsage *usage) : IScp(true)
{
	Usage = usage;
	
	printf("IScpServer running\n");
}

IScpServer::~IScpServer()
{
}

bool IScpServer::Respond(GSocketI *s, int Code)
{
	IScpData r(SCP_RESPONSE);

	r.SetInt("Status", Code);
	r.Data = (Code) ? NewStr("SCP: Success") : NewStr("SCP: Failed.");

	printf("Doing Writedata\n");
	return WriteData(s, &r);
}

bool IScpServer::OnIdle(GSocketI *s)
{
	if (s)
	{
		IScpData *d;
		
		while (ReadData(s, d))
		{
			int SeshId = d->GetInt("session");

			switch (d->Cmd)
			{
				case SCP_LOGIN:
				{
					char *User = d->GetStr("User");
					char *Pass = d->GetStr("Pass");
					if (User AND Pass)
					{
						SeshId = (int)((int64)LgiGetCurrentThread() ^ LgiCurrentTime());
						bool Ok = OnLogin(User, Pass, SeshId);
						IScpData r(SCP_RESPONSE);

						r.SetInt("Status", Ok);
						r.SetInt("Session", SeshId);
						r.Data = NewStr((char*)(Ok?"SCP: Success":"SCP: Failed."));

						return WriteData(s, &r);
					}
					break;
				}
				case SCP_QUIT:
				{
					OnQuit();
					s->Close();
					return false;
					break;
				}
				case SCP_SEARCH:
				{
					if (LoggedIn(SeshId))
					{
						IScpSearch Search;
						Search.UserId = d->GetInt("UserId");
						Search.Type = (ScpObject)d->GetInt("Type");
						Search.Start.Set(d->GetStr("Start"));
						Search.End.Set(d->GetStr("End"));
						Search.Param = NewStr(d->GetStr("Param"));

						IScpData Response(SCP_RESPONSE);
						Response.SetInt("Status", OnSearch(&Search, &Response));
						WriteData(s, &Response);
					}
					break;
				}
				case SCP_STORE:
				{
					if (LoggedIn(SeshId))
					{
						Respond(s, OnSave(d));
					}
					break;
				}
				case SCP_DELETE:
				{
					if (LoggedIn(SeshId))
					{
						Respond(s, OnDelete(d));
					}
					break;
				}
				case SCP_POLL:
				{
					if (LoggedIn(SeshId))
					{
						IScpData r(SCP_RESPONSE);
						r.SetInt("Status", OnPoll(&r));
						return WriteData(s, &r);
					}
					break;
				}
				case SCP_LOAD:
				{
					if (LoggedIn(SeshId))
					{
						IScpData r(SCP_RESPONSE);
						int Uid = d->GetInt("Uid");
						r.SetInt("Status", Uid ? OnLoad(Uid, &r) : false);
						return WriteData(s, &r);
					}
					break;
				}
				case SCP_NOT_SCP:
				{
					char *u = Usage ? Usage->GetUsage(this) : 0;
					if (u)
					{
						IScpData d(SCP_NOT_SCP);
						d.Data = u;
						return WriteData(s, &d);
					}
					break;
				}
			}

			DeleteObj(d);
		}
	}

	return true;
}
