/// \file
/// \author Matthew Allen
/// \brief Native mac file open dialog.
#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GToken.h"

//////////////////////////////////////////////////////////////////////////
// This is just a private data container to make it easier to change the
// implementation of this class without effecting headers and applications.
class GFileSelectPrivate
{
	friend class GFileSelect;
	friend class GFileSelectDlg;
	friend class GFolderList;

	GView *Parent;
	GFileSelect *Select;

	char *Title;
	char *DefExt;
	bool MultiSelect;
	List<char> Files;
	int CurrentType;
	List<GFileType> Types;
	List<char> History;
	bool ShowReadOnly;
	bool ReadOnly;
	
	bool EatClose;

public:
	static char *InitPath;
	static bool InitShowHiddenFiles;
	static GRect InitSize;

	GFileSelectPrivate(GFileSelect *select)
	{
		ShowReadOnly = false;
		ReadOnly = false;
		EatClose = false;
		Select = select;
		Title = 0;
		DefExt = 0;
		MultiSelect = false;
		Parent = 0;
		CurrentType = -1;
	}

	virtual ~GFileSelectPrivate()
	{
		DeleteArray(Title);
		DeleteArray(DefExt);

		Types.DeleteObjects();
		Files.DeleteArrays();
		History.DeleteArrays();
	}


};

char *GFileSelectPrivate::InitPath = 0;

//////////////////////////////////////////////////////////////////////////
GFileSelect::GFileSelect()
{
	d = new GFileSelectPrivate(this);
}

GFileSelect::~GFileSelect()
{
	DeleteObj(d);
}

void GFileSelect::ShowReadOnly(bool b)
{
	d->ShowReadOnly = b;;
}

bool GFileSelect::ReadOnly()
{
	return d->ReadOnly;
}

char *GFileSelect::Name()
{
	return d->Files.First();
}

bool GFileSelect::Name(const char *n)
{
	d->Files.DeleteArrays();
	if (n)
	{
		d->Files.Insert(NewStr(n));
	}

	return true;
}

char *GFileSelect::operator [](size_t i)
{
	return d->Files.ItemAt(i);
}

size_t GFileSelect::Length()
{
	return d->Files.Length();
}

size_t GFileSelect::Types()
{
	return d->Types.Length();
}

void GFileSelect::ClearTypes()
{
	d->Types.DeleteObjects();
}

GFileType *GFileSelect::TypeAt(ssize_t n)
{
	return d->Types.ItemAt(n);
}

bool GFileSelect::Type(const char *Description, const char *Extension, int Data)
{
	GFileType *Type = new GFileType;
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		d->Types.Insert(Type);
	}

	return Type != 0;
}

ssize_t GFileSelect::SelectedType()
{
	return d->CurrentType;
}

GViewI *GFileSelect::Parent()
{
	return d->Parent;
}

void GFileSelect::Parent(GViewI *Window)
{
	d->Parent = dynamic_cast<GView*>(Window);
}

bool GFileSelect::MultiSelect()
{
	return d->MultiSelect;
}

void GFileSelect::MultiSelect(bool Multi)
{
	d->MultiSelect = Multi;
}

#define CharPropImpl(Func, Var)				\
	char *GFileSelect::Func()				\
	{										\
		return Var;							\
	}										\
	void GFileSelect::Func(const char *i)	\
	{										\
		DeleteArray(Var);					\
		if (i)								\
		{									\
			Var = NewStr(i);				\
		}									\
	}

CharPropImpl(InitialDir, d->InitPath);
CharPropImpl(Title, d->Title);
CharPropImpl(DefaultExtension, d->DefExt);
   
bool GFileSelect::Open()
{
	bool Status = false;
	

	return Status;
}

bool GFileSelect::OpenFolder()
{
	bool Status = false;
	

	return Status;
}

bool GFileSelect::Save()
{
	bool Status = false;
	

	return Status;
}

