#include <stdio.h>

#include "Lgi.h"
#include "GDocView.h"

#define SubtractPtr(a, b)		((a)-(b))

const char *GDocView::WhiteSpace		= " \t\r\n";
const char *GDocView::Delimiters		= "!@#$%^&*()'\":;,.<>/?[]{}-=+\\|`~";
const char *GDocView::UrlDelim			= "!~/:%+-?@&$#._=,;*()|";

GDocumentEnv::GDocumentEnv(GDocView *v)
{
	if (v)
	{
		Viewers.Add(v);
	}
}

GDocumentEnv::~GDocumentEnv()
{
	for (uint32 i=0; i<Viewers.Length(); i++)
	{
		Viewers[i]->Environment = 0;
	}
}

void GDocumentEnv::OnDone(GAutoPtr<GThreadJob> j)
{
	LoadJob *ld = dynamic_cast<LoadJob*>(j.Get());
	if (ld)
	{
		if (Lock(_FL))
		{
			if (Viewers.HasItem(ld->View))
			{
				ld->View->OnContent(ld);
				j.Release();
			}
			Unlock();
		}
	}
	else LgiAssert(!"RTTI failed.");
}

//////////////////////////////////////////////////////////////////////////////////
char16 *ConvertToCrLf(char16 *Text)
{
	if (Text)
	{
		// add '\r's
		int Lfs = 0;
		int Len = 0;
		char16 *s=Text;
		for (; *s; s++)
		{
			if (*s == '\n') Lfs++;
			Len++;
		}

		char16 *Temp = new char16[Len+Lfs+1];
		if (Temp)
		{
			char16 *d=Temp;
			s = Text;
			for (; *s; s++)
			{
				if (*s == '\n')
				{
					*d++ = 0x0d;
					*d++ = 0x0a;
				}
				else if (*s == '\r')
				{
					// ignore
				}
				else
				{
					*d++ = *s;
				}
			}
			*d++ = 0;

			DeleteObj(Text);
			return Temp;
		}
	}

	return Text;
}

//////////////////////////////////////////////////////////////////////////////////
bool LgiDetectLinks(GArray<GLinkInfo> &Links, char16 *Text, int Size)
{
	if (!Text)
		return false;

	if (Size < 0)
		Size = StrlenW(Text);

	static char16 Http[] = {'h', 't', 't', 'p', ':', '/', '/', 0 };
	static char16 Https[] = {'h', 't', 't', 'p', 's', ':', '/', '/', 0};

	for (int64 i=0; i<Size; i++)
	{
		switch (Text[i])
		{
			case 'h':
			case 'H':
			{
				if (StrnicmpW(Text+i, Http, 6) == 0 ||
					StrnicmpW(Text+i, Https, 7) == 0)
				{
					// find end
					char16 *s = Text + i;
					char16 *e = s + 6;
					for ( ; (SubtractPtr(e, Text) < Size) && 
							UrlChar(*e); e++);
					
					while
					(
						e > s &&
						!
						(
							IsAlpha(e[-1]) ||
							IsDigit(e[-1]) ||
							e[-1] == '/'
						)
					)
						e--;

					GLinkInfo &Url = Links.New();
					Url.Email = false;
					Url.Start = (int)SubtractPtr(s, Text);
					Url.Len = (int)SubtractPtr(e, s);
					i = (int)SubtractPtr(e, Text);
				}
				break;
			}
			case '@':
			{
				// find start
				char16 *s = Text + (max(i, 1) - 1);
				
				for ( ; s > Text && EmailChar(*s); s--)
					;

				if (s < Text + i)
				{
					if (!EmailChar(*s))
						s++;

					bool FoundDot = false;
					char16 *Start = Text + i + 1;
					char16 *e = Start;
					for ( ; (SubtractPtr(e, Text) < Size) && 
							EmailChar(*e); e++)
					{
						if (*e == '.') FoundDot = true;
					}
					while (e > Start && e[-1] == '.') e--;

					if (FoundDot)
					{
						GLinkInfo &Url = Links.New();
						Url.Email = true;
						Url.Start = (int)SubtractPtr(s, Text);
						Url.Len = (int)SubtractPtr(e, s);
						i = SubtractPtr(e, Text);
					}
				}
				break;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////
#include "INet.h"
GDocumentEnv::LoadType GDefaultDocumentEnv::GetContent(LoadJob *&j)
{
	if (!j || !ValidStr(j->Uri))
		return LoadError;

	GUri u(j->Uri);
	if (u.Protocol && !stricmp(u.Protocol, "file"))
	{
		char p[MAX_PATH];
		LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
		LgiMakePath(p, sizeof(p), p, u.Path);
		if (!FileExists(p))
		{
			GAutoString f(LgiFindFile(u.Path));
			if (f)
				strsafecpy(p, f, sizeof(p));
		}		
		if (FileExists(p))
		{
			if (j->Pref == GDocumentEnv::LoadJob::FmtFilename)
			{
				j->Filename.Reset(NewStr(p));
				return LoadImmediate;
			}
			else
			{
				j->pDC.Reset(LoadDC(p));
				return LoadImmediate;
			}
		}
	}

	return LoadError;
}

bool GDefaultDocumentEnv::OnNavigate(const char *Uri)
{
	if (Uri)
	{
		if
		(
			_strnicmp(Uri, "mailto:", 7) == 0
			||
			(
				strchr(Uri, '@') != 0
				&&
				strchr(Uri, '/') == 0
			)
		)
		{
			// email
			GArray<GAppInfo*> Apps;
			if (LgiGetAppsForMimeType("application/email", Apps))
			{
				GAppInfo *First = Apps[0];
				GStringPipe a;

				char *Arg = First->Params ? strstr(First->Params, "%1") : 0;
				if (Arg)
				{
					// change '%1' into the email address in question
					a.Write(First->Params, (int) (Arg-First->Params));
					a.Print("%s%s", Uri, Arg + 2);
				}
				else
				{
					// no argument place holder... just pass as a normal arg
					a.Print(" %s", Uri);
				}

				GAutoString Exe(TrimStr(First->Path, "\"\'"));
				GAutoString Args(a.NewStr());
				LgiExecute(Exe, Args, ".");
			}
			else
			{
				printf("%s:%i - Couldn't get app to handle email.\n", _FL);
			}
		}
		else
		{
			// webpage
			return LgiExecute(Uri);
		}
	}

	return false;
}

