// Mac Implementation of General LGI functions
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

// #define _POSIX_TIMERS
#include <time.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GTextLabel.h"
#include "GButton.h"
#include "INet.h"

////////////////////////////////////////////////////////////////
// Local helper functions
bool _lgi_check_file(char *Path)
{
	if (Path)
	{
		if (FileExists(Path))
		{
			// file is there
			return true;
		}
		else
		{
			// shortcut?
			char *e = Path + strlen(Path);
			strcpy(e, ".lnk");
			if (FileExists(Path))
			{
				// resolve shortcut
				char Link[256];
				if (ResolveShortcut(Path, Link, sizeof(Link)))
				{
					// check destination of link
					if (FileExists(Link))
					{
						strcpy(Path, Link);
						return true;
					}
				}
			}
			*e = 0;
		}
	}

	return false;
}

void LgiSleep(int i)
{
	struct timespec request, remain;

	ZeroObj(request);
	ZeroObj(remain);
	
	request.tv_sec = i / 1000;
	request.tv_nsec = (i % 1000) * 1000000;

	while (nanosleep(&request, &remain) == -1)
	{
	    request = remain;
	}
}

char *p2c(unsigned char *s)
{
	if (s)
	{
		s[1+s[0]] = 0;
		return (char*)s + 1;
	}
	return 0;
}

struct GLgiAssert
{
	GAutoString Msg;
	int Result;
	
	GLgiAssert() { Result = 0; }
};

static void *_LgiAssert(void *Param)
{
	GLgiAssert *Assert = (GLgiAssert*) Param;
	GAlert a(0, "Assert Failed", Assert->Msg, "Abort", "Debug", "Ignore");
	Assert->Result = a.DoModal();
	return Assert;
}

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
	{
		Asserting = true;
		printf("%s:%i - Assert failed:\n%s\n", file, line, test);

		#ifdef _DEBUG

		GStringPipe p;
		p.Print("Assert failed, file: %s, line: %i\n%s", file, line, test);
		GLgiAssert Assert;
		Assert.Msg.Reset(p.NewStr());

		#ifdef MAC
		MPRemoteCall(_LgiAssert, &Assert, kMPOwningProcessRemoteContext);
		#else
		GAlert a(0, "Assert Failed", Assert.Msg, "Abort", "Debug", "Ignore");
		Assert.Result = a.DoModal();
		#endif
		
		switch (Assert.Result)
		{
			default:
			{
				exit(-1);
				break;
			}
			case 2:
			{
				// Crash here to bring up the debugger...
				int *p = 0;
				*p = 0;
				break;
			}
			case 3:
			{
				break;
			}
		}

		#endif

		Asserting = false;
	}
}

////////////////////////////////////////////////////////////////////////
// Implementations
GMessage CreateMsg(int m, int a, int b)
{
	static GMessage Msg;
	Msg.m = m;
	Msg.a = a;
	Msg.b = b;
	return Msg;
}

OsView DefaultOsView(GView *v)
{
	return 0;
}

bool LgiGetFileMimeType(const char *File, char *Mime, int BufLen)
{
	GAutoString m = LgiApp->GetFileMimeType(File);
	if (!m)
		return false;

	strsafecpy(Mime, m, BufLen);
	return true;
}

#include "GToken.h"
bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize)
{
	if (ValidStr(In))
	{
		bool InGroup = false;
		GToken t(In, "\r\n");
		
		for (int i=0; i<t.Length(); i++)
		{
			char *Line = t[i];
			if (*Line == '[')
			{
				// Reset inside group at the start of each group
				InGroup = false;

				char *e = strchr(++Line, ']');
				if (e)
				{
					*e = 0;
					InGroup = stricmp(Grp, Line) == 0;
				}
			}
			else if (InGroup)
			{
				char *e = strchr(Line, '=');
				if (e)
				{
					// Point v to the data after the equals
					// Leave e pointing to the byte before
					char *v = e-- + 1;

					// Seek e through any whitespace between the equals and the
					// field name
					while (e > Line AND strchr(" \t", *e)) e--;
					
					// Seek v through any whitespace after the equals
					while (*v AND strchr(" \t", *v)) v++;
					
					// Calculate the length of the field
					int flen = (int)e-(int)Line;

					// Check the current field against the input field
					if (strnicmp(Field, Line, flen) == 0)
					{
						while (	*v AND
								strchr("\r\n", *v) == 0 AND
								OutSize > 1) // leave space for NULL
						{
							*Out++ = *v++;
							OutSize--;
						}

						// Add the NULL
						*Out++ = 0;
						
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

bool LgiGetAppsForMimeType(const char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	if (Mime)
	{
	}

	return Status;
}

bool LgiGetAppForMimeType(const char *Mime, char *AppPath, int BufSize)
{
	bool Status = false;
	if (AppPath)
	{
		GArray<GAppInfo*> Apps;
		Status = LgiGetAppsForMimeType(Mime, Apps, 1);
		if (Status)
		{
			strsafecpy(AppPath, Apps[0]->Path, BufSize);
		}
	}
	return Status;
}

int LgiRand(int Limit)
{
	return rand() % Limit;
}

bool LgiPlaySound(const char *FileName, int ASync)
{
	return LgiExecute(FileName);
}

bool LgiExecute(const char *File, const char *Args, const char *Dir)
{
	bool Status = false;
	
	if (File)
	{
		GUri uri(File);
		
		if (uri.Protocol)
		{
		}
		else
		{
		}
	}
	
	return Status;
}

bool LgiGetMimeTypeExtensions(const char *Mime, GArray<char*> &Ext)
{
	int Start = Ext.Length();

	#define HardCodeExtention(Mime, Ext1, Ext2) \
		else if (!stricmp(Mime, Mime)) \
		{	if (Ext1) Ext.Add(NewStr(Ext1)); \
			if (Ext2) Ext.Add(NewStr(Ext2)); }

	if (!Mime);
	HardCodeExtention("text/calendar", "ics", 0)
	HardCodeExtention("text/x-vcard", "vcf", 0)
	HardCodeExtention("text/mbox", "mbx", "mbox");

	return Ext.Length() > Start;
}
