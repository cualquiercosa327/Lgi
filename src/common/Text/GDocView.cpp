#include <stdio.h>

#include "Lgi.h"
#include "GDocView.h"

char *GDocView::WhiteSpace		= " \t\r\n";
char *GDocView::Delimiters		= "!@#$%^&*()'\":;,.<>/?[]{}-=+\\|`~";
char *GDocView::UrlDelim		= "!~/:%+-?@&$#._=,;*()|";

GDocumentEnv::~GDocumentEnv()
{
	if (Viewer)
	{
		Viewer->Environment = 0;
	}
}

bool GDocView::AlphaOrDigit(char c)
{
	return IsDigit(c) OR IsLetter(c);
}

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
bool GDefaultDocumentEnv::GetImageUri(char *Uri, GSurface **pDC, char *FileName, int FileBufSize)
{
	if (ValidStr(Uri))
	{
		char Exe[256];
		LgiGetExePath(Exe, sizeof(Exe));

		#ifdef WIN32
		if (stristr(Exe, "\\Debug") OR
			stristr(Exe, "\\Release"))
		{
			LgiTrimDir(Exe);
		}
		#endif

		char File[256];
		LgiMakePath(File, sizeof(File), Exe, Uri);

		if (!FileExists(File))
		{
			char *f = LgiFindFile(Uri);
			if (f)
			{
				strsafecpy(File, f, sizeof(File));
				DeleteArray(f);
			}
		}
		
		if (FileExists(File))
		{
			if (pDC)
			{
				*pDC = LoadDC(File);
			}
			else if (FileName)
			{
				strsafecpy(FileName, File, FileBufSize);
			}

			return true;
		}

	}
	return false;
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

