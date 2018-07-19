// Linux Implementation of General LGI functions
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#define _POSIX_TIMERS
#include <time.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GTextLabel.h"
#include "GButton.h"
#ifndef LGI_SDL
#include "LgiWinManGlue.h"
#endif
#include "GToken.h"

#include <errno.h>
#include <pwd.h>

#define DEBUG_GET_APPS_FOR_MIMETYPE			0

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

GString LgiCurrentUserName()
{
	struct passwd *pw = getpwuid(geteuid());
	if (pw)
		return pw->pw_name;

  	return "";
}


void LgiSleep(uint32 i)
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

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
	{
		Asserting = true;

		printf("%s:%i - Assert failed:\n%s\n", file, line, test);

		#ifdef LGI_SDL
		exit(-1);
		#else
		Gtk::gint Result = Gtk::GTK_RESPONSE_NO;
		#if 1
		Gtk::GtkWidget *dialog = 
			Gtk::gtk_message_dialog_new 
			(
				NULL,
				Gtk::GTK_DIALOG_DESTROY_WITH_PARENT,
				Gtk::GTK_MESSAGE_ERROR,
				Gtk::GTK_BUTTONS_NONE,
				"%s:%i - Assert failed:\n%s",
				file, line,
				test
			);
		Gtk::GtkDialog *dlg = GtkCast(dialog, gtk_dialog, GtkDialog);		
		Gtk::gtk_dialog_add_buttons(dlg, "Break", 1, "Quit", 2, "Ignore", 3, NULL);
		Result = Gtk::gtk_dialog_run(dlg);
		Gtk::gtk_widget_destroy(dialog);
		#endif

		switch (Result)
		{
			case 1:
			{
				int *i = NULL;
				*i = 0;
				break;
			}
			case 2:
			{
				exit(-1);
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
	static GMessage Msg(0);
	Msg.Set(m, a, b);
	return Msg;
}

bool LgiGetMimeTypeExtensions(const char *Mime, GArray<char*> &Ext)
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

GString LgiGetFileMimeType(const char *File)
{
	GAutoString s = LgiApp->GetFileMimeType(File);
	return GString(s.Get());
}

bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize)
{
	bool Status = false;

	#ifndef LGI_SDL
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
	#endif
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

bool LgiGetAppsForMimeType(const char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	char Args[MAX_PATH];
	sprintf(Args, "query default %s", Mime);
	GProcess p;
	GStringPipe Output;

	GLanguage *CurLang = LgiGetLanguageId();	
	char LangName[64];
	sprintf_s(LangName, sizeof(LangName), "Name[%s]", CurLang ? CurLang->Id : "en");

	#if DEBUG_GET_APPS_FOR_MIMETYPE
	printf("LgiGetAppsForMimeType('%s', ..., %i)\nRunning 'xdg-mime %s'\n",
		Mime, Limit, Args);
	#endif

	if (p.Run("xdg-mime", Args, 0, true, 0, &Output))
	{
		GAutoString o(Output.NewStr());

		#if DEBUG_GET_APPS_FOR_MIMETYPE
		printf("Output:\n%s\n", o.Get());
		#endif
		if (o)
		{
			char *e = o + strlen(o);
			while (e > o && strchr(" \t\r\n", e[-1]))
				*(--e) = 0;
			
			char p[MAX_PATH];
			if (LgiMakePath(p, sizeof(p), "/usr/share/applications", o))
			{
				if (FileExists(p))
				{
					GAutoString txt(ReadTextFile(p));
					GAutoString Section;

					#if DEBUG_GET_APPS_FOR_MIMETYPE
					printf("Reading '%s', got %i bytes\n", p, strlen(txt));
					#endif

					if (txt)
					{
						GAppInfo *ai = new GAppInfo;
						Apps.Add(ai);
						
						GToken t(txt, "\n");
						for (int i=0; i<t.Length(); i++)
						{
							char *Var = t[i];

							#if DEBUG_GET_APPS_FOR_MIMETYPE
							printf("    '%s'\n", Var);
							#endif
							
							if (*Var == '[')
							{
								Var++;
								char *End = strchr(Var, ']');
								if (End)
								{
									Section.Reset(NewStr(Var, End - Var));
								}							
							}
							else if (!Section || !stricmp(Section, "Desktop Entry"))
							{
								char *Value = strchr(Var, '=');
								if (Value)
								{
									*Value++ = 0;
									if (!stricmp(Var, "Exec"))
									{
										GAutoString exe(TrimStr(Value));
										char *sp = strchr(exe, ' ');
										if (sp)
											ai->Path.Reset(NewStr(exe, sp-exe));
										else
											ai->Path = exe;
										
										Status = true;
									}
									else if (!stricmp(Var, "Name") ||
											!stricmp(Var, LangName))
									{
										ai->Name.Reset(TrimStr(Value));
									}
								}
							}
						}

						#if DEBUG_GET_APPS_FOR_MIMETYPE
						printf("    ai='%s' '%s'\n", ai->Name.Get(), ai->Path.Get());
						#endif
					}
					else LgiTrace("%s:%i - Can't read from '%s'\n", _FL, p);
				}
				else LgiTrace("%s:%i - '%s' doesn't exist.", _FL, p);
			}
			else LgiTrace("%s:%i - Failed to create path.\n", _FL);
		}
		else LgiTrace("%s:%i - No output from xdg-mime\n", _FL);
	}
	else LgiTrace("%s:%i - Failed to execute xdg-mime\n", _FL);

	return Status;
}

GString LgiGetAppForMimeType(const char *Mime)
{
	GString App;
	GArray<GAppInfo*> Apps;
	if (LgiGetAppsForMimeType(Mime, Apps, 1))
		App = Apps[0]->Path.Get();
	Apps.DeleteObjects();
	return App;
}

int LgiRand(int Limit)
{
	return rand() % Limit;
}

GAutoString LgiErrorCodeToString(uint32 ErrorCode)
{
	GAutoString e;
	char *s = strerror(ErrorCode);
	if (s)
	{
		e.Reset(NewStr(s));
	}
	else
	{
		char buf[256];
		sprintf_s(buf, sizeof(buf), "UnknownError(%i)", ErrorCode);
		e.Reset(NewStr(buf));
	}
	
	return e;
}

bool LgiExecute(const char *File, const char *Args, const char *Dir, GAutoString *Error)
{
	if (File)
	{
		bool IsUrl = false;

		char App[MAX_PATH] = "";
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
				strcpy_s(Path, sizeof(Path), File);
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
				else
				{
					// look up the type...
					char Mime[256] = "";
					if (LgiGetFileMimeType(File, Mime, sizeof(Mime)))
					{
						// printf("LgiGetFileMimeType(%s)=%s\n", File, Mime);
						
						if (stricmp(Mime, "application/x-executable") == 0 ||
							stricmp(Mime, "application/x-shellscript") == 0)
						{
							TreatAsExe:
							char f[512];
							sprintf_s(f, sizeof(f), "\"%s\" %s &", File, Args ? Args : (char*)"");
							return system(f) == 0;
						}
						else
						{
							// got the mime type
							if (!LgiGetAppForMimeType(Mime, App, sizeof(App)))
							{
								// printf("%s:%i: LgiExecute - LgiGetAppForMimeType failed to return the app for '%s'\n", _FL, File);
								goto TreatAsExe;
							}
						}
					}
					else
					{
						// printf("LgiExecute: LgiGetFileMimeType failed to return a mime type for '%s'\n", File);
						
						// If a file can't be typed it's most likely an executable.
						// goto TreatAsExe;
					}
				}
			}
			else if (Error)
			{
				char m[MAX_PATH];
				sprintf_s(m, sizeof(m), "'%s' doesn't exist.\n", File);
				Error->Reset(NewStr(m));
			}
		}

		if (App[0])
		{
			bool FileAdded = false;
			GString AppPath;
			GString EscFile = GString::Escape(File, -1, " &");

			GString a = App;
			int Pos;
			while ((Pos = a.Find("%")) >= 0)
			{
				char *s = a.Get() + Pos;
				printf("a='%s'\n", a.Get());
				switch (s[1])
				{			
					case 'f':
					case 'F':
					{
						a = a(0,Pos) + EscFile + a(Pos+2,-1);
						FileAdded = true;
						break;
					}
					case 'u':
					case 'U':
					{
						if (IsUrl)
							a = a(0,Pos) + EscFile + a(Pos+2,-1);
						else
							a = a(0,Pos) + GString("file:") + EscFile + a(Pos+2,-1);
						FileAdded = true;
						break;
					}
					default:
					{
						// we don't understand this command
						a = a(0,Pos) + a(Pos+2,-1);
						break;
					}
				}
				printf("a='%s'\n", a.Get());
			}

			if (!FileAdded)
			{
				a += " ";
				a += EscFile;
			}

			a += " > /dev/null 2> /dev/null &";

			int e;
			if (Dir) chdir(Dir);
			if (e = system(a))
			{
				if (Error)
					*Error = LgiErrorCodeToString(errno);
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

#if HAS_GSTREAMER
// sudo apt-get install libgstreamer1.0-dev
using namespace Gtk;
#include <gst/gst.h>
#endif

bool LgiPlaySound(const char *FileName, int ASync)
{
	#if HAS_GSTREAMER
	
	#else
	return LgiExecute(FileName);
	#endif
}
