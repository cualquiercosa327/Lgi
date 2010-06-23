#include "Lgi.h"
#include "GHttpUi.h"
#include "INet.h"
#include "INetTools.h"

struct GHttpServerPriv;

#define LOG_HTTP            0

class GHttpThread : public GThread
{
	GSocket *s;
	GHttpServerPriv *p;

public:
	GHttpThread(GSocket *sock, GHttpServerPriv *priv);
	int Main();
};

class GHttpServer_TraceSocket : public GSocket
{
public:
	void OnInformation(char *Str)
	{
		LgiTrace("%s:%i - SocketInfo: %s\n", _FL, Str);
	}

	void OnError(int ErrorCode, char *ErrorDescription)
	{
		LgiTrace("%s:%i - SocketError %i: %s\n", _FL, ErrorCode, ErrorDescription);
	}
};

struct GHttpServerPriv : public GThread
{
	GHttpCallback *Callback;
	GHttpServer_TraceSocket Listen;
	int Port;

	GHttpServerPriv(GHttpCallback *cb, int port)
	{
		Callback = cb;
		Port = port;
		Run();
	}

	~GHttpServerPriv()
	{
		Listen.Close();
		while (!IsExited())
		{
			LgiSleep(50);
		}
	}

	int Main()
	{
	    #if LOG_HTTP
		LgiTrace("Attempting to listen on port %i...\n", Port);
		#endif
		if (!Listen.Listen(Port))
		{
			LgiTrace("%s:%i - Can't listen on port %i.\n", _FL, Port);
			return false;
		}

        #if LOG_HTTP
		LgiTrace("Listening on port %i.\n", Port);
		#endif
		while (Listen.IsOpen())
		{
			GSocket *s = new GSocket;
			#if LOG_HTTP
			LgiTrace("Accepting...\n");
			#endif
			if (Listen.Accept(s))
			{
                #if LOG_HTTP
				LgiTrace("Got new connection...\n");
				#endif
				new GHttpThread(s, this);
			}
		}

		return 0;
	}
};

GHttpThread::GHttpThread(GSocket *sock, GHttpServerPriv *priv)
{
	s = sock;
	p = priv;
	Run();
}

int GHttpThread::Main()
{
	GArray<char> Buf;
	int Block = 4 << 10, Used = 0, ContentLen = 0;
	int HeaderLen = 0;
	Buf.Length(Block);

	// Read headers...
	while (s->IsOpen())
	{
		if (Buf.Length() - Used < 1)
			Buf.Length(Buf.Length() + Block);

		int r = s->Read(&Buf[Used], Buf.Length()-Used, 0);
		#if LOG_HTTP
		LgiTrace("%s:%i - read=%i\n", _FL, r);
		#endif
		if (r > 0)
		{		
			Used += r;
		}

		char *Eoh;
		if (!HeaderLen &&
			(Eoh = strnistr(&Buf[0], "\r\n\r\n", Used)))
		{
			// Found end of headers...
			HeaderLen = Eoh - &Buf[0] + 4;
			#if LOG_HTTP
			LgiTrace("%s:%i - HeaderLen=%i\n", _FL, HeaderLen);
			#endif
			char *s = InetGetHeaderField(&Buf[0], "Content-Length", HeaderLen);
			if (s)
			{
				ContentLen = atoi(s);
				#if LOG_HTTP
				LgiTrace("%s:%i - ContentLen=%i\n", _FL, ContentLen);
				#endif
			}
			else break;
		}

		if (ContentLen > 0)
		{
			if (Used >= HeaderLen + ContentLen)
				break;
		}

		if (r < 1)
			break;
	}

	Buf[Used] = 0;
	#if LOG_HTTP
	LgiTrace("%s:%i - got headers\n", _FL);
	#endif

	char *Action = &Buf[0];
	char *Eol = strnstr(Action, "\r\n", Used);
	#if LOG_HTTP
	LgiTrace("%s:%i - eol=%p\n", _FL, Eol);
	#endif
	if (Eol)
	{
		*Eol = 0;
		Eol += 2;
		int FirstLine = Eol - Action;

		char *Uri = strchr(Action, ' ');
		if (Uri)
		{
			*Uri++ = 0;
			#if LOG_HTTP
			LgiTrace("%s:%i - uri='%s'\n", _FL, Uri);
			#endif

			char *Protocol = strrchr(Uri, ' ');
			if (Protocol)
			{
				*Protocol++ = 0;

				#if LOG_HTTP
				LgiTrace("%s:%i - protocol='%s'\n", _FL, Protocol);
				#endif
				if (p->Callback)
				{
					char *Eoh = strnistr(Eol, "\r\n\r\n", Used - FirstLine);
					if (Eoh)
						*Eoh = 0;

					GVariant Out;
					int Code = p->Callback->OnRequest(Action, Uri, Eol, Eoh + 4, Out);
					#if LOG_HTTP
					LgiTrace("%s:%i - Code=%i\n", _FL, Code);
					#endif

					s->Print("HTTP/1.0 %i Ok\r\n\r\n", Code);
					char *Response = Out.Str();
					#if LOG_HTTP
					LgiTrace("%s:%i - Response=%p\n", _FL, Response);
					#endif
					if (Response)
					{
						int Len = strlen(Out.Str());
					    #if LOG_HTTP
						LgiTrace("%s:%i - Len=%p\n", _FL, Len);
						#endif
						s->Write(Out.Str(), Len);
					}
				}
			}
		}
	}

	s->Close();
	DeleteObj(s);

	return 0;
}

GHttpServer::GHttpServer(GHttpCallback *cb, int port)
{
	d = new GHttpServerPriv(cb, port);
}

GHttpServer::~GHttpServer()
{
	DeleteObj(d);
}

char *GHttpCallback::FormDecode(char *s)
{
	char *i = s, *o = s;
	while (*i)
	{
		if (*i == '+')
		{
			*o++ = ' ';
			i++;
		}
		else if (*i == '%')
		{
			char h[3] = {i[1], i[2], 0};
			*o++ = htoi(h);
			i += 3;
		}
		else
		{
			*o++ = *i++;
		}
	}
	*o = 0;
	return s;
}

char *GHttpCallback::HtmlEncode(char *s)
{
	GStringPipe p;
	char *e = "<>";

	while (s && *s)
	{
		char *b = s;
		while (*s && !strchr(e, *s)) s++;
		
		if (s > b)
			p.Write(b, s - b);
		
		if (*s)
		{
			p.Print("&#%i;", *s);
			s++;
		}
		else break;
	}

	return p.NewStr();
}

bool GHttpCallback::ParseHtmlWithDom(GVariant &Out, GDom *Dom, char *Html)
{
	if (!Dom || !Html)
		return false;

	GStringPipe p;
	for (char *s = Html; s && *s; )
	{
		char *e = stristr(s, "<?");
		if (e)
		{
			p.Write(s, e - s);
			s = e + 2;
			while (*s && strchr(" \t\r\n", *s)) s++;
			char *v = s;
			while (*s && (isdigit(*s) || isalpha(*s) || strchr(".[]", *s)) ) s++;
			char *Var = NewStr(v, s - v);
			while (*s && strchr(" \t\r\n", *s)) s++;
			if (strncmp(s, "?>", 2) == 0)
			{
				GVariant Value;
				if (Dom->GetValue(Var, Value))
				{
					p.Push(Value.CastString());
				}

				s += 2;
			}
			else break;
		}
		else
		{
			p.Push(s);
			break;
		}
	}

	Out.Empty();
	Out.Type = GV_STRING;
	Out.Value.String = p.NewStr();
	return true;
}
