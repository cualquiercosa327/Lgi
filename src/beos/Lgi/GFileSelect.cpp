/*hdr
**      FILE:           GFileSelect.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           18/7/98
**      DESCRIPTION:    Common file/directory selection dialog
**
**      Copyright (C) 1998-2001, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <FilePanel.h>
#include <Path.h>

#include "Lgi.h"
#include "Clipboard.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
class GFileSelectPrivate : public BHandler
{
public:
	int Size;
	char *FileStr;
	char *InitDir;
	char *DefExt;
	char *TitleStr;
	bool MultiSel;
	BFilePanel *Panel;
	List<GFileType> TypeList;
	GView *ParentWnd;
	bool WaitForMessage;
	BMessenger *Messenger;
	
	GFileSelectPrivate() : BHandler("GFileSelect")
	{
		ParentWnd = 0;
		WaitForMessage = false;
		Size = 4096;
		FileStr = NEW(char[Size]);
		if (FileStr)
		{
			*FileStr = 0;
		}
		else
		{
			Size = 0;
		}
		
		MultiSel = FALSE;
		InitDir = 0;
		DefExt = 0;
		TitleStr = 0;
		Panel = 0;
	}

	~GFileSelectPrivate()
	{
		DeleteObj(Panel);
		DeleteArray(FileStr);
		DeleteArray(InitDir);
		DeleteArray(DefExt);
		DeleteArray(TitleStr);
		TypeList.DeleteObjects();
	}

	void Wait()
	{
		thread_id	this_tid = find_thread(NULL);
		BLooper		*pLoop;
		BWindow		*pWin = 0;
	
		WaitForMessage = true;
		pLoop = BLooper::LooperForThread(this_tid);
		if (pLoop)
		{
			pWin = dynamic_cast<BWindow*>(pLoop);
		}
	
		if (pWin)
		{
			do
			{
				// update the window periodically
				pWin->UpdateIfNeeded();
				snooze(50);
			}
			while (Panel->IsShowing() AND WaitForMessage);
		}
		else
		{
			do
			{
				// just wait for exit
				snooze(50);
			}
			while (Panel->IsShowing() AND WaitForMessage);
		}
	}
	
	void MessageReceived(BMessage *message)
	{
		WaitForMessage = false;
	
		entry_ref ref;
		char *name = 0;
		if (message->FindRef("refs", &ref) == B_OK)
		{
			// open
			BEntry Entry(&ref, true);
			BPath Path;
			Entry.GetPath(&Path);
			strcpy(FileStr, Path.Path());
		}
		else if (message->FindRef("directory", &ref) == B_OK AND
				message->FindString("name", (const char**) &name) == B_OK)
		{
			// save
			BEntry Entry(&ref, true);
			BPath Path;
			Entry.GetPath(&Path);
	
			strcpy(FileStr, Path.Path());
			strcat(FileStr, DIR_STR);
			strcat(FileStr, name);
		}
	}
	
	bool DoDialog(file_panel_mode Mode, int32 NodeType = 0)
	{
		BEntry Dir(InitDir);
		entry_ref DirRef;
		Dir.GetRef(&DirRef);
	
		Messenger = 0;
		Panel = NEW(BFilePanel(	Mode,
								NULL,
								&DirRef,
								NodeType,
								MultiSel,
								NEW(BMessage(M_CHANGE)),
								NULL,
								TRUE, // modal
								FALSE)); // hide when done
		if (Panel)
		{
			if (ValidStr(FileStr))
			{
				Panel->SetSaveText(FileStr);
			}
		
			BWindow *Wnd = Panel->Window();
			if (Wnd->Lock())
			{
				Wnd->AddHandler(this);
				Wnd->Unlock();
			}
			
			Messenger = NEW(BMessenger(this));
			Panel->SetTarget(*Messenger);
			Panel->Show();
			Wait();
			Panel->Hide();
		}
	
		return strlen(FileStr) > 0;
	}	
};

GFileSelect::GFileSelect()
{
	d = NEW(GFileSelectPrivate);
}

GFileSelect::~GFileSelect()
{
	DeleteObj(d);
}

char *GFileSelect::Name()
{
	return d->FileStr;
}

bool GFileSelect::Name(char *n)
{
	bool Status = FALSE;

	if (n AND d->FileStr)
	{
		strncpy(d->FileStr, n, d->Size);
	}

	return Status;
}

char *GFileSelect::operator [](int i)
{
	return NULL;
}

int GFileSelect::Length()
{
	return 1;
}

/*
char *GFileSelect::File()
{
	char *Ret = 0;
	char *Last = FileStr;
	char *Dir = strchr(Last, '/');
	while (Dir)
	{
		Last = Dir + 1;
		Dir = strchr(Last, '/');
	}
	return Last;
}

char *GFileSelect::Extension()
{
	char *Ret = File();
	char *Dot = strchr(Ret, '.');
	if (Dot)
	{
		return Dot + 1;
	}
	return NULL;
}
*/

int GFileSelect::Types()
{
	return d->TypeList.GetItems();
}

void GFileSelect::ClearTypes()
{
	d->TypeList.DeleteObjects();
}

GFileType *GFileSelect::TypeAt(int n)
{
	return d->TypeList[n];
}

bool GFileSelect::Type(char *Description, char *Extension, int Data)
{
	GFileType *Type = NEW(GFileType);
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		d->TypeList.Insert(Type);
	}

	return Type != 0;
}

int GFileSelect::SelectedType()
{
	return -1;
}

GView *GFileSelect::Parent()
{
	return d->ParentWnd;
}

void GFileSelect::Parent(GView *Window)
{
	d->ParentWnd = Window;
}

bool GFileSelect::MultiSelect()
{
	return d->MultiSel;
}

void GFileSelect::MultiSelect(bool Multi)
{
	d->MultiSel = Multi;
}

char *GFileSelect::InitialDir()
{
	return d->InitDir;
}

void GFileSelect::InitialDir(char *Dir)
{
	DeleteArray(d->InitDir);
	d->InitDir = NewStr(Dir);
}

char *GFileSelect::Title()
{
	return d->TitleStr;
}

void GFileSelect::Title(char *title)
{
	DeleteArray(d->TitleStr);
	d->TitleStr = NewStr(title);
}

char *GFileSelect::DefaultExtension()
{
	return d->DefExt;
}

void GFileSelect::DefaultExtension(char *DefExt)
{
	DeleteArray(d->DefExt);
	d->DefExt = NewStr(DefExt);
}

bool GFileSelect::Open()
{
	return d->DoDialog(B_OPEN_PANEL);
}

bool GFileSelect::OpenFolder()
{
	return d->DoDialog(B_OPEN_PANEL, B_DIRECTORY_NODE);
}

bool GFileSelect::Save()
{
	return d->DoDialog(B_SAVE_PANEL);
}

