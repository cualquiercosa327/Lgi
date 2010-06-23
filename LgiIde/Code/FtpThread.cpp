#include <stdio.h>

#include "Lgi.h"
#include "FtpThread.h"

static FtpThread *Ftp = 0;

class Line : public GListItem
{
	COLOUR c;

public:
	Line(char *s, COLOUR col)
	{
		SetText(s);
		c = col;
	}

	void OnPaint(GItem::ItemPaintCtx &Ctx)
	{
		if (!Select())
			Ctx.Fore = c;
		GListItem::OnPaint(Ctx);
	}
};

class LogSock : public GSocket
{
	GStringPipe r, w;

	void Pop(GStringPipe &p, COLOUR c)
	{
		char s[256];
		while (p.Pop(s, sizeof(s)))
		{
			char *e = s + strlen(s) - 1;
			while (e > s AND strchr(" \t\r\n", e[-1]))
			{
				e--;
				*e = 0;
			}
			
			if (Log)
			{
				GListItem *i;
				Log->Insert(i = new Line(s, c));
				i->ScrollTo();
			}
		}
	}

public:
	GList *Log;

	LogSock(GList *log)
	{
		Log = log;
	}

	void OnRead(char *Data, int Len)
	{
		r.Write(Data, Len);
		Pop(r, Rgb24(0, 0xc0, 0));
	}

	void OnWrite(char *Data, int Len)
	{
		w.Write(Data, Len);
		Pop(w, Rgb24(0, 0, 0xc0));
	}

	void OnError(int ErrorCode, char *ErrorDescription)
	{
		char s[256];
		sprintf(s, "Error(%i): %s", ErrorCode, ErrorDescription);
		Log->Insert(new Line(s, Rgb24(255, 0, 0)));
	}
};

struct FtpConn
{
	GUri *Base;
	LogSock *Sock;
	IFtp Ftp;

	FtpConn(char *u)
	{
		Base = new GUri(u);
		Sock = 0;
	}

	void SetLog(GList *l)
	{
		if (Sock)
			Sock->Log = l;
	}

	bool Match (char *Uri)
	{
		GUri u(Uri);
		
		if (Base AND
			u.Host AND Base->Host AND
			u.User AND Base->User AND
			u.Pass AND Base->Pass)
		{
			return	stricmp(u.Host, Base->Host) == 0 AND
					strcmp(u.User, Base->User) == 0 AND
					strcmp(u.Pass, Base->Pass) == 0;
		}

		return false;
	}

	bool Open(GList *Watch)
	{
		if (Base)
		{
			if
			(
				Ftp.Open
				(
					Sock = new LogSock(Watch),
					Base->Host,
					Base->Port ? Base->Port : 21,
					Base->User,
					Base->Pass
				)
				==
				FO_Connected
			)
				return true;
		}
		return false;
	}
};

////////////////////////////////////////////////////////////////////////////////////
FtpCmd::FtpCmd(FtpCommand c, FtpCallback *cb)
{
	Cmd = c;
	Callback = cb;
	Uri = 0;
	File = 0;
	Watch = 0;
	Status = false;
	View = 0;
}

FtpCmd::~FtpCmd()
{
	DeleteArray(Uri);
	DeleteArray(File);
	Dir.DeleteObjects();
}

void FtpCmd::Error(char *e)
{
	if (Watch)
	{
		Watch->Insert(new Line(e, Rgb24(255, 0, 0)));
	}
}

////////////////////////////////////////////////////////////////////////////////////
#define M_FTP_CMD			(M_USER + 0x4000)

class FtpRedir : public GWindow
{
public:
	FtpRedir()
	{
		GRect r(-20001, -20001, -20000, -20000);
		SetPos(r);
		Attach(0);
	}

	int OnEvent(GMessage *m)
	{
		if (MsgCode(m) == M_FTP_CMD)
		{
			FtpCmd *c = (FtpCmd*) MsgA(m);
			c->Callback->OnCmdComplete(c);
			DeleteObj(c);
		}

		return GWindow::OnEvent(m);
	}
};

struct FtpThreadPriv : public GSemaphore, public GNetwork
{
	bool Loop;
	GArray<FtpConn*> Conn;
	GArray<FtpCmd*> Cmds;
	FtpRedir *Redir;

	FtpThreadPriv()
	{
		Loop = true;
		Redir = new FtpRedir;
	}

	~FtpThreadPriv()
	{
		DeleteObj(Redir);
	}

	FtpConn *GetConn(char *Uri, GList *Watch = 0)
	{
		// Look for existing connection
		for (int i=0; i<Conn.Length(); i++)
		{
			if (Conn[i]->Match(Uri))
			{
				Conn[i]->SetLog(Watch);
				return Conn[i];
			}
		}

		// Create new connection
		FtpConn *n = new FtpConn(Uri);
		if (n)
		{
			if (n->Open(Watch))
			{
				Conn.Add(n);
			}
			else DeleteObj(n);

			return n;
		}

		return 0;
	}

	void FreeConn(FtpConn *c)
	{
		Conn.Delete(c);
		DeleteObj(c);
	}
};

FtpThread::FtpThread()
{
	d = new FtpThreadPriv;
	Run();
}

FtpThread::~FtpThread()
{
	d->Loop = false;

	while (!IsExited())
	{
		LgiSleep(1);
	}
	
	DeleteObj(d);
}

void FtpThread::Post(FtpCmd *Cmd)
{
	GSemaphore::Auto Lock(d, _FL);
	d->Cmds.Add(Cmd);
}

int FtpThread::Main()
{
	while (d->Loop)
	{
		FtpCmd *c = 0;
		if (d->LockWithTimeout(50, _FL))
		{
			if (d->Cmds.Length())
			{
				c = d->Cmds[0];
				d->Cmds.DeleteAt(0, true);
			}
			d->Unlock();
		}

		if (c)
		{
			switch (c->Cmd)
			{
				case FtpList:
				{
					if (c->Uri)
					{
						FtpConn *Conn = d->GetConn(c->Uri, c->Watch);
						if (Conn)
						{
							if (Conn->Base->Path)
							{
								if (!Conn->Ftp.SetDir(Conn->Base->Path))
								{
									c->Error("Couldn't set path.");
								}
							}

							c->Status = Conn->Ftp.ListDir(&c->Dir);
							if (!c->Status)
								c->Error("Failed to list folder.");
						}
						else c->Error("Failed to connect.");
					}
					break;
				}
				case FtpRead:
				{
					if (c->Uri)
					{
						FtpConn *Conn = d->GetConn(c->Uri, c->Watch);
						if (Conn)
						{
							GUri u(c->Uri);
							if (u.Path)
							{
								char p[256];
								strsafecpy(p, u.Path, sizeof(p));
								char *d = strrchr(p, '/');
								if (d) *d = 0;

								if (!Conn->Ftp.SetDir(p))
								{
									c->Error("Couldn't set path.");
									break;
								}
							}

							char r[256], *d = strrchr(c->Uri, '/');
							LgiGetTempPath(r, sizeof(r));
							LgiMakePath(r, sizeof(r), r, d + 1);
							if (FileExists(r))
							{
								FileDev->Delete(r, false);
							}
							
							IFtpEntry e;
							e.Size = -1;
							e.Name.Reset(NewStr(d + 1));
							c->Status = Conn->Ftp.DownloadFile(r, &e);
							if (c->Status)
							{
								c->File = NewStr(r);
							}
							else
							{
								c->Error("Download failed.");
							}
						}
						else c->Error("Failed to connect.");
					}
					break;
				}
				case FtpWrite:
				{
					if (c->Uri)
					{
						FtpConn *Conn = d->GetConn(c->Uri, c->Watch);
						if (Conn)
						{
							GUri u(c->Uri);
							if (u.Path)
							{
								char p[256];
								strsafecpy(p, u.Path, sizeof(p));
								char *d = strrchr(p, '/');
								if (d) *d = 0;

								if (!Conn->Ftp.SetDir(p))
								{
									c->Error("Couldn't set path.");
									break;
								}
							}

							if (FileExists(c->File))
							{
								char *d = strrchr(c->File, DIR_CHAR);
								c->Status = Conn->Ftp.UploadFile(c->File, d + 1);
								if (!c->Status)
								{
									c->Error("Upload failed.");
								}
							}
							else c->Error("Source file doesn't exist");
						}
						else c->Error("Failed to connect.");
					}
					break;
				}
				default:
				{
					LgiAssert(!"Not implemented.");
					break;
				}
			}

			if (c->Callback)
			{
				d->Redir->PostEvent(M_FTP_CMD, (int)c);
			}
			else DeleteObj(c);
		}
		else LgiSleep(1);
	}

	return 0;
}

FtpThread *GetFtpThread()
{
	if (!Ftp)
		Ftp = new FtpThread;
	return Ftp;
}

void ShutdownFtpThread()
{
	DeleteObj(Ftp);
}

