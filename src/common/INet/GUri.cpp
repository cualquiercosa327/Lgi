#include "Lgi.h"
#include "INet.h"
#include "GRegKey.h"


/////////////////////////////////////////////////////////////////////////////////
static const char *Ws = " \t\r\n";
#define SkipWs(s) while (*s && strchr(Ws, *s)) s++;

GUri::GUri(const char *uri)
{
	Protocol = 0;
	User = 0;
	Pass = 0;
	Host = 0;
	Port = 0;
	Path = 0;
	Anchor = 0;
	if (uri)
		Set(uri);
}

GUri::~GUri()
{
	Empty();
}

GUri &GUri::operator =(const GUri &u)
{
	Empty();
	Protocol = NewStr(u.Protocol);
	User = NewStr(u.User);
	Pass = NewStr(u.Pass);
	Host = NewStr(u.Host);
	Path = NewStr(u.Path);
	Anchor = NewStr(u.Anchor);
	return *this;
}

void GUri::Empty()
{
	Port = 0;
	DeleteArray(Protocol);
	DeleteArray(User);
	DeleteArray(Pass);
	DeleteArray(Host);
	DeleteArray(Path);
	DeleteArray(Anchor);
}

GAutoString GUri::GetUri()
{
	const char *Empty = "";
	GStringPipe p;
	if (Protocol)
	{
		p.Print("%s://", Protocol);
	}
	if (User || Pass)
	{
		GAutoString UserEnc = Encode(User, "@:");
		GAutoString PassEnc = Encode(Pass, "@:");
		p.Print("%s:%s@", UserEnc?UserEnc:Empty, PassEnc?PassEnc:Empty);
	}
	if (Host)
	{
		p.Write(Host, (int)strlen(Host));
	}
	if (Port)
	{
		p.Print(":%i", Port);
	}
	if (Path)
	{
		GAutoString e = Encode(Path);
		char *s = e ? e : Path;
		p.Print("%s%s", *s == '/' ? "" : "/", s);
	}
	if (Anchor)
		p.Print("#%s", Anchor);
		
	GAutoString a(p.NewStr());
	return a;
}

bool GUri::Set(const char *uri)
{
	if (!uri)
		return false;

	Empty();

	const char *s = uri;
	SkipWs(s);

	// Scan ahead and check for protocol...
	const char *p = s;
	while (*s && IsAlpha(*s)) s++;
	if (s[0] == ':' && (s - p) > 1)
	{
		Protocol = NewStr(p, s - p);
		s++;
		if (*s == '/') s++;
		if (*s == '/') s++;
	}
	else
	{
		// No protocol, so assume it's a host name or path
		s = p;
	}

	// Check for path
	bool HasPath = false;
	if
	(
		(s[0] && s[1] == ':')
		||
		(s[0] == '/')
		||
		(s[0] == '\\')
	)
	{
		HasPath = true;
	}
	else
	{
		// Scan over the host name
		p = s;
		while (	*s &&
				*s > ' ' &&
				*s < 127 &&
				*s != '/' &&
				*s != '\\')
		{
			s++;
		}

		Host = NewStr(p, s - p);
		if (Host)
		{
			char *At = strchr(Host, '@');
			if (At)
			{
				*At++ = 0;
				char *Col = strchr(Host, ':');
				if (Col)
				{
					*Col++ = 0;
					Pass = NewStr(Col);
				}
				User = NewStr(Host);

				memmove(Host, At, strlen(At) + 1);
			}

			char *Col = strchr(Host, ':');
			if (Col)
			{
				*Col++ = 0;
				Port = atoi(Col);
			}
		}

		HasPath = *s == '/';
	}

	if (HasPath)
	{
		const char *Start = s;
		while (*s && *s != '#')
			s++;

		if (*s)
		{
			Path = NewStr(Start, s - Start);
			Anchor = NewStr(s + 1);
		}
		else
		{
			Path = NewStr(Start);
		}
		
		#if 0
		// This decodes the path from %## encoding to raw characters.
		// However sometimes we need the encoded form. So instead of
		// doing the conversion here the caller has to do it now.
		char *i = Path, *o = Path;
		while (*i)
		{
			if (*i == '%' && i[1] && i[2])
			{
				char h[3] = {i[1], i[2], 0};
				*o++ = htoi(h);
				i+=2;
			}
			else
			{
				*o++ = *i;
			}
			i++;
		}
		*o = 0;
		#endif
	}

	return Host || Path;
}

GAutoString GUri::Encode(const char *s, const char *ExtraCharsToEncode)
{
	GStringPipe p(256);
	if (s)
	{
		while (*s)
		{
			if (*s == ' ' || (ExtraCharsToEncode && strchr(ExtraCharsToEncode, *s)))
			{
				char h[4];
				sprintf_s(h, sizeof(h), "%%%2.2X", (uint32)(uchar)*s++);
				p.Write(h, 3);
			}
			else
			{
				p.Write(s++, 1);
			}
		}
	}
	return GAutoString(p.NewStr());
}

GUri::StrMap GUri::Params()
{
	StrMap m;

	if (Path)
	{
		const char *q = strchr(Path, '?');
		if (q++)
		{
			auto Parts = GString(q).SplitDelimit("&");
			for (auto p : Parts)
			{
				auto Var = p.Split("=", 1);
				if (Var.Length() == 2)
					m.Add(Var[0], Var[1]);
			}
		}
	}

	return m;
}

GAutoString GUri::Decode(char *s)
{
	GStringPipe p(256);
	if (s)
	{
		while (*s)
		{
			if (s[0] == '%' && s[1] && s[2])
			{
				char h[3] = { s[1], s[2], 0 };
				char c = htoi(h);
				p.Write(&c, 1);
				s += 3;
			}
			else
			{
				p.Write(s++, 1);
			}
		}
	}

	return GAutoString(p.NewStr());
}

#ifdef MAC
int CFNumberRefToInt(CFNumberRef r, int Default = 0)
{
	int i = Default;
	if (r && 
		CFGetTypeID(r) == CFNumberGetTypeID())
	{
		CFNumberGetValue(r, kCFNumberIntType, &r);
	}
	return i;
}
#endif

GProxyUri::GProxyUri()
{
	#if defined(WIN32)

	GRegKey k(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
	if (k.IsOk())
	{
	    uint32 Enabled = 0;
	    if (k.GetInt("ProxyEnable", Enabled) && Enabled)
	    {
		    char *p = k.GetStr("ProxyServer");
		    if (p)
		    {
			    Set(p);
		    }
		}
	}

	#elif defined LINUX
	
	char *HttpProxy = getenv("http_proxy");
	if (HttpProxy)
	{
		Set(HttpProxy);
	}
	
	#elif defined MAC
	
//	CFDictionaryRef Proxies = SCDynamicStoreCopyProxies(0);
//	if (!Proxies)
//		LgiTrace("%s:%i - SCDynamicStoreCopyProxies failed.\n", _FL);
//	else
//	{
//		int enable = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPEnable));
//		if (enable)
//		{
//			#ifdef COCOA
//			LgiAssert(!"Fixme");
//			#else
//			Host = CFStringToUtf8((CFStringRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPProxy));
//			#endif
//			Port = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPPort));
//		}
//		
//		CFRelease(Proxies);
//	}

	#else

	#warning "Impl getting OS proxy here."

	#endif
}


