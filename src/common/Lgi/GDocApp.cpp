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
// #include "GProperties.h"

//////////////////////////////////////////////////////////////////////////////////////////
class GDocAppPrivate
{
public:
	// Data
	GAutoString		OptionsFile;
	GAutoString		OptionsParam;
	char			*AppName;
	char			*CurFile;
	char			*Icon;
	bool			Dirty;

	GDocAppPrivate(char *param)
	{
		OptionsParam.Reset(NewStr(param));
		AppName = 0;
		CurFile = 0;
		Icon = 0;
		Dirty = 0;
	}
	
	~GDocAppPrivate()
	{
		// Release memory
		DeleteArray(AppName);
		DeleteArray(Icon);
		DeleteArray(CurFile);
	}
	
	GAutoString GetOptionsFile(char *Ext)
	{
		// Get options file
		GAutoString Status;
		char Buf[256];
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
				char *End = strchr(File, '.');
				if (End)
				{
					strcpy(End+1, Ext);
				}
				else
				#endif
				{
					// unix apps have no '.' in their name
					sprintf(File+strlen(File), ".%s", Ext);
				}

				Status.Reset(LgiFindFile(File));
				if (!Status)
				{
					// No options file yet... so create one in the
					// app's directory
					Status.Reset(NewStr(Buf));
				}
			}
		}

		return Status;
	}

	///////////////////////////////////////////////////////////////////////////
	// Xml Options
	char *GetExtension(GOptionsFile *p)
	{
		return "xml";
	}

	bool SetOpt(GOptionsFile *p, char *opt, char *str)
	{
		GVariant v = str;
		return p ? p->SetValue(opt, v) : false;
	}

	bool GetOpt(GOptionsFile *p, char *opt, GVariant &v)
	{
		return p ? p->GetValue(opt, v) : false;
	}

	bool SerializeOpts(GOptionsFile *p, bool Write)
	{
		if (!p) return false;

		p->SetFile(OptionsFile);
		return p->Serialize(Write);
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
GDocApp<OptionsFmt>::GDocApp(char *appname, char *icon, char *optsname)
{
	Options = 0;
	_LangOptsName = 0;
	d = new GDocAppPrivate(optsname);

	GRect r(0, 0, 800, 600);
	SetPos(r);
	MoveToCenter();
	_FileMenu = 0;
	d->AppName = NewStr(appname?appname:(char*)"Lgi.GDocApp");

	SetQuitOnClose(true);

	// Setup class
	if (icon)
	{
		#if defined WIN32
		GWin32Class *c = GWin32Class::Create(d->AppName);
		if (c)
		{
			#ifdef UNICODE
			GAutoWString wIcon(LgiNewUtf8To16(icon));
			GPointer p;
			p.c = icon;
			c->Class.w.hIcon = LoadIcon(LgiProcessInst(), p.i & 0xffff0000 ? wIcon : MAKEINTRESOURCE(icon));
			#else
			c->Class.a.hIcon = LoadIcon(LgiProcessInst(), ((int)icon&0xffff0000)?icon:MAKEINTRESOURCE(icon));
			#endif
		}
		#else
		d->Icon = NewStr(icon);
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
		char *Ext = d->GetExtension(Options);
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
		#if defined WIN32
		if (!IsZoomed(Handle()) AND !IsIconic(Handle()))
		#endif
		{
			GRect r = GetPos();
			d->SetOpt(Options, "Pos", r.Describe());
			printf("Writing pos %s\n", r.GetStr());
		}

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
		GRect r(100, 100, 500, 400);
		GVariant Pos;
		if (d->GetOpt(Options, "Pos", Pos) &&
			r.SetStr(Pos.Str()))
		{
			printf("Read pos %s\n", r.GetStr());
		}

		SetPos(r);
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
	
		#if defined LINUX
		if (d->Icon)
		{
			char *Path = LgiFindFile(d->Icon);
			if (Path)
			{
				// Handle()->setIcon(Path);
				DeleteArray(Path);
			}
		}
		#endif

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
bool GDocApp<OptionsFmt>::_LoadMenu(char *Resource, char *Tags)
{
	if (Menu = new GMenu)
	{
		Menu->Attach(this);

		if (Resource)
		{
			Menu->Load(this, Resource, Tags);
			_FileMenu = Menu->AppendSub("&File", 0);
		}
		else
		{
			_FileMenu = Menu->AppendSub("&File");
		}

		if (_FileMenu)
		{
			_FileMenu->AppendItem("&Open\tCtrl+O", IDM_OPEN, true);
			_FileMenu->AppendItem("&Save\tCtrl+S", IDM_SAVE, true);
			_FileMenu->AppendItem("Save &As", IDM_SAVEAS, true);
			_FileMenu->AppendItem("Close\tCtrl+W", IDM_CLOSE, true);
			_FileMenu->AppendSeparator();

			GSubMenu *Recent = _FileMenu->AppendSub("Recent...");
			if (Recent)
			{
				Set(Recent);
				_FileMenu->AppendSeparator();
				GMru::Serialize(Options, "Mru", false);
			}

			_FileMenu->AppendItem("&Quit\tCtrl+Q", IDM_EXIT, true);
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
		strsafecpy(RealPath, File, sizeof(RealPath));
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
	if (f != d->CurFile)
	{
		DeleteArray(d->CurFile);
		d->CurFile = NewStr(f);
	}

	char s[256];
	if (d->CurFile)
	{
		sprintf(s, "%s [%s%s]", d->AppName, d->CurFile, d->Dirty ? " changed" : "");
	}
	else if (d->Dirty)
	{
		sprintf(s, "%s [changed]", d->AppName);
	}
	else
	{
		strcpy(s, d->AppName);
	}

	Name(s);
}

template <typename OptionsFmt>
char *GDocApp<OptionsFmt>::GetCurFile()
{
	return d->CurFile;
}

template <typename OptionsFmt>
bool GDocApp<OptionsFmt>::SetDirty(bool Dirty)
{
	if (d->Dirty ^ Dirty)
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
					DoFileDlg(false);
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
	if (f AND _OpenFile(f, false))
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
int GDocApp<OptionsFmt>::OnEvent(GMessage *m)
{
	GMru::OnEvent(m);

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

	return GWindow::OnEvent(m);
}

template class GDocApp<GOptionsFile>;
#ifdef __PROP_H
template class GDocApp<ObjProperties>;
#endif
