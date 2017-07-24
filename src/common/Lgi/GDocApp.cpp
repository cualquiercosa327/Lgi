/*hdr
**      FILE:           GDocApp.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           20/4/01
**      DESCRIPTION:    Generic Document Application Impl
**
*/

#include <stdlib.h>
#include "GDocApp.h"
#include "GToken.h"
#include "GOptionsFile.h"
#ifdef HAS_PROPERTIES
#include "GProperties.h"
#endif
#include "LgiRes.h"

//////////////////////////////////////////////////////////////////////////////////////////
class GDocAppPrivate
{
public:
	// Data
	GWindow				*App;
	GAutoString			OptionsFile;
	GAutoString			OptionsParam;
	char				*AppName;
	GString				CurFile;
	bool				Dirty;
	GDocAppInstallMode	Mode;

	GDocAppPrivate(GWindow *app, char *param)
	{
		App = app;
		OptionsParam.Reset(NewStr(param));
		AppName = 0;
		Dirty = 0;
		Mode = InstallPortable;
	}
	
	~GDocAppPrivate()
	{
		// Release memory
		DeleteArray(AppName);
	}
	
	GAutoString GetOptionsFile(const char *Ext)
	{
		// Get options file
		GAutoString Status;
		char Buf[MAX_PATH];
		if (LgiApp->GetOption("o", Buf, sizeof(Buf)))
		{
			if (FileExists(Buf))
			{
				Status.Reset(NewStr(Buf));
			}
		}

		if (!Status)
		{
			LgiGetExeFile(Buf, sizeof(Buf));
				
			char *File = strrchr(Buf, DIR_CHAR);
			if (File)
			{
				File++;
				#ifdef WIN32
				char *End = strrchr(File, '.');
				if (End)
				{
					End++;
					strcpy_s(End, Buf + sizeof(Buf) - End, Ext);
				}
				else
				#endif
				{
					// unix apps have no '.' in their name
					size_t Len = strlen(File);
					sprintf_s(File + Len, Buf + sizeof(Buf) - (File + Len), ".%s", Ext);
				}
				
				GAutoString BaseName(NewStr(File));
				
				char p[MAX_PATH];
				if (Mode == InstallPortable)
				{
					LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
				}
				else
				{
					if (LgiGetSystemPath(LSP_APP_ROOT, p, sizeof(p)) &&
						!DirExists(p))
					{
						FileDev->CreateFolder(p);
					}
				}
				LgiMakePath(p, sizeof(p), p, BaseName);
				Status.Reset(NewStr(p));
			}
		}

		return Status;
	}

	///////////////////////////////////////////////////////////////////////////
	// Xml Options
	const char *GetExtension(GOptionsFile *p)
	{
		return "xml";
	}

	bool SetOpt(GOptionsFile *p, const char *opt, char *str)
	{
		GVariant v = str;
		return p ? p->SetValue(opt, v) : false;
	}

	bool GetOpt(GOptionsFile *p, const char *opt, GVariant &v)
	{
		return p ? p->GetValue(opt, v) : false;
	}

	bool SerializeOpts(GOptionsFile *p, bool Write)
	{
		if (!p) return false;

		p->SetFile(OptionsFile);
		bool Result = p->Serialize(Write);
		if (Write && !Result)
		{
			// Probably because we don't have write access to the install folder?
			LgiMsg(	App,
					"Failed to write options to '%s' (mode=%s)",
					App->Name(),
					MB_OK,
					OptionsFile.Get(),
					Mode == InstallPortable ? "Portable" : "Desktop");
		}
		
		return Result;
	}

	////////////////////////////////////////////////////////////////////////////
	// Prop List Options
	#ifdef __PROP_H
	char *GetExtension(ObjProperties *p)
	{
		return "r";
	}

	bool SetOpt(ObjProperties *p, char *opt, char *str)
	{
		return p ? p->Set(opt, str) : false;
	}

	bool GetOpt(ObjProperties *p, char *opt, GVariant &v)
	{
		char *str;
		if (p && p->Get(opt, str))
		{
			v = str;
			return true;
		}

		return false;
	}

	bool SerializeOpts(ObjProperties *p, bool Write)
	{
		bool Status = false;
		if (p)
		{
			GFile f;
			if (f.Open(OptionsFile, Write?O_WRITE:O_READ))
			{
				if (Write)
					f.SetSize(0);

				Status = p->Serialize(f, Write);
			}
		}
		return Status;
	}
	#endif
};

//////////////////////////////////////////////////////////////////////////////////////////
template <typename OptionsFmt>
GDocApp<OptionsFmt>::GDocApp(const char *appname, const TCHAR *icon, char *optsname)
{
	Options = 0;
	_LangOptsName = 0;
	d = new GDocAppPrivate(this, optsname);
	
	GRect r(0, 0, 800, 600);
	SetPos(r);
	MoveToCenter();
	_FileMenu = 0;
	d->AppName = NewStr(appname?appname:(char*)"Lgi.GDocApp");

	char p[MAX_PATH];
	if (LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p)))
	{
		LgiMakePath(p, sizeof(p), p, "_write_test.txt");
		GFile f;
		if (!f.Open(p, O_WRITE))
		{
			d->Mode = InstallDesktop;
		}
		else
		{
			f.Close();
			FileDev->Delete(p, false);
		}
	}

	SetQuitOnClose(true);

	// Setup class
	if (icon)
	{
		#if defined WIN32
		GWin32Class *c = GWin32Class::Create(d->AppName);
		if (c)
		{
			#ifdef UNICODE
			GPointer p;
			p.w = (TCHAR*)icon;
			if (p.i < 0x10000)
			{
				c->Class.hIcon = LoadIcon(LgiProcessInst(), MAKEINTRESOURCE(icon));
			}
			else
			{
				GAutoWString wIcon(NewStrW(icon));
				c->Class.hIcon = LoadIcon(LgiProcessInst(), wIcon);
			}
			#else
			c->Class.hIcon = LoadIcon(LgiProcessInst(), ((NativeInt)icon&0xffff0000)?icon:MAKEINTRESOURCE(icon));
			#endif
		}
		#else
		if (icon)
			SetIcon(icon);
		#endif
	}
}

template <typename OptionsFmt>
GDocApp<OptionsFmt>::~GDocApp()
{
	DeleteObj(d);
	DeleteObj(Options);
}

template <typename OptionsFmt>
GDocAppInstallMode GDocApp<OptionsFmt>::GetInstallMode()
{
	return d->Mode;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::SetLanguage(char *LangId)
{	
	if (!_LangOptsName)
		return false;
	
	if (LgiMsg(	this,
				LgiLoadString(L_DOCAPP_RESTART_APP, "Changing the language requires restarting the application.\nDo you want to restart?"),
				d->AppName,
				MB_YESNO) != IDYES)
		return false;

	GVariant v;
	GetOptions()->SetValue(_LangOptsName, v = LangId);
	GetOptions()->Serialize(true);
	LgiCloseApp();

	char Exe[MAX_PATH];
	if (LgiGetExeFile(Exe, sizeof(Exe)))
	{
		LgiExecute(Exe);
	}

	return true;
}

template <typename OptionsFmt>
char *GDocApp<OptionsFmt>::GetOptionsFileName()
{
	return d->OptionsFile;
}

template <typename OptionsFmt>
void GDocApp<OptionsFmt>::_Close()
{
	Empty();
	SetCurFile(0);
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_DoSerialize(bool Write)
{
	bool Status = false;

	if (!d->OptionsFile)
	{
		const char *Ext = d->GetExtension(Options);
		d->OptionsFile = d->GetOptionsFile(Ext);
	}

	if (!Options)
	{
		if (FileExists(d->OptionsFile))
			Options = new OptionsFmt(d->OptionsFile);
		else
			Options = new OptionsFmt(GOptionsFile::PortableMode, d->OptionsParam);
	}

	if (Write)
	{
		// Save window position
		SerializeState(Options, "Pos", false);

		// save misc options
		SerializeOptions(Options, true);

		GOptionsFile *Of = dynamic_cast<GOptionsFile*>(Options);
		if (Of) Of->CreateTag("Mru");
		GMru::Serialize(Options, "Mru", true);
	}

	// do the work
	Status = _SerializeFile(Write);

	if (!Write)
	{
		GVariant Lang;
		if (_LangOptsName &&
			Options->GetValue(_LangOptsName, Lang))
		{
			GXmlTag *t = new GXmlTag("language");
			if (t)
			{
				t->SetAttr("id", Lang.Str());
				LgiApp->SetConfig(t);
			}
		}

		// read misc options
		GMru::Serialize(Options, "Mru", false);
		SerializeOptions(Options, false);

		// window pos
		SerializeState(Options, "Pos", true);
	}

	return Status;
 }

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_SerializeFile(bool Write)
{
	return d->SerializeOpts(Options, Write);
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_Create()
{
	// Load options
	_DoSerialize(false);

	// Create and setup the main application window
	#ifdef WIN32
	CreateClassW32(d->AppName);
	#endif

	if (Attach(0))
	{
		Name(d->AppName);
	
		if (X() < 1)
		{	
			GRect r(100, 100, 600, 500);
			SetPos(r);
		}
	}
	else
	{
		LgiAssert(!"Window create failed.");
		return false;
	}

	return true;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_Destroy()
{
	// Save options
	_DoSerialize(true);
	LgiApp->AppWnd = 0;
	return true;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_LoadMenu(const char *Resource, const char *Tags, int FileMenuId, int RecentMenuId)
{
	if ((Menu = new GMenu))
	{
		Menu->Attach(this);

		if (Resource)
		{
			Menu->Load(this, Resource, Tags);
			if (FileMenuId >= 0)
				_FileMenu = Menu->FindSubMenu(FileMenuId);
			else
				_FileMenu = Menu->AppendSub("&File", 0);
		}
		else
		{
			_FileMenu = Menu->AppendSub("&File");
		}

		if (_FileMenu)
		{
			if (!_FileMenu->FindItem(IDM_OPEN))
				_FileMenu->AppendItem("&Open", IDM_OPEN, true, -1, "Ctrl+O");

			if (!_FileMenu->FindItem(IDM_SAVE))
				_FileMenu->AppendItem("&Save", IDM_SAVE, true, -1, "Ctrl+S");
			
			if (!_FileMenu->FindItem(IDM_SAVEAS))
				_FileMenu->AppendItem("Save &As", IDM_SAVEAS, true);
			
			if (!_FileMenu->FindItem(IDM_CLOSE))
			{
				_FileMenu->AppendItem("Close", IDM_CLOSE, true, -1, "Ctrl+W");			
				_FileMenu->AppendSeparator();
			}

			GSubMenu *Recent = NULL;
			if (RecentMenuId >= 0)
				Recent = _FileMenu->FindSubMenu(RecentMenuId);
			else
				Recent = _FileMenu->AppendSub("Recent...");
			if (Recent)
			{
				Set(Recent);
				//_FileMenu->AppendSeparator();
				GMru::Serialize(Options, "Mru", false);
			}

			if (!_FileMenu->FindItem(IDM_EXIT))
				_FileMenu->AppendItem("&Quit", IDM_EXIT, true, -1, "Ctrl+Q");
		}
	}

	return Menu != 0;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_OpenFile(char *File, bool ReadOnly)
{
	bool Status = false;
	if (SetDirty(false))
	{
		char RealPath[256];
		if (ResolveShortcut(File, RealPath, sizeof(RealPath)))
		{
			File = RealPath;
		}

		Status = GMru::_OpenFile(File, ReadOnly);
		if (Status)
		{
			d->Dirty = false;
			SetCurFile(File);
		}
	}

	return Status;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::_SaveFile(char *File)
{
	char RealPath[256];
	if (ResolveShortcut(File, RealPath, sizeof(RealPath)))
	{
		File = RealPath;
	}
	else
	{
		strcpy_s(RealPath, sizeof(RealPath), File);
	}

	bool Status = GMru::_SaveFile(RealPath);
	if (Status)
	{
		d->Dirty = false;
		SetCurFile(RealPath);
		OnDirty(d->Dirty);
	}

	return Status;
}

template <typename OptionsFmt>
char *GDocApp<OptionsFmt>::GetAppName()
{
	return d->AppName;
}

template <typename OptionsFmt>
void GDocApp<OptionsFmt>::SetCurFile(char *f)
{
	if (!d->CurFile.Equals(f))
	{
		d->CurFile = f;
	}

	GString Display;
	if (SerializeEntry(&Display, &d->CurFile, NULL))
	{
		char s[MAX_PATH + 100];
		
		if (Display)
		{
			sprintf_s(s, sizeof(s), "%s [%s%s]", d->AppName, Display.Get(), d->Dirty ? " changed" : "");
		}
		else if (d->Dirty)
		{
			sprintf_s(s, sizeof(s), "%s [changed]", d->AppName);
		}
		else
		{
			strcpy_s(s, sizeof(s), d->AppName);
		}

		Name(s);
	}
}

template <typename OptionsFmt>
char *GDocApp<OptionsFmt>::GetCurFile()
{
	return d->CurFile;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::SetDirty(bool Dirty)
{
	if (IsAttached() && (d->Dirty ^ Dirty))
	{
		// Changing...
		if (Dirty)
		{
			// Setting dirty
			d->Dirty = true;
			SetCurFile(d->CurFile);
		}
		else
		{
			// Clearing dirty
			int Result = LgiMsg(this,
								LgiLoadString(L_DOCAPP_SAVE_CHANGE, "Do you want to save your changes?"),
								d->AppName,
								MB_YESNOCANCEL);
			if (Result == IDYES)
			{
				if (!ValidStr(d->CurFile))
				{
					GMru::OnCommand(IDM_SAVEAS);
				}
				else
				{
					_SaveFile(d->CurFile);
				}
			}
			else if (Result == IDCANCEL)
			{
				return false;
			}

			d->Dirty = false;
			SetCurFile(d->CurFile);
		}

		OnDirty(d->Dirty);
	}

	return true;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::GetDirty()
{
	return d->Dirty;
}

template <typename OptionsFmt>
OptionsFmt *GDocApp<OptionsFmt>::GetOptions()
{
	return Options;
}

template <typename OptionsFmt>
void GDocApp<OptionsFmt>::OnReceiveFiles(GArray<char*> &Files)
{
	char *f = Files.Length() ? Files[0] : 0;
	if (f && _OpenFile(f, false))
	{
		GMru::AddFile(f);
	}
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::OnRequestClose(bool OsShuttingDown)
{
	if (SetDirty(false))
	{
		return GWindow::OnRequestClose(OsShuttingDown);
	}

	return false;
}

template <typename OptionsFmt>
int GDocApp<OptionsFmt>::OnCommand(int Cmd, int Event, OsView Window)
{
	switch (Cmd)
	{
		case IDM_SAVE:
		{
			if (!GetCurFile())
			{
				GMru::OnCommand(IDM_SAVEAS);
				return 0;
			}
			else
			{
				_SaveFile(GetCurFile());
				return 0;
			}
			break;
		}
		case IDM_CLOSE:
		{
			if (SetDirty(false))
				_Close();
			break;
		}
		case IDM_EXIT:
		{
			LgiCloseApp();
			break;
		}
	}

	GMru::OnCommand(Cmd);
	return 0;
}

template <typename OptionsFmt>
GMessage::Result GDocApp<OptionsFmt>::OnEvent(GMessage *m)
{
	GMru::OnEvent(m);

	/*
	switch (MsgCode(m))
	{
		#ifdef WIN32
		case WM_CLOSE:
		{
			if (!SetDirty(false))
			{
				return 0;
			}
			break;
		}
		#endif
	}
	*/

	return GWindow::OnEvent(m);
}

template class GDocApp<GOptionsFile>;
#ifdef __PROP_H
template class GDocApp<ObjProperties>;
#endif
