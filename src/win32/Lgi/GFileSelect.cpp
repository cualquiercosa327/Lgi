/*hdr
**      FILE:           GFileSelect.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           26/9/2001
**      DESCRIPTION:    Common file/directory selection dialog
**
**      Copyright (C) 1998-2001, Matthew Allen
**              fret@ozemail.com.au
*/

#include <stdio.h>
#include <CdErr.h>
#include "Lgi.h"

class GFileSelectPrivate
{
	friend class GFileSelect;

	bool			CanMultiSelect;
	bool			MultiSelected;

	char			*FileStr;
	char			*InitDir;
	char			*DefExt;
	char			*TitleStr;
	bool			WaitForMessage;
	int				SelectedType;
	GViewI			*ParentWnd;
	List<GFileType> TypeList;
	List<char>		Files;
	bool			ShowReadOnly;
	bool			ReadOnly;

	char *TypeStrA()
	{
		GStringPipe p;

		for (GFileType *Type = TypeList.First(); Type; Type = TypeList.Next())
		{
			p.Push(Type->Description());
			p.Push("", 1);
			p.Push(Type->Extension());
			p.Push("", 1);
		}

		return (char*)p.New(1);
	}

	char16 *TypeStrW()
	{
		GMemQueue p;

		for (GFileType *Type = TypeList.First(); Type; Type = TypeList.Next())
		{
			char16 *d = LgiNewUtf8To16(Type->Description());
			char16 *e = LgiNewUtf8To16(Type->Extension());
			
			p.Write((uchar*)d, sizeof(char16)*StrlenW(d));
			p.Write((uchar*)L"", sizeof(char16));
			p.Write((uchar*)e, sizeof(char16)*StrlenW(e));
			p.Write((uchar*)L"", sizeof(char16));

			DeleteArray(d);
			DeleteArray(e);
		}

		return (char16*)p.New(sizeof(char16));
	}

	void BeforeDlg(OPENFILENAME &Info)
	{
		ZeroObj(Info);
		Info.lStructSize = sizeof(Info);
		Info.lpstrFilter = TypeStrA();
		Info.lpstrInitialDir = NewStr(InitDir);
		Info.nMaxFile = 4096;
		Info.lpstrFile = new char[Info.nMaxFile];
		if (Info.lpstrFile)
		{
			Info.lpstrFile[0] = 0;

			GAutoString s(LgiToNativeCp(FileStr));
			if (s)
				strcpy_s(Info.lpstrFile, Info.nMaxFile, s);
		}

		Info.hwndOwner = ParentWnd ? ParentWnd->Handle() : 0;

		if (CanMultiSelect)
			Info.Flags |= OFN_ALLOWMULTISELECT;
		else
			Info.Flags &= ~OFN_ALLOWMULTISELECT;

		if (ShowReadOnly)
			Info.Flags &= ~OFN_HIDEREADONLY;
		else
			Info.Flags |= OFN_HIDEREADONLY;

		Info.Flags |= OFN_EXPLORER;
		Files.DeleteArrays();
	}

	bool DoFallback(OPENFILENAMEA &Info)
	{
		DWORD err = CommDlgExtendedError();
		if (err == FNERR_INVALIDFILENAME)
		{
			// Something about the filename is making the dialog unhappy...
			// So nuke it and go without.
			Info.lpstrFile[0] = 0;
			return true;
		}

		return false;
	}

	bool DoFallback(OPENFILENAMEW &Info)
	{
		DWORD err = CommDlgExtendedError();
		if (err == FNERR_INVALIDFILENAME)
		{
			// Something about the filename is making the dialog unhappy...
			// So nuke it and go without.
			Info.lpstrFile[0] = 0;
			return true;
		}

		return false;
	}

	void AfterDlg(OPENFILENAMEA &Info, bool Status)
	{
		if (Status)
		{
			MultiSelected = strlen(Info.lpstrFile) < Info.nFileOffset;
			if (MultiSelected)
			{
				char s[MAX_PATH << 1];
				strcpy_s(s, sizeof(s), Info.lpstrFile);
				char *e = s + strlen(s);
				if (*e != DIR_CHAR) *e++ = DIR_CHAR;
				char *f = Info.lpstrFile + Info.nFileOffset;
				while (*f)
				{
					strcpy_s(e, sizeof(s) - (e - s), f);
					Files.Insert(LgiFromNativeCp(s));
					f += strlen(f) + 1;
				}
			}
			else
			{
				Files.Insert(LgiFromNativeCp(Info.lpstrFile));
			}

			ReadOnly = TestFlag(Info.Flags, OFN_READONLY);
		}
		else
		{
			DWORD err = CommDlgExtendedError();
			LgiTrace("%s:%i - FileNameDlg error 0x%x\n", _FL, err);
		}

		DeleteArray(Info.lpstrFile);
		DeleteArray((char*&)Info.lpstrInitialDir);
		DeleteArray((char*&)Info.lpstrFilter);
		SelectedType = Info.nFilterIndex - 1;
	}

	void BeforeDlg(OPENFILENAMEW &Info)
	{
		ZeroObj(Info);
		Info.lStructSize = sizeof(Info);
		Info.lpstrFilter = TypeStrW();
		Info.nMaxFile = 4096;
		Info.lpstrFile = new char16[Info.nMaxFile];
		Info.hwndOwner = ParentWnd ? ParentWnd->Handle() : 0;
		
		if (Info.lpstrFile)
		{
			memset(Info.lpstrFile, 0, sizeof(*Info.lpstrFile) * Info.nMaxFile);

			char16 *s = LgiNewUtf8To16(FileStr);
			if (s)
			{
				StrcpyW(Info.lpstrFile, s);
				DeleteArray(s);
			}
		}

		Info.lpstrInitialDir = LgiNewUtf8To16(InitDir);

		if (CanMultiSelect)
			Info.Flags |= OFN_ALLOWMULTISELECT;
		else
			Info.Flags &= ~OFN_ALLOWMULTISELECT;

		if (ShowReadOnly)
			Info.Flags &= ~OFN_HIDEREADONLY;
		else
			Info.Flags |= OFN_HIDEREADONLY;

		Info.Flags |= OFN_EXPLORER;
		Files.DeleteArrays();
	}

	void AfterDlg(OPENFILENAMEW &Info, bool Status)
	{
		DeleteArray(FileStr);
		if (Status)
		{
			MultiSelected = StrlenW(Info.lpstrFile) < Info.nFileOffset;
			if (MultiSelected)
			{
				char16 s[256];
				StrcpyW(s, Info.lpstrFile);
				char16 *e = s + StrlenW(s);
				if (*e != DIR_CHAR) *e++ = DIR_CHAR;
				char16 *f = Info.lpstrFile + Info.nFileOffset;
				while (*f)
				{
					StrcpyW(e, f);
					Files.Insert(LgiNewUtf16To8(s));
					f += StrlenW(f) + 1;
				}
			}
			else
			{
				char *f = LgiNewUtf16To8(Info.lpstrFile);
				if (f)
					Files.Insert(f);
			}

			ReadOnly = TestFlag(Info.Flags, OFN_READONLY);
		}
		else
		{
			DWORD err = CommDlgExtendedError();
			LgiTrace("%s:%i - FileNameDlg error 0x%x\n", _FL, err);
		}

		DeleteArray(Info.lpstrFile);
		DeleteArray((char16*&)Info.lpstrInitialDir);
		DeleteArray((char16*&)Info.lpstrFilter);
		SelectedType = Info.nFilterIndex - 1;
	}

	GFileSelectPrivate()
	{
		CanMultiSelect = false;
		MultiSelected = false;
		InitDir = 0;
		DefExt = 0;
		TitleStr = 0;
		SelectedType = 0;
		ParentWnd = 0;
		FileStr = 0;
		ReadOnly = false;
		ShowReadOnly = false;
	}

	~GFileSelectPrivate()
	{
		DeleteArray(FileStr);
		DeleteArray(InitDir);
		Files.DeleteArrays();
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
GFileSelect::GFileSelect()
{
	d = new GFileSelectPrivate;
}

GFileSelect::~GFileSelect()
{
	ClearTypes();
	DeleteObj(d);
}

char *GFileSelect::Name()
{
	return d->Files.First();
}

bool GFileSelect::Name(const char *n)
{
	bool Status = FALSE;

	DeleteArray(d->FileStr);
	if (n)
	{
		d->FileStr = NewStr(n);
	}

	return Status;
}

char *GFileSelect::operator [](int i)
{
	return d->Files[i];
}

int GFileSelect::Length()
{
	return d->Files.Length();
}

int GFileSelect::Types()
{
	return d->TypeList.Length();
}

int GFileSelect::SelectedType()
{
	return d->SelectedType;
}

GFileType *GFileSelect::TypeAt(int n)
{
	return d->TypeList.ItemAt(n);
}

void GFileSelect::ClearTypes()
{
	d->TypeList.DeleteObjects();
}

bool GFileSelect::Type(const char *Description, const char *Extension, int Data)
{
	GFileType *Type = new GFileType;
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		Type->Data(Data);
		d->TypeList.Insert(Type);
	}

	return Type != 0;
}

GViewI *GFileSelect::Parent()
{
	return d->ParentWnd;
}

void GFileSelect::Parent(GViewI *Window)
{
	d->ParentWnd = Window;
}

bool GFileSelect::ReadOnly()
{
	return d->ReadOnly;
}

void GFileSelect::ShowReadOnly(bool ro)
{
	d->ShowReadOnly = ro;
}

bool GFileSelect::MultiSelect()
{
	return d->CanMultiSelect;
}

void GFileSelect::MultiSelect(bool Multi)
{
	d->CanMultiSelect = Multi;
}

char *GFileSelect::InitialDir()
{
	return d->InitDir;
}

void GFileSelect::InitialDir(const char *InitDir)
{
	DeleteArray(d->InitDir);
	d->InitDir = NewStr(InitDir);
}

char *GFileSelect::Title()
{
	return d->TitleStr;
}

void GFileSelect::Title(const char *Title)
{
	DeleteArray(d->TitleStr);
	d->TitleStr = NewStr(Title);
}

char *GFileSelect::DefaultExtension()
{
	return d->DefExt;
}

void GFileSelect::DefaultExtension(const char *DefExt)
{
	DeleteArray(d->DefExt);
	d->DefExt = NewStr(DefExt);
}

bool GFileSelect::Open()
{
	bool Status = FALSE;

	if (IsWin9x)
	{
		OPENFILENAMEA	Info;
		d->BeforeDlg(Info);
		Status = GetOpenFileNameA(&Info);
		if (!Status && d->DoFallback(Info))
			Status = GetOpenFileNameA(&Info);
		d->AfterDlg(Info, Status);
	}
	else
	{
		OPENFILENAMEW	Info;
		d->BeforeDlg(Info);
		Status = GetOpenFileNameW(&Info);
		if (!Status && d->DoFallback(Info))
			Status = GetOpenFileNameW(&Info);
		d->AfterDlg(Info, Status);
	}

	return Status && Length() > 0;
}

#include "shlobj.h"

int CALLBACK GFileSelectBrowseCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	char *Name = (char*)lpData;
	if (uMsg == BFFM_INITIALIZED && Name)
	{
        PostMessage(hwnd, BFFM_SETSELECTION, true, (GMessage::Param)Name);
	}

	return 0;
}

bool GFileSelect::OpenFolder()
{
	LPMALLOC pMalloc; // Gets the Shell's default allocator
	bool Status = false;

	if  (::SHGetMalloc(&pMalloc) == NOERROR)
	{
		BROWSEINFO      bi;
		char            pszBuffer[MAX_PATH];
		LPITEMIDLIST    pidl;

		ZeroObj(bi);

		// Get help on BROWSEINFO struct - it's got all the bit settings.
		bi.hwndOwner        = (d->ParentWnd) ? d->ParentWnd->Handle() : 0;
		bi.pszDisplayName   = pszBuffer;
		bi.lpszTitle        = "Select a Directory";
		bi.ulFlags          = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_EDITBOX;
		bi.lpfn				= GFileSelectBrowseCallback;
		bi.lParam			= (LPARAM)InitialDir();

		// This next call issues the dialog box.
		if  (pidl = ::SHBrowseForFolder(&bi))
		{
			if  (::SHGetPathFromIDList(pidl, pszBuffer))
			{
				// At this point pszBuffer contains the selected path
				d->Files.Insert(LgiFromNativeCp(pszBuffer));
				Status = true;
			}

			// Free the PIDL allocated by SHBrowseForFolder.
			pMalloc->Free(pidl);
		}

		// Release the shell's allocator
		pMalloc->Release();
	}

	return Status;
}

bool GFileSelect::Save()
{
	bool Status = FALSE;

	if (IsWin9x)
	{
		OPENFILENAMEA	Info;
		d->BeforeDlg(Info);
		Status = GetSaveFileNameA(&Info);
		if (!Status && d->DoFallback(Info))
			Status = GetSaveFileNameA(&Info);		
		d->AfterDlg(Info, Status);
	}
	else
	{
		OPENFILENAMEW	Info;
		d->BeforeDlg(Info);
		Status = GetSaveFileNameW(&Info);
		if (!Status && d->DoFallback(Info))
			Status = GetSaveFileNameW(&Info);		
		d->AfterDlg(Info, Status);
	}

	return Status && Length() > 0;
}
