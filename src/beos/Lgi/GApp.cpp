#include <Path.h>
#include "Lgi.h"
#include "GSkinEngine.h"

class GAppPrivate
{
public:
	// Common
	OsAppArguments Args;
	GXmlTag *Config;
	GFileSystem *FileSystem;
	GLibrary *SkinLib;
	GdcDevice *GdcSystem;

	GAppPrivate()
	{
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
	}

	~GAppPrivate()
	{
		DeleteObj(Config);
		DeleteObj(FileSystem);
		DeleteObj(GdcSystem);
		DeleteObj(SkinLib)
	}
};

/////////////////////////////////////////////////////////////////////////////
GApp *TheApp = 0;
class GSkinEngine *GApp::SkinEngine = 0;

GApp *GApp::ObjInstance()
{
	return TheApp;
}

GApp::GApp(const char *Mime, OsAppArguments &OsArgs, GAppArguments *AppArgs) : BApplication(Mime)
{
	// Sanity Checks
	LgiAssert(sizeof(int8) == 1);
	LgiAssert(sizeof(uint8) == 1);

	LgiAssert(sizeof(int16) == 2);
	LgiAssert(sizeof(uint16) == 2);

	LgiAssert(sizeof(int32) == 4);
	LgiAssert(sizeof(uint32) == 4);

	LgiAssert(sizeof(int64) == 8);
	LgiAssert(sizeof(uint64) == 8);
	
	LgiAssert(sizeof(char16) == 4);

	// Private data
	d = new GAppPrivate;
	TheApp = this;
	d->Args = OsArgs;

	// Setup LGI Sub-systems
	GFontSystem::Inst();
	d->FileSystem = new GFileSystem;
	d->GdcSystem = new GdcDevice;

	srand(system_time());
	LgiInitColours();
	// MouseHook = new GMouseHook;
	MouseHook = 0;
	AppWnd = 0;
	SetAppArgs(OsArgs);

	// System font
	SystemNormal = 0;
	GFontType SysFontType;
	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();
		if (SystemNormal)
		{
			SystemNormal->Transparent(true);
			
			SystemBold = new GFont;
			if (SystemBold)
			{
				*SystemBold = *SystemNormal;
				SystemBold->Bold(true);
			}
		}
	}

	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: GApp::GApp", MB_OK);
		LgiExitApp();
	}

	if (!GetOption("noskin"))
	{
		// Load library
		#if defined(_DEBUG)
		d->SkinLib = new GLibrary("liblgiskind");
		#else
		d->SkinLib = new GLibrary("liblgiskin");
		#endif
		if (d->SkinLib)
		{
			if (d->SkinLib->IsLoaded())
			{
				Proc_CreateSkinEngine CreateSkinEngine =
					(Proc_CreateSkinEngine)d->SkinLib->GetAddress(LgiSkinEntryPoint);
				if (CreateSkinEngine)
				{
					SkinEngine = CreateSkinEngine(this);
					if (!SkinEngine)
					{
						printf("%s:%i - CreateSkinEngine returned NULL.\n", __FILE__, __LINE__);
					}
				}
				else
				{
					printf("%s:%i - No CreateSkinEngine entry point.\n", __FILE__, __LINE__);
				}
			}
			else
			{
				printf("%s:%i - Can't load skin library.\n", __FILE__, __LINE__);
				DeleteObj(d->SkinLib);
			}
		}
		else
		{
			printf("%s:%i - Can't find skin library.\n", __FILE__, __LINE__);
		}
	}
}

GApp::~GApp()
{
	TheApp = 0;
	DeleteObj(d);
}

OsThreadId GApp::GetGuiThread()
{
	return 0;
}

OsProcessId GApp::GetProcessId()
{
	return getpid();
}

bool GApp::IsOk()
{
	return true;
}

GViewI *GApp::GetFocus()
{
	for (int i=0; i<CountWindows(); i++)
	{
		BWindow *w = WindowAt(i);
		if (w AND w->IsActive())
		{
			BView *f = w->CurrentFocus();
			if (f)
			{
				BViewRedir *r = dynamic_cast<BViewRedir*>(f);
				if (r)
				{
					return r->WindowHandle();
				}
			}
		}
	}
	
	return 0;
}

int32 GApp::GetMetric(LgiSystemMetric Which)
{
	return 0;
}

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (Loop)
	{
		//OnCommandLine();
		BApplication::Run();
	}
	else
	{
		// ???
	}
}

void GApp::RefsReceived(BMessage *Msg)
{
	GArray<char*> Files;
	int32 Count = 0;
	type_code Type = 0;
	
	if (Msg->GetInfo("refs", &Type, &Count) == B_OK)
	{
		for (int i=0; i<Count; i++)
		{
			entry_ref Ref;
			if (Msg->FindRef("refs", i, &Ref) == B_OK)
			{
				BPath Path;
				BEntry Entry(&Ref, true);
				Entry.GetPath(&Path);
				char *p = (char*) Path.Path();
				if (p)
				{
					Files.Add(NewStr(p));
				}
			}
		}
	}
	
	if (AppWnd AND Files.Length() > 0)
	{
		AppWnd->OnReceiveFiles(Files);
	}
	
	Files.DeleteArrays();
}

void GApp::OnReceiveFiles(GArray<char*> &Files)
{
	if (AppWnd)
	{
		AppWnd->OnReceiveFiles(Files);
	}
}

void GApp::SetConfig(GXmlTag *Tag)
{
	if (IsOk() AND Tag)
	{
		GXmlTag *Old = GetConfig(Tag->Tag);
		if (Old)
		{
			Old->RemoveTag();
			DeleteObj(Old);
		}

		if (!d->Config)
		{
			GetConfig(0);
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

GXmlTag *GApp::GetConfig(const char *Tag)
{
	if (IsOk() AND !d->Config)
	{
		char File[] = "lgi.conf";
		char Path[256];
		if (LgiGetExePath(Path, sizeof(Path)))
		{
			if (Path[strlen(Path)-1] != DIR_CHAR) strcat(Path, DIR_STR);
			strcat(Path, File);

			/*
			if (!FileExists(Path))
			{
				char *f = LgiFindFile(File);
				if (f)
				{
					strcpy(Path, f);
					DeleteArray(f);
				}
			}
			*/

			if (FileExists(Path))
			{
				d->Config = new GXmlTag("Config");
				if (d->Config)
				{
					GFile f;
					if (f.Open(Path, O_READ))
					{
						GXmlTree t;
						t.Read(d->Config, &f, 0);
					}
				}
			}
		}

		if (!d->Config)
		{
			d->Config = new GXmlTag("Options");
		}
	}

	if (Tag AND d->Config)
	{
		return d->Config->GetTag(Tag);
	}

	return 0;
}

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	if (Option)
	{
		int OptLen = strlen(Option);
		
		for (int i=1; i<d->Args.Args; i++)
		{
			char *c = d->Args.Arg[i];
			if (*c == '/' || *c == '\\' || *c == '-')
			{
				c++;
				if (strncmp(c, Option, OptLen) == 0)
				{
					c += OptLen;
					
					if (!*c AND i < d->Args.Args - 1)
					{
						c = d->Args.Arg[++i];
					}
					
					if (c)
					{
						while (*c AND strchr(" \t\r\n", *c))
						{
							c++;
						}
		
						if (Dest AND DestLen > 0)
						{
							for (char *i = c; *i AND *i != ' ' AND DestLen > 1;)
							{
								*Dest++ = *i++;
								DestLen--;
							}
							*Dest++ = 0;
						}
						
						return true;
					}
				}
			}
		}
	}

	return false;
}

void GApp::OnCommandLine()
{
	char WhiteSpace[] = " \r\n\t";

	GArray<char*> Files;
	
	char *Delim = "\'\"";
	char *s;
	/*
	for (s = CmdLine; *s; )
	{
		// skip ws
		while (strchr(WhiteSpace, *s)) s++;

		// read to end of token
		char *e = s;
		if (strchr(Delim, *s))
		{
			for (e = ++s; *e AND !strchr(Delim, *e); e++);
		}
		else
		{
			for (; *e AND !strchr(WhiteSpace, *e); e++);
		}

		char *Arg = NewStr(s, (int)e-(int)s);
		if (Arg)
		{
			if (FileExists(Arg))
			{
				Files.Insert(Arg);
			}
			else
			{
				DeleteArray(Arg);
			}
		}

		// next
		s = (*e) ? e + 1 : e;
	}
	*/

	// call app
	if (Files.Length() > 0)
	{
		OnReceiveFiles(Files);
	}

	// clear up
	Files.DeleteArrays();
}

char *GApp::GetArgumentAt(int n)
{
	return n >= 0 AND n < d->Args.Args ? NewStr(d->Args.Arg[n]) : 0;
}

OsAppArguments *GApp::GetAppArgs()
{
	return &d->Args;
}

bool GApp::GetOption(const char *Option, GAutoString &Buf)
{
	if (IsOk() AND Option)
	{
		int OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			char *a = d->Args.Arg[i];

			if (strchr("-/\\", a[0]))
			{
				if (strncmp(a+1, Option, OptLen) == 0)
				{
					char *Arg = 0;
					if (strlen(a+1+OptLen) > 0)
					{
						Arg = a + 1 + OptLen;
					}
					else if (i < d->Args.Args - 1)
					{
						Arg = d->Args.Arg[i + 1];
					}
					
					if (Arg)
					{
						if (strchr("\'\"", *Arg))
						{
							char Delim = *Arg++;
							char *End = strchr(Arg, Delim);
							if (End)
							{
								int Len = (int)End-(int)Arg;
								if (Len > 0)
								{
									Buf.Reset(NewStr(Arg, End - Arg));
								}
								else return false;
							}
							else return false;
						}
						else
						{
							Buf.Reset(NewStr(Arg));
						}
					}

					return true;
				}
			}
		}
	}

	return false;
}

void GApp::SetAppArgs(OsAppArguments &AppArgs)
{
	d->Args = AppArgs;
}

void GApp::Exit(int Code)
{
	if (Code)
	{
		// hard exit
		exit(-1);
	}
	else
	{
		// normal exit
		BMessenger m(0, be_app);
		m.SendMessage(B_QUIT_REQUESTED);
	}
}

GAutoString GApp::GetFileMimeType(const char *File)
{
	LgiAssert(0);
	return GAutoString();
}

GMouseHook *GApp::MouseHook = 0;
GMouseHook *GApp::GetMouseHook()
{
	return MouseHook;
}

