#include "Lgi.h"
#include "GOptionsFile.h"

#define DEBUG_OPTS_FILE		0


void GOptionsFile::_Init()
{
	Dirty = false;
	Tag = Allocator->Alloc("Options");
	if (Lock(_FL))
	{
		_Defaults();
		Unlock();
	}
}

GOptionsFile::GOptionsFile(const char *FileName) : LMutex("GOptionsFile")
{
	_Init();

	if (FileExists(FileName))
		File.Reset(NewStr(FileName));
	else
		SetMode(PortableMode, FileName);
}

GOptionsFile::GOptionsFile(PortableType Mode, const char *BaseName) : LMutex("GOptionsFile")
{
	_Init();

	SetMode(Mode, BaseName);
}

GOptionsFile::~GOptionsFile()
{
}

bool GOptionsFile::SetMode(PortableType Mode, const char *BaseName)
{
	char FullPath[MAX_PATH];
	if (!LGetSystemPath(Mode == DesktopMode ? LSP_APP_ROOT : LSP_APP_INSTALL, FullPath, sizeof(FullPath)))
		return false;

	if (!DirExists(FullPath))
		FileDev->CreateFolder(FullPath);

	LgiMakePath(FullPath, sizeof(FullPath), FullPath, BaseName ? BaseName : (char*)"Options");
	if (!LgiGetExtension(FullPath))
		strcat_s(FullPath, sizeof(FullPath), ".xml");
	File.Reset(NewStr(FullPath));
	Dirty = !FileExists(File);

	return true;
}

bool GOptionsFile::_OnAccess(bool Start)
{
	if (Start)
	{
		return Lock(_FL);
	}

	Unlock();
	return true;
}

bool GOptionsFile::IsValid()
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (Attr.Length() > 0 &&
			Children.Length() > 0)
		{
			Status = true;
		}

		Unlock();
	}

	return Status;
}

void GOptionsFile::SetFile(const char *f)
{
	File.Reset(NewStr(f));
	Dirty = true;
}

bool GOptionsFile::SerializeFile(bool Write)
{
	bool Status = false;

	if (File && Lock(_FL))
	{
		GFile f;
		if
		(
			(
				Write
				||
				FileExists(File)
			)
			&&
			f.Open(File, Write ? O_WRITE : O_READ)
		)
		{
			#if DEBUG_OPTS_FILE
			LgiTrace("%s:%i - GOptionsFile::Serialize(%i) File='%s'\n", 
				_FL, Write, File.Get());
			#endif
			
			GXmlTree Tree(GXT_PRETTY_WHITESPACE);
			if (Write)
			{
				f.SetSize(0);
				Status = Tree.Write(this, &f);
			}
			else
			{
				Empty(true);
				if ((Status = Tree.Read(this, &f, 0)))
				{
					_Defaults();
				}
			}
		}
		else if (Write)
		{
			LgiTrace("%s:%i - Failed to open '%s'\n", _FL, File.Get());
			LgiAssert(!"Failed to open file.");
		}

		Unlock();
	}

	return Status;
}

bool GOptionsFile::DeleteValue(const char *Name)
{
	bool Status = false;

	if (Name && Lock(_FL))
	{
		GVariant v;
		SetValue(Name, v);
		Unlock();
	}

	return Status;
}

bool GOptionsFile::CreateTag(const char *Name)
{
	GXmlTag *Status = 0;

	if (Name && Lock(_FL))
	{
		Status = GetChildTag(Name, true);
		Unlock();
	}

	return Status != 0;
}

bool GOptionsFile::DeleteTag(const char *Name)
{
	bool Status = false;

	if (Name && Lock(_FL))
	{
		GXmlTag *t = GetChildTag(Name);
		if (t)
		{
			t->RemoveTag();
			DeleteObj(t);
			Status = true;
		}
		Unlock();
	}

	return Status;
}

GXmlTag *GOptionsFile::LockTag(const char *Name, const char *File, int Line)
{
	GXmlTag *t = 0;

	if (Lock(File, Line))
	{
		t = Name ? GXmlTag::GetChildTag(Name) : this;
		if (!t)
		{
			Unlock();
		}
	}

	return t;
}
