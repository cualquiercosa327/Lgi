#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "HttpTools.h"
#include "INet.h"
#include "INetTools.h"
#include "GToken.h"
#include "IHttp.h"

#ifdef _DEBUG
// #define PROXY_OVERRIDE		"10.10.0.50:8080"
#endif

GXmlTag *GetFormField(GXmlTag *Form, char *Field)
{
	if (Form && Field)
	{
		char *Ast = strchr(Field, '*');
		GArray<GXmlTag*> Matches;

		for (GXmlTag *x = Form->Children.First(); x; x = Form->Children.Next())
		{
			char *Name = x->GetAttr("Name");
			if (Name)
			{
				if (Ast)
				{
					if (MatchStr(Field, Name))
					{
						Matches.Add(x);
					}
				}
				else if (_stricmp(Name, Field) == 0)
				{
					Matches.Add(x);
				}
			}
		}

		if (Matches.Length() >= 1)
		{
			return Matches[0];
		}
		/*
		else if (Matches.Length() > 1)
		{
			for (int i=0; i<Matches.Length(); i++)
			{
				LgiTrace("Match[%i]=%s\n", i, Matches[i]->GetAttr("Name"));
			}

			LgiAssert(0);
		}
		*/
	}

	return 0;
}

void StrFormEncode(GStream &p, char *s, bool InValue)
{
	for (char *c = s; *c; c++)
	{
		if (isalpha(*c) || isdigit(*c) || *c == '_' || *c == '.' || (!InValue && *c == '+') || *c == '-' || *c == '%')
		{
			p.Write(c, 1);
		}
		else if (*c == ' ')
		{
			p.Write((char*)"+", 1);
		}
		else
		{
			p.Print("%%%02.2X", *c);
		}
	}
}

// This just extracts the forms in the HTML and makes an XML tree of what it finds.
GXmlTag *ExtractForms(char *Html, GStream *Log)
{
	GXmlTag *f = 0;

	if (Html)
	{
		WebPage Page(Html, Log);
		if (Page.Html)
		{
			GXmlTag *x = Page.GetRoot(Log);
			if (x)
			{
				for (GXmlTag *c = x->Children.First(); c; c = x->Children.Next())
				{
					if (c->IsTag("form"))
					{
						if (!f)
						{
							f = new GXmlTag("Forms");
						}
						if (f)
						{
							GXmlTag *Form = new GXmlTag("Form");
							if (Form)
							{
								f->InsertTag(Form);

								char *s = c->GetAttr("action");
								if (s)
								{
									GStringPipe p;
									for (char *c = s; *c; c++)
									{
										if (*c == ' ')
										{
											char h[6];
											sprintf_s(h, sizeof(h), "%%%2.2X", (uint8)*c);
											p.Push(h);
										}
										else p.Push(c, 1);
									}
									char *e = p.NewStr();
									Form->SetAttr("action", e);
									DeleteArray(e);
								}
								s = c->GetAttr("method");
								if (s) Form->SetAttr("method", s);

								while (c)
								{
									#define CopyAttr(name) \
									{ char *s = c->GetAttr(name); \
									if (s) i->SetAttr(name, s); }

									if (c->IsTag("input"))
									{
										GXmlTag *i = new GXmlTag("Input");
										if (i)
										{
											Form->InsertTag(i);

											CopyAttr("type");
											CopyAttr("name");
											CopyAttr("value");
										}
									}

									if (c->IsTag("select"))
									{
										GXmlTag *i = new GXmlTag("Select");
										if (i)
										{
											Form->InsertTag(i);

											CopyAttr("name");

											while ((c && !c->GetTag())
                                                   ||
                                                   _stricmp(c->GetTag(), "/select") != 0)
											{
												if (c->IsTag("option"))
												{
													GXmlTag *o = new GXmlTag("Option");
													if (o)
													{
														char *s = c->GetAttr("Value");
														if (s) o->SetAttr("Value", s);
														o->SetContent(c->GetContent());
														i->InsertTag(o);															
													}
												}

												c = x->Children.Next();
											}
										}
									}
																			
									if (c->IsTag("/form"))
									{
										break;
									}

									c = x->Children.Next();
								}
							}
						}
					}
				}
			}
		}
		else if (Log) Log->Print("%s:%i - No html after parsing out scripts\n", __FILE__, __LINE__);
	}
	else if (Log) Log->Print("%s:%i - No html.\n", __FILE__, __LINE__);

	return f;
}

void XmlToStream(GStream *s, GXmlTag *x, char *Style)
{
	if (s && x)
	{
		GStringPipe p(1024);
		GXmlTree t;
		if (Style) t.SetStyleFile(Style, "text/xsl");
		t.Write(x, &p);

		int Len = (int)p.GetSize();
		char *Xml = p.NewStr();
		if (Xml)
		{
			s->Write(Xml, Len);
			DeleteArray(Xml);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
WebPage::WebPage(char *Page, GStream *Log)
{
	Html = 0;
	Script = 0;
	Parsed = 0;
	Charset = 0;

	if (Page)
	{
		// Parse out the scripts...
		GStringPipe h, scr;
		for (char *s = Page; s && *s; )
		{
			char *e = stristr(s, "<script");
			if (e)
			{
				e = strchr(e + 1, '>');
				if (e)
				{
					e++;
					h.Push(s, (int) (e - s));
					s = e;
					e = stristr(s, "</script>");
					if (e)
					{
						scr.Push(s, (int) (e - s));
						s = e;
					}
					else
					{
						if (Log) Log->Print("%s:%i - No end of script tag????\n", __FILE__, __LINE__);
						scr.Push(s);
						break;
					}
				}
				else break;
			}
			else
			{
				h.Push(s);
				break;
			}
		}

		Html = h.NewStr();
		Script = scr.NewStr();
	}
}

WebPage::~WebPage()
{
	DeleteArray(Html);
	DeleteArray(Script);
	DeleteArray(Charset);
	DeleteObj(Parsed);
}

char *WebPage::GetCharSet()
{
	if (!Charset)
	{
		GXmlTag *t = GetRoot();
		if (t)
		{
			List<GXmlTag>::I it = t->Children.Start();
			for (GXmlTag *c = *it; !Charset && c; c = *++it)
			{
				if (c->IsTag("meta"))
				{
					char *http_equiv = c->GetAttr("http-equiv");
					if (http_equiv && _stricmp(http_equiv, "Content-Type") == 0)
					{
						char *Value = c->GetAttr("Content");
						if (Value)
						{
							char *s = stristr(Value, "charset=");
							if (s)
							{
								s += 8;
								char *e = s;
								while (*e && (isalpha(*e) || isdigit(*e) || *e == '-' || *e == '_')) e++;
								Charset = NewStr(s, e - s);
							}
						}
					}
				}
			}
		}
	}

	return Charset;
}

GXmlTag *WebPage::GetRoot(GStream *Log)
{
	if (!Parsed && Html)
	{
		GXmlTree t(GXT_NO_DOM);
		if ((Parsed = new GXmlTag))
		{
			GStringPipe p;
			p.Push(Html);
			
			t.GetEntityTable()->Add("nbsp", ' ');
			
			if (!t.Read(Parsed, &p, 0) && Log)
			{
				Log->Print("%s:%i - Html parse failed: %s\n", _FL, t.GetErrorMsg());
				DeleteObj(Parsed);
			}
		}
	}

	return Parsed;
}

char *WebPage::GetFormValue(char *field)
{
	GXmlTag *x = GetRoot();
	if (x)
	{
		for (GXmlTag *t = x->Children.First(); t; t = x->Children.Next())
		{
			if (t->IsTag("input"))
			{
				char *Name = t->GetAttr("name");
				if (Name && _stricmp(Name, field) == 0)
				{
					return t->GetAttr("Value");
				}
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
FormPost::FormPost(GXmlTag *f)
{
	Form = f;
}

GXmlTag *FormPost::GetField(char *n)
{
	if (n && Form)
	{
		char *a = strchr(n, '*');

		for (GXmlTag *t = Form->Children.First(); t; t = Form->Children.Next())
		{
			char *Name = t->GetAttr("name");
			if (Name)
			{
				if (a)
				{
					if (MatchStr(n, Name))
						return t;
				}
				else
				{
					if (_stricmp(Name, n) == 0)
						return t;
				}
			}
		}
	}

	return 0;
}

char *FormPost::GetActionUri()
{
	return Form->GetAttr("action");
}

char *FormPost::EncodeFields(GStream *Debug, char *RealFields, bool EncodePlus)
{
	GStringPipe p;
	GHashTable Done;
	GHashTable Real;

	if (RealFields)
	{
		GToken t(RealFields, "\n");
		for (unsigned i=0; i<t.Length(); i++)
		{
			char *v = strchr(t[i], '=');
			if (v)
			{
				*v++ = 0;

				#if 0
				if (Debug && stricmp(t[i], "__VIEWSTATE") == 0)
				{
					Debug->Print("RealField '%s' is %i long\n", t[i], strlen(v));
				}
				#endif

				Real.Add(t[i], NewStr(v));
			}
		}
	}

	for (unsigned i=0; i<Values.Length(); i++)
	{
		FormValue *v = &Values[i];
		if (v->Value && v->Field)
		{
			#if 0
			if (Debug && stricmp(v->Field, "__VIEWSTATE") == 0)
			{
				Debug->Print("OurField '%s' is %i long\n", v->Field, strlen(v->Value));
			}
			#endif

			if (!Done.Find(v->Field))
			{
				Done.Add(v->Field);

				if (p.GetSize())
					p.Push("&");

				char *a;
				while ((a = strchr(v->Field, ' ')))
					*a = '+';
				while ((a = strchr(v->Value, ' ')))
					*a = '+';

				StrFormEncode(p, v->Field, false);
				p.Write((char*)"=", 1);
				StrFormEncode(p, v->Value, EncodePlus);

				if (Debug && RealFields)
				{
					char *Value = (char*) Real.Find(v->Field);
					if (Value)
					{
						if (_stricmp(Value, v->Value) != 0)
						{
							Debug->Print("\tValues for '%s' different: '%s' = '%s'\n", v->Field.Get(), Value, v->Value.Get());
						}
					}
					else
					{
						Debug->Print("\tExtra field in our list: %s = %s\n", v->Field.Get(), v->Value.Get());
					}

					Real.Delete(v->Field);
				}
			}
		}
	}

	if (Debug && RealFields)
	{
		char *k;
		for (void *p = Real.First(&k); p; p = Real.Next(&k))
		{
			Debug->Print("\tMissing field: %s = %s\n", k, p);
		}
	}

	#if 0
	// if (Form)
	{
		for (GXmlTag *t = Form->Children.First(); t; t = Form->Children.Next())
		{
			char *Type = t->GetAttr("type");
			if (Type && _stricmp(Type, "image") == 0)
				continue;

			char *Name = t->GetAttr("name");
			if (Name)
			{
				if (!Done.Find(Name))
				{
					Done.Add(Name);

					if (p.GetSize())
						p.Push("&");

					StrFormEncode(p, Name, false);
					p.Write((char*)"=", 1);
				}
			}
		}
	}
	#endif

	return p.NewStr();
}

bool Match(char *a, char *b)
{
	if (a && b)
	{
		bool Fuzzy = strchr(b, '*') != 0;
		if (Fuzzy)
		{
			return MatchStr(b, a);
		}
		else
		{
			return _stricmp(a, b) == 0;
		}
	}

	return false;
}

bool FormPost::Set(char *field, char *value, GStream *Log, bool AllowCreate)
{
	bool Status = false;

	if (field && value && Form)
	{
		GXmlTag *f = GetFormField(Form, field);
		if (f)
		{
			if (f->GetTag())
			{
				if (f->IsTag("Input"))
				{
					char *Nm = f->GetAttr("Name");
					FormValue *v = Nm ? Get(Nm) : 0;
					if (v)
					{
						v->Value.Reset(NewStr(value));
						Status = true;
					}
				}
				else if (f->IsTag("Select"))
				{
					char *Nm = f->GetAttr("Name");
					if (Nm && Match(Nm, field))
					{
						for (GXmlTag *o = f->Children.First(); o; o = f->Children.Next())
						{
							char *Value = o->GetAttr("Value");
							char *Content = o->GetContent();
							if (Value || Content)
							{
								bool Mat = !ValidStr(value);

								if (!Mat)
								{
									Mat = Match(Value, value);
								}
								if (!Mat)
								{
									Mat = Match(Content, value);
								}

								if (Mat)
								{
									FormValue *v = Nm ? Get(Nm) : 0;
									if (v)
									{
										if (ValidStr(value))
										{
											char *n = o->GetAttr("Value");
											if (n)
											{
												v->Value.Reset(NewStr(n));
												Status = true;

												// if (Log) Log->Print("'%s'='%s'\n", v->Field, v->Value);
												break;
											}
										}
										else
										{
											v->Value.Reset(NewStr(value));
											Status = true;
											break;
										}
									}
								}
							}
						}

						if (!Status && Log)
						{
							Log->Print("Error: No option for field '%s' had the value '%s'\n", field, value);
						}
					}
				}
				else if (Log) Log->Print("%s:%i - Unrecognised field '%s'\n", _FL, f->GetTag());
			}
			else if (Log) Log->Print("%s:%i - Field has no tag\n", _FL);
		}
		else
		{
			FormValue *v = Get(field, AllowCreate);
			if (v)
			{
				v->Value.Reset(NewStr(value));
				// if (Log) Log->Print("'%s'='%s'\n", v->Field, v->Value);
				Status = true;
			}
			else if (Log)
			{
				Log->Print("%s:%i - Invalid field name '%s'\n", _FL, field);
			}
		}
	}
	else
	{
		if (Log) Log->Print("%s:%i - Invalid arguments trying to set '%s' = '%s'\n", _FL, field, value);
	}

	return Status;
}

FormValue *FormPost::Get(char *Field, bool Create)
{
	char *Ast = strchr(Field, '*');

	GArray<FormValue*> Matches;
	for (unsigned i=0; i<Values.Length(); i++)
	{
		if (Ast)
		{
			if (MatchStr(Field, Values[i].Field))
				Matches.Add(&Values[i]);
		}
		else
		{
			if (_stricmp(Values[i].Field, Field) == 0)
				Matches.Add(&Values[i]);
		}
	}

	if (Matches.Length() == 1)
	{
		return Matches[0];
	}
	else if (!Ast && Matches.Length() == 0 && Create)
	{
		FormValue *v = &Values[Values.Length()];
		v->Field.Reset(NewStr(Field));
		return v;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
HttpTools::HttpTools()
{
	Wnd = 0;
}

HttpTools::~HttpTools()
{
}

void HttpTools::DumpView(GViewI *v, char *p)
{
	if (v && p)
	{
		uint8 *c = (uint8*) p;
		while (*c)
		{
			if (*c & 0x80)
			{
				*c = ' ';
			}
			c++;
		}

		v->Name(p);
	}
}

char *HttpTools::Fetch(char *uri, GStream *Log, GViewI *Dump, CookieJar *Cookies)
{
	char *Page = 0;

	const char *DefHeaders =	"User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.0.9) Gecko/20061206 Firefox/1.5.0.9\r\n"
								"Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\r\n"
								"Accept-Language: en-us,en;q=0.5\r\n"
								"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n";

	if (ValidStr(uri))
	{
		IHttp h;

		GProxyUri Proxy;
		if (Proxy.Host)
			h.SetProxy(Proxy.Host, Proxy.Port);

		GUri u(uri);
		if (!u.Port)
			u.Port = HTTP_PORT;

		GAutoPtr<GSocketI> Sock(new GSocket);
		if (h.Open(Sock, u.Host))
		{
			int ProtocolStatus = 0;
			GStringPipe p, hdr;

			GAutoString Enc = u.GetUri();
			IHttp::ContentEncoding type;
			if (h.Get(Enc, DefHeaders, &ProtocolStatus, &p, &type, &hdr))
			{
				if (Cookies)
				{
					char *Headers = hdr.NewStr();
					if (Headers)
					{
						Cookies->Set(Headers);
						DeleteArray(Headers);
					}
				}

				if (ProtocolStatus == 200)
				{
					Page = p.NewStr();
					if (Page)
					{
						char *Err = stristr(Page, "<html\">");
						if (Err)
						{
							Err[5] = ' ';
						}

						DumpView(Dump, Page);
					}
					else
					{
						Log->Print("Error: No page data for '%s'\n", uri);
					}
				}
				/*
				else if (ProtocolStatus == 302)
				{
					char *Headers = hdr.NewStr();
					if (Headers)
					{
						char *Loc = InetGetHeaderField(Headers, "Location", -1);
						if (Loc)
						{
							Log->Print("Redirects to '%s'\n", Loc);
							DeleteArray(Loc);
						}
						DeleteArray(Headers);
					}
				}
				*/
				else if (Log)
				{
					if (Dump)
					{
						char *pg = p.NewStr();
						if (pg)
							Dump->Name(pg);
						DeleteArray(pg);
					}

					Log->Print("Error: Error getting '%s' with HTTP error %i\n", Enc.Get(), ProtocolStatus);
				}
			}
			else if (Log)
			{
				Log->Print("Error: Failed to GET '%s' (errorcode: %i)\n", uri, ProtocolStatus);
			}
		}
		else if (Log)
		{
			Log->Print("Error: Couldn't open connection to '%s' [:%s]\n", u.Host, u.Port);
		}
	}
	
	return Page;
}

char *HttpTools::Post(char *uri, char *headers, char *body, GStream *Log, GViewI *Dump)
{
	if (uri && headers && body)
	{
		IHttp h;

		GUri u(uri);
		GSocket s;
		bool Open;

		GProxyUri Proxy;
		if (Proxy.Host)
		{
			Open = s.Open(Proxy.Host, Proxy.Port) != 0;
		}
		else
		{
			Open = s.Open(u.Host, u.Port ? u.Port : 80) != 0;
		}

		if (Open)
		{
			char *e = headers + strlen(headers) - 1;
			while (e > headers && strchr("\r\n", *e))
			{
				*e-- = 0;
			}

			size_t ContentLen = strlen(body);
			GStringPipe p;
			char *EncPath = u.Encode(u.Path);

			if (Proxy.Host)
			{
				p.Print("POST http://%s%s HTTP/1.1\r\n"
						"Host: %s\r\n",
						u.Host,
						EncPath,
						u.Host);
			}
			else
			{
				p.Print("POST %s HTTP/1.1\r\n"
						"Host: %s\r\n",
						EncPath,
						u.Host);
			}
			DeleteArray(EncPath);

			p.Print("Content-Length: %i\r\n"
					"%s\r\n"
					"\r\n",
					ContentLen,
					headers);

			int64 Len = p.GetSize();
			char *h = p.NewStr();
			if (h)
			{
				ssize_t w = s.Write(h, (int)Len, 0);
				if (w == Len)
				{
					w = s.Write(body, ContentLen, 0);
					if (w == ContentLen)
					{
						char Buf[1024];
						ssize_t r;
						while ((r = s.Read(Buf, sizeof(Buf), 0)) > 0)
						{
							p.Push(Buf, r);
						}
						if (Log && p.GetSize() == 0)
						{
							Log->Print("HTTP Response: Failed with %i\n", r);
						}

						if (p.GetSize())
						{
							char *Page = p.NewStr();
							if (Page)
							{
								DumpView(Dump, Page);

								return Page;
							}
						}
					}
					else
					{
						Log->Print("HTTP Request: Error, wrote %i of %i bytes\n", w, ContentLen);
					}
				}					
				else
				{
					Log->Print("HTTP Request: Error, wrote %i of %i bytes\n", w, Len);
				}

				DeleteArray(h);
			}
		}
		else if (Log)
		{
			Log->Print("Error: Failed to open socket.\n");
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////
void CookieJar::Empty()
{
	for (char *p = (char*)First(); p; p = (char*)Next())
	{
		DeleteArray(p);
	}
}

char *EndCookie(char *s)
{
	while (*s)
	{
		if (*s == '\n' || *s == '\r' || *s == ';')
			return s;
		s++;
	}

	return 0;
}

void CookieJar::Set(char *Cookie, char *Value)
{
	char *s;
	if ((s = (char*)Find(Cookie)))
	{
		DeleteArray(s);
		Delete(Cookie);
	}
	Add(Cookie, NewStr(Value));
}

void CookieJar::Set(char *Headers)
{
	Empty();

	if (Headers)
	{
		const char *Match = "Set-Cookie";
		size_t MatchLen = strlen(Match);

		char Ws[] = " \t\r\n";
		for (char *s = stristr(Headers, "\r\n"); s && *s; )
		{
			while (*s && strchr(Ws, *s)) s++;

			char *Hdr = s;
			while (*s && *s != ':' && !strchr(Ws, *s)) s++;
			if (*s == ':')
			{
				ssize_t Len = s - Hdr;
				bool IsCookie = Len == MatchLen && _strnicmp(Hdr, Match, Len) == 0;

				s++;
				while (*s && strchr(Ws, *s)) s++;
				
				if (IsCookie)
				{
					char *e;
					while ((e = EndCookie(s)))
					{
						char *Cookie = NewStr(s, e - s);
						if (Cookie)
						{
							char *Eq = strchr(Cookie, '=');
							if (Eq)
							{
								*Eq++ = 0;
								Add(Cookie, NewStr(Eq));
							}
						}

						s = e + 1;
						while (*s && strchr(" \t", *s)) s++;
						if (*s == '\r' || *s == '\n' || *s == 0)
							break;
					}
				}

				while (s && *s)
				{
					while (*s && *s != '\n')
						s++;

					if (*s)
					{
						if (*s == '\n' && s[1] && strchr(Ws, s[1])) s += 2;
						else break;
					}
				}
			}
			else break;
		}
	}
}

char *CookieJar::Get()
{
	GStringPipe p;

	char *k;
	for (char *s = (char*)First(&k); s; s = (char*)Next(&k))
	{
		if (p.GetSize()) p.Print("; ");
		p.Print("%s=%s", k, s);
	}

	return p.NewStr();
}

///////////////////////////////////////////////////////////////////////////////////
char *HtmlTidy(char *Html)
{
	GStringPipe p(256);
	int Depth = 0;
	bool LastWasEnd = false;

	for (char *s = Html; *s; )
	{
		char *c = s;
		while (*c)
		{
			if (*c == '<')
			{
				// start tag
				if (c > s)
				{
					Depth++;
					for (int i=0; i<Depth-1; i++)
						p.Write((char*)"    ", 4);
					p.Write(s, c - s);
					p.Write((char*)"\n", 1);
				}

				s = c;
				while (*c && *c != '>')
				{
					if (*c == '\'' || *c == '\"')
					{
						char delim = *c++;
						char *end = strchr(c, delim);
						if (end)
							c = end + 1;
						else
							c++;
					}
					else
					{
						c++;
					}
				}

				if (*c == '>')
					c++;

				bool IsEnd = s[1] == '/';
				if (IsEnd)
				{
					// end tag
					Depth--;
				}
				else
				{
					// start tag
					if (!LastWasEnd)
						Depth++;
				}

				for (int i=0; i<Depth-1; i++)
					p.Write((char*)"    ", 4);
				p.Write(s, c - s);
				p.Write((char*)"\n", 1);
				s = c;

				LastWasEnd = IsEnd;
			}
			else
			{
				c++;
			}
		}
	}

	return p.NewStr();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
GSurface *GetHttpImage(char *Uri)
{
	GSurface *Img = 0;

	if (Uri)
	{
		IHttp Http;

		GProxyUri p;
		if (p.Host)
			Http.SetProxy(p.Host, p.Port);

		GUri u(Uri);
		GAutoPtr<GSocketI> Sock(new GSocket);
		if (Http.Open(Sock, u.Host))
		{
			GStringPipe Data;
			int Code = 0;
			IHttp::ContentEncoding enc;
			if (Http.Get(Uri, 0, &Code, &Data, &enc))
			{
				if (Code == 200)
				{
					char n[MAX_PATH], r[32];
					do
					{
						LgiGetTempPath(n, sizeof(n));
						sprintf_s(r, sizeof(r), "_%x", LgiRand());
						LgiMakePath(n, sizeof(n), n, r);
					}
					while (FileExists(n));

					GFile f;
					if (f.Open(n, O_WRITE))
					{
						GCopyStreamer c;
						c.Copy(&Data, &f);
						f.Close();

						if ((Img = LoadDC(n)))
						{
							// LgiTrace("Read uri '%s'\n", __FILE__, __LINE__, Uri);
						}
						else LgiTrace("%s:%i - Failed to read image '%s'\n", _FL, n);

						FileDev->Delete(n, false);
					}
					else LgiTrace("%s:%i - Failed to open '%s'\n", _FL, n);
				}
				else LgiTrace("%s:%i - HTTP code %i\n", _FL, Code);
			}
			else LgiTrace("%s:%i - failed to download to '%s'\n", _FL, Uri);
		}
		else LgiTrace("%s:%i - failed to connect to '%s:%i'\n", _FL, u.Host, u.Port);
	}

	return Img;
}
