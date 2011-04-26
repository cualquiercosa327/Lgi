#include <stdio.h>

#include "Lgi.h"
#include "GMru.h"
#include "GVariant.h"

////////////////////////////////////////////////////////////////////
#define M_MRU_BASE				(M_USER+0x3500)

class GMruPrivate
{
public:
	int _Size;
	List<char> _Items;
	GSubMenu *_Parent;
	GFileType *_SelectedType;
	GFileSelect _Select;

	GMruPrivate()	
	{
		_Parent = 0;
		_Size = 10;
		_SelectedType = 0;
	}

	~GMruPrivate()	
	{
		_Items.DeleteArrays();
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

void GMru::GetFileTypes(GFileSelect *Dlg, bool Write)
{
	Dlg->Type("All Files", LGI_ALL_FILES);
}

char *GMru::_GetCurFile()
{
	return d->_Items.First();
}

GFileType *GMru::GetSelectedType()
{
	return d->_SelectedType;
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
				while (*s AND *s != ';')
				{
					*d++ = *s++;
				}
				*d++ = 0;
				strlwr(Ext);

				// bung the extension from the file type if not there
				char *Dot = strrchr(File, '.');
				if (!Dot OR strchr(Dot, DIR_CHAR))
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
	while (d->_Items.Length() > d->_Size)
	{
		char *s = d->_Items.Last();
		d->_Items.Delete(s);
		DeleteArray(s);
	}

	if (d->_Parent)
	{
		// remove existing items.
		d->_Parent->Empty();

		// add current items
		if (d->_Items.Length() > 0)
		{
			int i = M_MRU_BASE;
			for (char *c = d->_Items.First(); c; c = d->_Items.Next())
			{
				d->_Parent->AppendItem(c, i++, true);
			}
		}
		else
		{
			d->_Parent->AppendItem("(none)", -1, false);
		}
	}
}

bool GMru::Set(GSubMenu *parent, int size)
{
	d->_Parent = parent;
	if (size > 0)
	{
		d->_Size = size;
	}

	_Update();

	return true;
}

char *GMru::AddFile(char *FileName, bool Update)
{
	char *Status = FileName;
	char *c = 0;
	for (c = d->_Items.First(); c; c = d->_Items.Next())
	{
		if (stricmp(c, FileName) == 0)
		{
			// exact string being added.. just move to the top
			// no need to reallocate
			d->_Items.Delete(c);
			d->_Items.Insert(c, 0);
			break;
		}
	}

	if (!c)
	{
		Status = NewStr(FileName);
		d->_Items.Insert(Status, 0);
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
	for (char *c = d->_Items.First(); c; c = d->_Items.Next())
	{
		if (stricmp(c, FileName) == 0)
		{
			d->_Items.Delete(c);
			DeleteArray(c);
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
	GetFileTypes(&d->_Select, false);
	d->_Select.ShowReadOnly(Open);
	if (Open ? d->_Select.Open() : d->_Select.Save())
	{
		d->_SelectedType = d->_Select.TypeAt(d->_Select.SelectedType());
		if (Open)
			_OpenFile(d->_Select.Name(), d->_Select.ReadOnly());
		else
			_SaveFile(d->_Select.Name());
	}
}

void GMru::OnCommand(int Cmd)
{
	GViewI *Wnd = d->_Parent->GetMenu() ? d->_Parent->GetMenu()->WindowHandle() : 0;
	if (Wnd)
	{
		d->_Select.Parent(Wnd);
		d->_Select.ClearTypes();
		d->_SelectedType = 0;
		
		if (_GetCurFile())
		{
			d->_Select.Name(_GetCurFile());
			
			char Path[256];
			strsafecpy(Path, _GetCurFile(), sizeof(Path));
			LgiTrimDir(Path);
			d->_Select.InitialDir(Path);
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

	if (Cmd >= M_MRU_BASE AND
		Cmd < M_MRU_BASE + d->_Items.Length())
	{
		int Index = Cmd - M_MRU_BASE;
		char *c = d->_Items.ItemAt(Index);
		if (c)
		{
			_OpenFile(c, false);
		}
	}
}

int GMru::OnEvent(GMessage *Msg)
{
	/*
	if (d->_Parent AND
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

bool GMru::Serialize(GDom *Store, char *Prefix, bool Write)
{
	bool Status = false;
	GVariant v;

	if (Store AND Prefix)
	{
		if (Write)
		{
			// add our keys
			char Key[64];
			sprintf(Key, "%s.Items", Prefix);
			Store->SetValue(Key, v = d->_Items.Length());

			int i=0;
			for (char *c=d->_Items.First(); c; c=d->_Items.Next())
			{
				sprintf(Key, "%s.Item%i", Prefix, i++);
				Store->SetValue(Key, v = c);
			}
		}
		else
		{
			// clear ourself
			d->_Items.DeleteArrays();

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
						d->_Items.Insert(NewStr(File.Str()));
					}
				}
			}

			_Update();
		}
	}

	return Status;
}

/*
bool GMru::Serialize(ObjProperties *Props, char *Prefix, bool Write)
{
	bool Status = false;

	if (Props AND Prefix)
	{
		if (Write)
		{
			// clear existing keys
			for (bool b=Props->FirstKey(); b; b=Props->NextKey())
			{
				if (Props->KeyName() AND
					strnicmp(Props->KeyName(), "Prefix", strlen(Prefix)) == 0)
				{
					Props->DeleteKey();
					b = Props->KeyType();
				}
				else
				{
					b=Props->NextKey();
				}
			}

			// add our keys
			char Key[64];
			sprintf(Key, "%s.Items", Prefix);
			Props->Set(Key, d->_Items.Length());

			int i=0;
			for (char *c=d->_Items.First(); c; c=d->_Items.Next())
			{
				sprintf(Key, "%s.%i", Prefix, i++);
				Props->Set(Key, c);
			}
		}
		else
		{
			// clear ourself
			d->_Items.DeleteArrays();

			// read our keys in
			char Key[64];
			sprintf(Key, "%s.Items", Prefix);
			int i = 0;
			if (Props->Get(Key, i))
			{
				for (int n=0; n<i; n++)
				{
					sprintf(Key, "%s.%i", Prefix, n);

					char *File;
					if (Props->Get(Key, File))
					{
						d->_Items.Insert(NewStr(File));
					}
				}
			}

			_Update();
		}
	}

	return Status;
}
*/

