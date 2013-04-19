#include "Lgi.h"
#include "GOptionsFile.h"

void GOptionsFile::_Init()
{
	Dirty = false;
	Tag = NewStr("Options");
	if (Lock(_FL))
	{
		_Defaults();
		Unlock();
	}
}

GOptionsFile::GOptionsFile(const char *FileName) : GMutex("GOptionsFile")
{
	_Init();

	if (FileExists(FileName))
		File.Reset(NewStr(FileName));
	else
		SetMode(PortableMode);
}

GOptionsFile::GOptionsFile(PortableType Mode, const char *BaseName) : GMutex("GOptionsFile")
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
	if (!LgiGetSystemPath(Mode == DesktopMode ? LSP_APP_ROOT : LSP_APP_INSTALL, FullPath, sizeof(FullPath)))
		return false;

	if (!DirExists(FullPath))
		FileDev->CreateFolder(FullPath);

	LgiMakePath(FullPath, sizeof(FullPath), FullPath, BaseName ? BaseName : (char*)"Options");
	if (!LgiGetExtension(FullPath))
		strsafecat(FullPath, ".xml", sizeof(FullPath));
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
		if (Attr.Length() > 0 AND
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

bool GOptionsFile::Serialize(bool Write)
{
	bool Status = false;

	if (File AND Lock(_FL))
	{
		GFile f;
		if
		(
			(
				Write
				||
				FileExists(File)
			)
			AND
			f.Open(File, Write?O_WRITE:O_READ)
		)
		{
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
			LgiAssert(!"Failed to open file.");

		Unlock();
	}

	return Status;
}

bool GOptionsFile::DeleteValue(const char *Name)
{
	bool Status = false;

	if (Name AND Lock(_FL))
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

	if (Name AND Lock(_FL))
	{
		Status = GetTag(Name, true);
		Unlock();
	}

	return Status != 0;
}

bool GOptionsFile::DeleteTag(const char *Name)
{
	bool Status = false;

	if (Name AND Lock(_FL))
	{
		GXmlTag *t = GetTag(Name);
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
		t = Name ? GXmlTag::GetTag(Name) : this;
		if (!t)
		{
			Unlock();
		}
	}

	return t;
}
