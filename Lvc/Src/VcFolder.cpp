#include "Lvc.h"
#include "../Resources/resdefs.h"

ReaderThread::ReaderThread(GSubProcess *p, GStream *out) : LThread("ReaderThread")
{
	Process = p;
	Out = out;
	Run();
}

ReaderThread::~ReaderThread()
{
	Out = NULL;
	while (!IsExited())
		LgiSleep(1);
}

int ReaderThread::Main()
{
	Process->Start(true, false);

	while (Process->IsRunning())
	{
		if (Out)
		{
			char Buf[1024];
			int r = Process->Read(Buf, sizeof(Buf));
			if (r > 0)
				Out->Write(Buf, r);
		}
		else
		{
			Process->Kill();
			break;
		}
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
VcFolder::VcFolder(AppPriv *priv, const char *p)
{
	d = priv;
	Path = p;
	Type = VcNone;
}

VcFolder::VcFolder(AppPriv *priv, GXmlTag *t)
{
	d = priv;
	Type = VcNone;
	Serialize(t, false);
}

VersionCtrl VcFolder::GetType()
{
	if (Type == VcNone)
		Type = DetectVcs(Path);
	return Type;
}

char *VcFolder::GetText(int Col)
{
	if (ReadCurrent || ReadLog)
	{
		Cache = Path;
		Cache += " (loading...)";
		return Cache;
	}

	return Path;
}

bool VcFolder::Serialize(GXmlTag *t, bool Write)
{
	if (Write)
		t->SetContent(Path);
	else
		Path = t->GetContent();
	return true;
}

GXmlTag *VcFolder::Save()
{
	GXmlTag *t = new GXmlTag(OPT_Folder);
	if (t)
		Serialize(t, true);
	return t;
}

void VcFolder::Select(bool b)
{
	GTreeItem::Select(b);
	
	if (b)
	{
		if (Log.Length() == 0 && !ReadLog && LogBuf.GetSize() == 0)
		{
			GSubProcess *Process = NULL;

			switch (GetType())
			{
				case VcGit:
					Process = new GSubProcess("git", "log master");
					break;
				case VcSvn:
					Process = new GSubProcess("svn", "log");
					break;
			}				
			if (Process)
			{
				Process->SetInitFolder(Path);
				ReadLog.Reset(new ReaderThread(Process, &LogBuf));
				Update();
			}
		}

		char *Ctrl = d->Lst->GetWindow()->GetCtrlName(IDC_FILTER);
		GString Filter = ValidStr(Ctrl) ? Ctrl : NULL;

		d->Lst->RemoveAll();
		List<LListItem> it;
		int64 CurRev = Atoi(CurrentCommit.Get());
		for (unsigned i=0; i<Log.Length(); i++)
		{
			if (CurrentCommit &&
				Log[i]->GetRev())
			{
				switch (GetType())
				{
					case VcSvn:
					{
						int64 LogRev = Atoi(Log[i]->GetRev());
						if (CurRev >= 0 && CurRev >= LogRev)
						{
							CurRev = -1;
							Log[i]->SetCurrent(true);
						}
						else
						{
							Log[i]->SetCurrent(false);
						}
						break;
					}
					default:
						Log[i]->SetCurrent(!_stricmp(CurrentCommit, Log[i]->GetRev()));
						break;
				}
			}

			bool Add = !Filter;
			if (Filter)
			{
				const char *s = Log[i]->GetRev();
				if (s && strstr(s, Filter) != NULL)
					Add = true;

				s = Log[i]->GetAuthor();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
				s = Log[i]->GetMsg();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
			}
			if (Add)
				it.Insert(Log[i]);
		}
		d->Lst->Insert(it);
		if (GetType() == VcSvn)
			d->Lst->ResizeColumnsToContent();
		d->Lst->UpdateAllItems();

		if (!CurrentCommit && !ReadCurrent && InfoBuf.GetSize() == 0)
		{
			GSubProcess *Process = NULL;

			switch (GetType())
			{
				case VcGit:
					Process = new GSubProcess("git", "rev-parse HEAD");
					break;
				case VcSvn:
					Process = new GSubProcess("svn", "info");
					break;
			}				
			if (Process)
			{
				Process->SetInitFolder(Path);
				ReadCurrent.Reset(new ReaderThread(Process, &InfoBuf));
				Update();
			}
		}
	}
}

void VcFolder::ParseLog(GString s)
{
	switch (GetType())
	{
		case VcGit:
		{
			GString::Array c = s.Split("commit");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d));
				if (Rev->GitParse(c[i]))
					Log.Add(Rev.Release());
			}
			break;
		}
		case VcSvn:
		{
			GString::Array c = s.Split("------------------------------------------------------------------------");
			printf("c=%i\n", c.Length());
			for (unsigned i=0; i<c.Length(); i++)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d));
				GString Raw = c[i].Strip();
				if (Rev->SvnParse(Raw))
					Log.Add(Rev.Release());
				else
					printf("Failed:\n%s\n\n", Raw.Get());
			}
			break;
		}			
		default:
			LgiAssert(!"Impl me.");
			break;
	}
}

void VcFolder::ParseInfo(GString s)
{
	switch (GetType())
	{
		case VcGit:
		{
			CurrentCommit = s.Strip();
			break;
		}
		case VcSvn:
		{
			GString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				if (c[i].Find("Revision:") >= 0)
				{
					CurrentCommit = c[i].Split(":", 1)[1].Strip();
					break;
				}
			}
			break;
		}			
		default:
			LgiAssert(!"Impl me.");
			break;
	}
}

void VcFolder::ParseFiles(GString s)
{
	d->Files->Empty();

	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.Split("\n");
			for (unsigned i=1; i<a.Length(); i++)
			{
				LListItem *li = new LListItem;
				li->SetText(a[i]);
				d->Files->Insert(li);
			}
			break;
		}
	}
}

void VcFolder::OnPulse()
{
	bool Reselect = false;
		
	if (ReadLog && ReadLog->IsExited())
	{
		ParseLog(LogBuf.NewGStr());
		ReadLog.Reset();
		Reselect = true;
	}
	if (ReadCurrent && ReadCurrent->IsExited())
	{
		ParseInfo(InfoBuf.NewGStr());
		ReadCurrent.Reset();
		Reselect = true;
	}
	if (UpdateCmd && UpdateCmd->IsExited())
	{
		UpdateCmd.Reset();
		CurrentCommit = NewRev;
		Reselect = true;
	}
	if (FilesCmd && FilesCmd->IsExited())
	{
		ParseFiles(FilesBuf.NewGStr());
		FilesCmd.Reset();
	}

	if (Reselect)
	{
		Select(true);
		Update();
	}
}

void VcFolder::OnUpdate(const char *Rev)
{
	if (!Rev) return;
	
	if (!UpdateCmd)
	{
		GSubProcess *Process = NULL;
		GString Exe, Args;

		switch (GetType())
		{
			case VcGit:
				Exe = "git";			
				Args.Printf("checkout %s", Rev);
				break;
			case VcSvn:
				Exe = "svn";
				Args.Printf("up -r %s", Rev);
				break;
		}
			
		Process = new GSubProcess(Exe, Args);
		if (Process)
		{
			Process->SetInitFolder(Path);
			UpdateCmd.Reset(new ReaderThread(Process, &UpBuf));
			NewRev = Rev;
		}
	}
}

void VcFolder::ListCommit(const char *Rev)
{
	if (!FilesCmd)
	{
		GSubProcess *Process = NULL;
		GString Exe, Args;

		switch (GetType())
		{
			case VcGit:
				Exe = "git";			
				Args.Printf("show --oneline --name-only %s", Rev);
				break;
			case VcSvn:
				Exe = "svn";
				Args.Printf("log --verbose -r %s", Rev);
				break;
		}
			
		Process = new GSubProcess(Exe, Args);
		if (Process)
		{
			Process->SetInitFolder(Path);
			FilesCmd.Reset(new ReaderThread(Process, &FilesBuf));
		}
	}
}
