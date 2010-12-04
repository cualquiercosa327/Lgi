#include <stdio.h>

#include "Lgi.h"
#include "GDocView.h"

#define SubtractPtr(a, b)		((a)-(b))

char *GDocView::WhiteSpace		= " \t\r\n";
char *GDocView::Delimiters		= "!@#$%^&*()'\":;,.<>/?[]{}-=+\\|`~";
char *GDocView::UrlDelim		= "!~/:%+-?@&$#._=,;*()|";

GDocumentEnv::GDocumentEnv(GDocView *v)
{
	if (v)
	{
		Viewers.Add(v);
	}
}

GDocumentEnv::~GDocumentEnv()
{
	for (int i=0; i<Viewers.Length(); i++)
	{
		Viewers[i]->Environment = 0;
	}
}

void GDocumentEnv::OnDone(GThreadJob *j)
{
	LoadJob *ld = dynamic_cast<LoadJob*>(j);
	if (ld && Lock(_FL))
	{
		if (Viewers.HasItem(ld->View))
		{
			ld->View->OnContent(ld);
			j = ld = 0;
		}
		Unlock();
	}
	DeleteObj(j);
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

	for (int i=0; i<Size; i++)
	{
		switch (Text[i])
		{
			case 'h':
			case 'H':
			{
				if (StrnicmpW(Text+i, Http, 6) == 0 OR
					StrnicmpW(Text+i, Https, 7) == 0)
				{
					// find end
					char16 *s = Text + i;
					char16 *e = s + 6;
					for ( ; (SubtractPtr(e, Text) < Size) AND 
							UrlChar(*e); e++);
					
					while
					(
						e > s AND
						!
						(
							isalpha(e[-1]) OR
							isdigit(e[-1]) OR
							e[-1] == '/'
						)
					)
						e--;

					GLinkInfo &Url = Links.New();
					Url.Email = false;
					Url.Start = SubtractPtr(s, Text);
					Url.Len = SubtractPtr(e, s);
					i = SubtractPtr(e, Text);
				}
				break;
			}
			case '@':
			{
				// find start
				char16 *s = Text + (max(i, 1) - 1);
				
				for ( ; s > Text AND EmailChar(*s); s--)
					;

				if (s < Text + i)
				{
					if (!EmailChar(*s))
						s++;

					bool FoundDot = false;
					char16 *Start = Text + i + 1;
					char16 *e = Start;
					for ( ; (SubtractPtr(e, Text) < Size) AND 
							EmailChar(*e); e++)
					{
						if (*e == '.') FoundDot = true;
					}
					while (e > Start AND e[-1] == '.') e--;

					if (FoundDot)
					{
						GLinkInfo &Url = Links.New();
						Url.Email = true;
						Url.Start = SubtractPtr(s, Text);
						Url.Len = SubtractPtr(e, s);
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
GDocumentEnv::LoadType GDefaultDocumentEnv::GetContent(LoadJob *&j)
{
	if (!j || !ValidStr(j->Uri))
		return LoadError;

	char Exe[256];
	LgiGetExePath(Exe, sizeof(Exe));

	#ifdef WIN32
	if (stristr(Exe, "\\Debug") OR
		stristr(Exe, "\\Release"))
	{
		LgiTrimDir(Exe);
	}
	#endif

	char File[MAX_PATH];
	LgiMakePath(File, sizeof(File), Exe, j->Uri);

	if (!FileExists(File))
	{
		GAutoString f(LgiFindFile(j->Uri));
		if (f)
			strsafecpy(File, f, sizeof(File));
	}
	
	if (FileExists(File))
	{
		if (j->Pref == GDocumentEnv::LoadJob::FmtFilename)
		{
			j->Filename.Reset(NewStr(File));
			return LoadImmediate;
		}
		else
		{
			j->pDC.Reset(LoadDC(File));
			return LoadImmediate;
		}
	}

	return LoadError;
}

bool GDefaultDocumentEnv::OnNavigate(char *Uri)
{
	if (Uri)
	{
		if
		(
			strnicmp(Uri, "mailto:", 7) == 0
			OR
			(
				strchr(Uri, '@') != 0
				AND
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
					a.Write(First->Params, Arg-First->Params);
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

