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
			GDocView *View = NULL;
			for (unsigned i=0; i<Viewers.Length(); i++)
			{
				if (Viewers[i]->GetDocumentUid() == ld->UserUid)
				{
					View = Viewers[i];
					break;
				}
			}
			
			if (View)
			{
				View->OnContent(ld);
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
#include "INet.h"
GDocumentEnv::LoadType GDefaultDocumentEnv::GetContent(LoadJob *&j)
{
	if (!j || !ValidStr(j->Uri))
		return LoadError;

	char p[MAX_PATH];
	char *FullPath = NULL;
	char *FileName = NULL;
	GUri u(j->Uri);
	if (u.Protocol && !_stricmp(u.Protocol, "file"))
		FileName = u.Path;
	else
		FileName = j->Uri;

	if (FileName)
	{
		LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
		LgiMakePath(p, sizeof(p), p, FileName);
		if (FileExists(p))
		{
			FullPath = p;
		}
		else
		{
			GAutoString f(LgiFindFile(FileName));
			if (f)
				strcpy_s(FullPath = p, sizeof(p), f);
		}
	}			

	if (FileExists(FullPath))
	{
		char Mt[256] = "";
		LgiGetFileMimeType(FullPath, Mt, sizeof(Mt));
		
		if (stristr(Mt, "image/"))
		{
			j->pDC.Reset(LoadDC(p));
			return LoadImmediate;
		}
		
		j->Filename.Reset(NewStr(FullPath));
		return LoadImmediate;
	}

	return LoadError;
}

bool GDefaultDocumentEnv::OnNavigate(GDocView *Parent, const char *Uri)
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

				GAutoString ErrorMsg;
				if (LgiExecute(Exe, Args, ".", &ErrorMsg))
					return true;

				LgiMsg(Parent, "Failed to open '%s':\n%s", LgiApp->Name(), MB_OK, Exe.Get(), ErrorMsg.Get());
			}
			else
			{
				LgiMsg(Parent, "Couldn't get app to handle email.", LgiApp->Name(), MB_OK);
			}
		}
		else
		{
			// webpage
			GAutoString ErrorMsg;
			if (LgiExecute(Uri, NULL, NULL, &ErrorMsg))
				return true;

			LgiMsg(Parent, "Failed to open '%s':\n%s", LgiApp->Name(), MB_OK, Uri, ErrorMsg.Get());
		}
	}

	return false;
}

