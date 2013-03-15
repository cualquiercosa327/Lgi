#include <stdio.h>

#include "Lgi.h"
#include "GMru.h"
#include "GVariant.h"

////////////////////////////////////////////////////////////////////
#define M_MRU_BASE				(M_USER+0x3500)

struct GMruEntry
{
	GAutoString Display;
	GAutoString Raw;
	
	GMruEntry &operator =(const GMruEntry &e)
	{
		Display.Reset(NewStr(e.Display));
		Raw.Reset(NewStr(e.Raw));
		return *this;
	}
};

class GMruPrivate
{
public:
	int Size;
	GArray<GMruEntry*> Items;
	GSubMenu *Parent;
	GFileType *SelectedType;
	GFileSelect Select;

	GMruPrivate()	
	{
		Parent = 0;
		Size = 10;
		SelectedType = 0;
	}

	~GMruPrivate()	
	{
		Items.DeleteObjects();
	}
};	

////////////////////////////////////////////////////////////////////
GMru::GMru()
{
	d = new GMruPrivate;
}

GMru::~GMru()
{
	DeleteObj(d);
}

bool GMru::SerializeEntry
(
	/// The displayable version of the reference (this should have any passwords blanked out)
	GAutoString *Display,
	/// The form passed to the client software to open/save. (passwords NOT blanked)
	GAutoString *Raw,
	/// The form safe to write to disk, if a password is present it must be encrypted.
	GAutoString *Stored
)
{
	if (Raw && Raw->Get())
	{
		if (Stored)
			Stored->Reset(NewStr(*Raw));
		if (Display)
			Display->Reset(NewStr(*Raw));
	}
	else if (Stored && Stored->Get())
	{
		if (Display)
			Display->Reset(NewStr(*Stored));
		if (Raw)
			Raw->Reset(NewStr(*Stored));
	}
	
	return true;
}

void GMru::GetFileTypes(GFileSelect *Dlg, bool Write)
{
	Dlg->Type("All Files", LGI_ALL_FILES);
}

char *GMru::_GetCurFile()
{
	if (d->Items.Length())
		return d->Items[0]->Raw;
	return NULL;
}

GFileType *GMru::GetSelectedType()
{
	return d->SelectedType;
}

bool GMru::_OpenFile(char *File, bool ReadOnly)
{
	bool Status = OpenFile(File, ReadOnly);

	if (Status)
	{
		AddFile(File, true);
	}
	else
	{
		RemoveFile(File);
	}

	return Status;
}

bool GMru::_SaveFile(char *FileName)
{
	bool Status = false;

	if (FileName)
	{
		char File[MAX_PATH];
		strsafecpy(File, FileName, sizeof(File));
		
		if (!FileExists(File) &&
			GetSelectedType() &&
			GetSelectedType()->Extension())
		{
			// extract extension
			char Ext[64];
			char *s = strchr(GetSelectedType()->Extension(), '.'), *d = Ext;
			if (ValidStr(s) && stricmp(s, ".*"))
			{
				while (*s && *s != ';')
				{
					*d++ = *s++;
				}
				*d++ = 0;
				strlwr(Ext);

				// bung the extension from the file type if not there
				char *Dot = strrchr(File, '.');
				if (!Dot || strchr(Dot, DIR_CHAR))
				{
					strcat(File, Ext);
				}
			}
		}
		
		Status = SaveFile(File);

		if (Status)
		{
			AddFile(File);
		}
		else
		{
			RemoveFile(File);
		}
	}

	return Status;
}

void GMru::_Update()
{
	if (d->Items.Length() > d->Size)
	{
		d->Items.Length(d->Size);
	}

	if (d->Parent)
	{
		// remove existing items.
		d->Parent->Empty();

		// add current items
		if (d->Items.Length() > 0)
		{
			for (int i=0; i<d->Items.Length(); i++)
			{
				GMruEntry *c = d->Items[i];
				d->Parent->AppendItem(c->Display ? c->Display : c->Raw, M_MRU_BASE + i, true);
			}
		}
		else
		{
			d->Parent->AppendItem("(none)", -1, false);
		}
	}
}

bool GMru::Set(GSubMenu *parent, int size)
{
	d->Parent = parent;
	if (size > 0)
	{
		d->Size = size;
	}

	_Update();

	return true;
}

char *GMru::AddFile(char *FileName, bool Update)
{
	char *Status = FileName;
	GMruEntry *c = NULL;
	for (int i=0; i<d->Items.Length(); i++)
	{
		GMruEntry *e = d->Items[i];
		if (stricmp(e->Raw, FileName) == 0)
		{
			// exact string being added.. just move to the top
			// no need to reallocate
			
			e->Raw.Reset(NewStr(FileName)); // This fixes any changes in case...
			
			d->Items.DeleteAt(i, true);
			d->Items.AddAt(0, e);
			c = e;
			break;
		}
	}

	if (!c)
	{
		c = new GMruEntry;
		c->Raw.Reset(NewStr(FileName));
		if (SerializeEntry(&c->Display, &c->Raw, NULL))
		{
			d->Items.AddAt(0, c);
		}
		else LgiAssert(0);
	}

	if (Update)
	{
		// update
		_Update();
	}

	return Status;
}

void GMru::RemoveFile(char *FileName, bool Update)
{
	// remove from list if there
	for (int i=0; i<d->Items.Length(); i++)
	{
		GMruEntry *e = d->Items[i];
		if (stricmp(e->Raw, FileName) == 0)
		{
			d->Items.DeleteAt(i);
			DeleteObj(e);
			break;
		}
	}

	if (Update)
	{
		_Update();
	}
}

void GMru::DoFileDlg(bool Open)
{
	GetFileTypes(&d->Select, false);
	d->Select.ShowReadOnly(Open);
	if (Open ? d->Select.Open() : d->Select.Save())
	{
		d->SelectedType = d->Select.TypeAt(d->Select.SelectedType());
		if (Open)
			_OpenFile(d->Select.Name(), d->Select.ReadOnly());
		else
			_SaveFile(d->Select.Name());
	}
}

void GMru::OnCommand(int Cmd)
{
	GViewI *Wnd = d->Parent->GetMenu() ? d->Parent->GetMenu()->WindowHandle() : 0;
	if (Wnd)
	{
		d->Select.Parent(Wnd);
		d->Select.ClearTypes();
		d->SelectedType = 0;
		
		if (_GetCurFile())
		{
			d->Select.Name(_GetCurFile());
			
			char Path[256];
			strsafecpy(Path, _GetCurFile(), sizeof(Path));
			LgiTrimDir(Path);
			d->Select.InitialDir(Path);
		}

		if (Cmd == IDM_OPEN)
		{
			DoFileDlg(true);
		}
		else if (Cmd == IDM_SAVEAS)
		{
			DoFileDlg(false);
		}
	}

	if (Cmd >= M_MRU_BASE &&
		Cmd < M_MRU_BASE + d->Items.Length())
	{
		int Index = Cmd - M_MRU_BASE;
		GMruEntry *c = d->Items[Index];
		if (c)
		{
			_OpenFile(c->Raw, false);
		}
	}
}

GMessage::Result GMru::OnEvent(GMessage *Msg)
{
	/*
	if (d->Parent &&
		MsgCode(Msg) == M_COMMAND)
	{
		#ifdef BEOS
		int32 Cmd = 0;
		int32 Event = 0;
		Msg->FindInt32("Cmd", &Cmd);
		Msg->FindInt32("Event", &Event);
		#else
		int Cmd = MsgA(Msg) & 0xffff;
		#endif

		OnCommand(Cmd);
	}
	*/

	return false;
}

bool GMru::Serialize(GDom *Store, const char *Prefix, bool Write)
{
	bool Status = false;
	GVariant v;

	if (Store && Prefix)
	{
		if (Write)
		{
			// add our keys
			char Key[64];
			sprintf(Key, "%s.Items", Prefix);
			Store->SetValue(Key, v = (int)d->Items.Length());

			for (int i=0; i<d->Items.Length(); i++)
			{
				GMruEntry *e = d->Items[i];
				LgiAssert(e->Raw.Get());
				GAutoString Stored;
				if (SerializeEntry(NULL, &e->Raw, &Stored)) // Convert Raw -> Stored
				{
					sprintf(Key, "%s.Item%i", Prefix, i++);
					Store->SetValue(Key, v = Stored.Get());
				}
				else LgiAssert(0);
			}
		}
		else
		{
			// clear ourself
			d->Items.DeleteObjects();

			// read our keys in
			char Key[64];
			sprintf(Key, "%s.Items", Prefix);
			GVariant i;
			if (Store->GetValue(Key, i))
			{
				for (int n=0; n<i.CastInt32(); n++)
				{
					sprintf(Key, "%s.Item%i", Prefix, n);

					GVariant File;
					if (Store->GetValue(Key, File))
					{
						GAutoString Stored;
						Stored.Reset(File.ReleaseStr());
						LgiAssert(Stored);
						GAutoPtr<GMruEntry> e(new GMruEntry);
						if (SerializeEntry(&e->Display, &e->Raw, &Stored)) // Convert Stored -> Raw
						{
							d->Items.Add(e.Release());
						}
					}
				}
			}

			_Update();
		}
	}

	return Status;
}


