#include "Lgi.h"
#include "GOptionsFile.h"

GOptionsFile::GOptionsFile(char *BaseName, char *FileName) : GSemaphore("GOptionsFile")
{
	File = 0;
	if (FileName AND FileExists(FileName))
	{
		File = NewStr(FileName);
	}
	else
	{
		char FullPath[MAX_PATH];
		if (LgiGetSystemPath(LSP_APP_ROOT, FullPath, sizeof(FullPath)))
		{
			LgiMakePath(FullPath, sizeof(FullPath), FullPath, BaseName ? BaseName : (char*)"Options");
			if (!LgiGetExtension(FullPath))
				strsafecat(FullPath, ".xml", sizeof(FullPath));
			File = NewStr(FullPath);
		}
	}

	Tag = NewStr("Options");
	Dirty = false;
	Error = 0;

	if (Lock(_FL))
	{
		_Defaults();
		Unlock();
	}
}

GOptionsFile::~GOptionsFile()
{
	DeleteArray(File);
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

void GOptionsFile::SetFile(char *f)
{
	DeleteArray(File);
	File = NewStr(f);
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
				OR
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
				if (Status = Tree.Read(this, &f, 0))
				{
					_Defaults();
				}
			}
		}
		Unlock();
	}

	return Status;
}

bool GOptionsFile::DeleteValue(char *Name)
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

bool GOptionsFile::CreateTag(char *Name)
{
	GXmlTag *Status = 0;

	if (Name AND Lock(_FL))
	{
		Status = GetTag(Name, true);
		Unlock();
	}

	return Status != 0;
}

bool GOptionsFile::DeleteTag(char *Name)
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

GXmlTag *GOptionsFile::LockTag(char *Name, char *File, int Line)
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
