// Linux Implementation of General LGI functions
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#define _POSIX_TIMERS
#include <time.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GTextLabel.h"
#include "GButton.h"
#include "LgiWinManGlue.h"
#include "GToken.h"

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

void _lgi_sleep(int i)
{
	struct timespec request, remain;

	ZeroObj(request);
	ZeroObj(remain);
	
	request.tv_sec = i / 1000;
	request.tv_nsec = (i % 1000) * 1000000;

	//printf("%i LgiSleep(%i)\n", LgiGetCurrentThread(), i);
	while (nanosleep(&request, &remain) == -1)
	{
	    request = remain;
		//printf("\t%i Resleeping=%i\n", LgiGetCurrentThread(), request.tv_sec*1000 + request.tv_nsec/1000);
	}
}

void _lgi_assert(bool b, char *test, char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
	{
		Asserting = true;

		printf("%s:%i - Assert failed:\n%s\n", file, line, test);

		#if 0
		LgiMsg(0,
				"Assert failed, file: %s, line: %i\n"
				"%s\n",
				"Debug",
				MB_OK,
				file, line, test);
		#endif

		exit(-1);
	}
}

////////////////////////////////////////////////////////////////////////
// Implementations
GMessage CreateMsg(int m, int a, int b)
{
	static GMessage Msg(0);
	Msg.Set(m, a, b);
	return Msg;
}

bool LgiGetMimeTypeExtensions(char *Mime, GArray<char*> &Ext)
{
	int Start = Ext.Length();
	char *e;

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

bool LgiGetFileMimeType(char *File, char *Mime, int BufLen)
{
	GAutoString s = LgiApp->GetFileMimeType(File);
	if (!s || !Mime)
		return false;

	strsafecpy(Mime, s, BufLen);
	return true;
}

/*
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
					while (e > Line && strchr(" \t", *e)) e--;
					
					// Seek v through any whitespace after the equals
					while (*v && strchr(" \t", *v)) v++;
					
					// Calculate the length of the field
					int flen = (int)e-(int)Line;

					// Check the current field against the input field
					if (strnicmp(Field, Line, flen) == 0)
					{
						while (	*v &&
								strchr("\r\n", *v) == 0 &&
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

bool _GetKdePaths(GToken &t, char *Type)
{
	bool Status = false;

	char Args[256];
	sprintf(Args, "--path %s", Type);

	GStringPipe Out;
	GProcess p;
	if (p.Run("kde-config", Args, 0, true, 0, &Out))
	{
		char *Buf = Out.NewStr();
		if (Buf)
		{
			t.Parse(Buf, ":\r\n");
			DeleteArray(Buf);
			Status = true;
		}
		else
		{
			printf("_GetKdePaths: Didn't get string back from kde-config.\n");
		}
	}
	else
	{
		printf("_GetKdePaths: Couldn't run kde-config.\n");
	}

	return Status;
}

*/

bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize)
{
	bool Status = false;

	GLibrary *WmLib = LgiApp->GetWindowManagerLib();
	if (WmLib)
	{
		Proc_LgiWmGetSysFont GetSysFont = (Proc_LgiWmGetSysFont) WmLib->GetAddress("LgiWmGetSysFont");
		if (GetSysFont)
		{
			Status = GetSysFont(FontType, Font, FontBufSize, PointSize);
			if (!Status)
			{
				printf("%s:%i - GetSysFont failed\n", _FL);
			}
		}
		else
		{
			printf("%s:%i - Entry point doesn't exist\n", _FL);
		}
	}
	else
	{
		static bool Warn = true;
		if (Warn)
		{
			printf("%s:%i - GetWindowManagerLib failed\n", _FL);
			Warn = false;
		}
	}
	
	return Status;
}

bool LgiGetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	GLibrary *WmLib = LgiApp->GetWindowManagerLib();
	if (WmLib)
	{
		Proc_LgiWmMimeToApps WmMimeToApps = (Proc_LgiWmMimeToApps) WmLib->GetAddress("LgiWmMimeToApps");
		if (WmMimeToApps)
		{
			Status = WmMimeToApps(Mime, Apps, Limit);
			if (!Status)
			{
				printf("%s:%i - WmMimeToApps failed\n", _FL);
			}
		}
		else
		{
			printf("%s:%i - Entry point doesn't exist\n", _FL);
		}
	}
	else
	{
		printf("%s:%i - GetWindowManagerLib failed\n", _FL);
	}

	return Status;
}

bool LgiGetAppForMimeType(char *Mime, char *AppPath, int BufSize)
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

bool LgiPlaySound(char *FileName, int ASync)
{
	return LgiExecute(FileName);
}

bool LgiExecute(char *File, char *Args, char *Dir)
{
	if (File)
	{
		bool IsUrl = false;

		char App[400] = "";
		if (strnicmp(File, "http://", 7) == 0 ||
			strnicmp(File, "https://", 8) == 0)
		{
			IsUrl = true;
			LgiGetAppForMimeType("text/html", App, sizeof(App));
		}
		else
		{
			struct stat f;
			char Path[MAX_PATH];
			
			// see if the file is executable
			bool InPath = false;
			bool Ok = stat(File, &f) == 0;
			if (Ok)
			{
				strcpy(Path, File);
			}
			else
			{
				// look in the path
				InPath = true;
				GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
				for (int i=0; i<p.Length() && !Ok; i++)
				{
					LgiMakePath(Path, sizeof(Path), p[i], File);
					Ok = stat(Path, &f) == 0;
				}
			}
			
			if (Ok)			
			{
				if (S_ISDIR(f.st_mode))
				{
					// open the directory in the current browser
					LgiGetAppForMimeType("inode/directory", App, sizeof(App));
				}
				/*
				else if (f.st_mode & (S_IXUSR | S_IXGRP | 1) )
				{
					// execute the file...
					char f[512];
					sprintf(f, "\"%s\" %s &", File, Args ? Args : (char*)"");
					return system(f) == 0;
				}
				*/
				else // if (!InPath)
				{
					// look up the type...
					char Mime[256];
					if (LgiGetFileMimeType(File, Mime, sizeof(Mime)))
					{
						printf("LgiGetFileMimeType(%s)=%s\n", File, Mime);
						
						if (stricmp(Mime, "application/x-executable") == 0 ||
							stricmp(Mime, "application/x-shellscript") == 0)
						{
							TreatAsExe:
							char f[512];
							sprintf(f, "\"%s\" %s &", File, Args ? Args : (char*)"");
							return system(f) == 0;
						}
						else
						{
							// got the mime type
							if (!LgiGetAppForMimeType(Mime, App, sizeof(App)))
							{
								printf("%s:%i: LgiExecute - LgiGetAppForMimeType failed to return the app for '%s'\n", __FILE__, __LINE__, File);
								goto TreatAsExe;
							}
						}
					}
					else
					{
						// printf("LgiExecute: LgiGetFileMimeType failed to return a mime type for '%s'\n", File);
						
						// If a file can't be typed it's most likely an executable.
						goto TreatAsExe;
					}
				}
			}
			else
			{
				printf("LgiExecute: '%s' doesn't exist.\n", File);
			}
		}

		if (App[0])
		{
			bool FileAdded = false;
			char EscFile[512], *o = EscFile;
			for (char i=0; File[i]; i++)
			{
				if (File[i] == ' ' || File[i] == '&')
				{
					*o++ = '\\';
				}
				
				*o++ = File[i];
			}
			*o++ = 0;

			for (char *s=App; *s; )
			{
				if (*s == '%')
				{
					switch (s[1])
					{
						case 'f':
						case 'F':
						{
							int Len = strlen(EscFile);
							memmove(s + 2, s + Len - 2, strlen(s + 2) + 1);
							memcpy(s, EscFile, Len);
							FileAdded = true;
							break;
						}
						case 'u':
						case 'U':
						{
							char Url[256];
							sprintf(Url, IsUrl ? "%s" : "file:%s", EscFile);
							int Len = strlen(Url);

							memmove(s + 2, s + Len - 2, strlen(s + 2) + 1);
							memcpy(s, Url, Len);
							FileAdded = true;
							break;
						}
						default:
						{
							// we don't understand this command
							memmove(s, s + 2, strlen(s + 2) + 1);
							break;
						}
					}
				}
				else s++;
			}

			if (!FileAdded)
			{
				strcat(App, " ");
				strcat(App, EscFile);
			}

			strcat(App, " > /dev/null 2> /dev/null &");

			printf("LgiExecute: document, system('%s')\n", App);
			int e;
			if (Dir) chdir(Dir);
			if (e = system(App))
			{
				printf("\tsystem(...) failed with error: %i\n", e);
				return false;
			}

			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
WindowManager LgiGetWindowManager()
{
	static WindowManager Status = WM_Unknown;

	if (Status == WM_Unknown)
	{
		GDirectory d;

		for (bool b=d.First("/proc"); b && Status == WM_Unknown; b=d.Next())
		{
			if (d.IsDir() && isdigit(d.GetName()[0]))
			{
				char Path[256];
				d.Path(Path, sizeof(Path));
				LgiMakePath(Path, sizeof(Path), Path, "status");
				
				GFile s;
				if (s.Open(Path, O_READ))
				{
					char Buf[256];
					Buf[sizeof(Buf)-1] = 0;
					s.Read(Buf, sizeof(Buf)-1);
					
					char *n = strchr(Buf, '\n');
					if (n)
					{
						*n = 0;
						
						// printf("Buf=%s\n", Buf);
						
						if (stristr(Buf, "gnome-settings") != 0 ||
							stristr(Buf, "gnome-session") != 0 ||
							stristr(Buf, "gnome-panel") != 0)
						{
							Status = WM_Gnome;
						}
						else if (stristr(Buf, "startkde") != 0 ||
								 stristr(Buf, "kdesktop") != 0)
						{
							Status = WM_Kde;
						}
					}
				}
				else printf("%s:%i - error\n", __FILE__, __LINE__);
			}
		}	
	}
	
	return Status;
}

void LgiFinishXWindowsStartup(GViewI *Wnd)
{
	// Get the startup ID
	const char *EnvStartId = "DESKTOP_STARTUP_ID";
	char *DesktopStartupId = getenv(EnvStartId);
	if (ValidStr(DesktopStartupId))
	{
		GStringPipe oss;

		// Create remove string
		oss.Push("remove: ID=");

		// Quote the id string
		for (char *c = DesktopStartupId; *c; c++)
		{
			if (*c == ' ' || *c == '"' || *c == '\\')
			{
				oss.Write((char*)"\\", 1);
			}
			oss.Write(c, 1);
		}

		char *Str = oss.NewStr();

		// Get the window and display
		/*
		XWidget *View = Wnd->Handle();
		if (!View)
			return;
			
		Display *display = View->XDisplay();
		Window xroot_window = DefaultRootWindow(display);

		XSetWindowAttributes attrs;
		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

		// Get the atoms
		Atom type_atom = XInternAtom(display, "_NET_STARTUP_INFO", false);
		Atom type_atom_begin = XInternAtom(display, "_NET_STARTUP_INFO_BEGIN", false);

		// Create the event we will send
		XEvent xevent;
		xevent.xclient.type = ClientMessage;
		xevent.xclient.message_type = type_atom_begin;
		xevent.xclient.display = display;
		xevent.xclient.window = View->handle();
		xevent.xclient.format = 8;

		const char* src = Str;
		const char* src_end = src + strlen(Str) + 1; // Include trailing NUL.

		// Loop over the string and send it.
		while (src != src_end)
		{
			char* dest = &xevent.xclient.data.b[0];
			char* dest_end = dest + 20;        
			while (dest != dest_end && src != src_end)
			{
				*dest++ = *src++;
			}
			while (dest != dest_end)
			{
				*dest++ = 0;
			}
			printf("%s:%i - XSendEvent\n", __FILE__, __LINE__);
			XSendEvent(display, xroot_window, False, PropertyChangeMask, &xevent);
			xevent.xclient.message_type = type_atom;
		}

		// Clear the event ID so it's not inherited by child processes.
		unsetenv(EnvStartId);
		*/
	}
}

