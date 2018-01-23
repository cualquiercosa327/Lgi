#include "Lvc.h"
#include "../Resources/resdefs.h"

#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))

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
			ssize_t r = Process->Read(Buf, sizeof(Buf));
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
void VcFolder::Init(AppPriv *priv)
{
	d = priv;
	IsLogging = false;
	IsGetCur = false;
	IsUpdate = false;
	IsFilesCmd = false;
	IsWorkingFld = false;
	Type = VcNone;

	LgiAssert(d != NULL);
}

VcFolder::VcFolder(AppPriv *priv, const char *p)
{
	Init(priv);
	Path = p;
}

VcFolder::VcFolder(AppPriv *priv, GXmlTag *t)
{
	Init(priv);
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
	if (Cmds.Length())
	{
		Cache = Path;
		Cache += " (...)";
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

const char *VcFolder::GetVcName()
{
	switch (GetType())
	{
		case VcGit:
			return "git";
		case VcSvn:
			return "svn";
		case VcHg:
			return "hg";
		default:
			LgiAssert(!"Impl me.");
			break;
	}				

	return NULL;
}

bool VcFolder::StartCmd(const char *Args, ParseFn Parser)
{
	const char *Exe = GetVcName();
	if (!Exe)
		return false;

	GAutoPtr<GSubProcess> Process(new GSubProcess(Exe, Args));
	if (!Process)
		return false;

	Process->SetInitFolder(Path);

	GAutoPtr<Cmd> c(new Cmd);
	if (!c)
		return false;

	c->PostOp = Parser;
	c->Rd.Reset(new ReaderThread(Process.Release(), &c->Buf));
	Cmds.Add(c.Release());

	Update();

	LgiTrace("Cmd: %s %s\n", Exe, Args);

	return true;
}

void VcFolder::Select(bool b)
{
	GTreeItem::Select(b);
	
	if (b)
	{
		if (Log.Length() == 0 && !IsLogging)
		{
			switch (GetType())
			{
				case VcGit:
					IsLogging = StartCmd("log master", &VcFolder::ParseLog);
					break;
				case VcSvn:
					IsLogging = StartCmd("log", &VcFolder::ParseLog);
					break;
				default:
					LgiAssert(!"Impl me.");
					break;
			}				
		}

		char *Ctrl = d->Lst->GetWindow()->GetCtrlName(IDC_FILTER);
		GString Filter = ValidStr(Ctrl) ? Ctrl : NULL;

		d->Lst->RemoveAll();
		List<LListItem> it;
		if (!Uncommit)
			Uncommit.Reset(new UncommitedItem(d));
		it.Add(Uncommit);

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

		if (!CurrentCommit && !IsGetCur)
		{
			switch (GetType())
			{
				case VcGit:
					IsGetCur = StartCmd("rev-parse HEAD", &VcFolder::ParseInfo);
					break;
				case VcSvn:
					IsGetCur = StartCmd("info", &VcFolder::ParseInfo);
					break;
				default:
					LgiAssert(!"Impl me.");
					break;
			}				
		}
	}
}

bool VcFolder::ParseLog(GString s)
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

	IsLogging = false;

	return true;
}

bool VcFolder::ParseInfo(GString s)
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

	IsGetCur = false;

	return true;
}

bool VcFolder::ParseUpdate(GString s)
{
	CurrentCommit = NewRev;
	IsUpdate = false;
	return true;
}

bool VcFolder::ParseWorking(GString s)
{
	d->ClearFiles();

	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.Split("\n\n");
			for (unsigned i=0; i<a.Length()-1; i += 2)
			{
				GString::Array Hdr = a[i].Split("\n");
				GString::Array Body = a[i+1].Split("\n");
				GString Type;

				for (unsigned h=0; h<Hdr.Length(); h++)
				{
					if (Hdr[h].Strip()(-1) == ':')
					{
						Type = Hdr[h].Split(":")[0].Strip();
						int asd=0;
					}
				}

				bool InChanges = Type.Find("Changes") >= 0;
				if (InChanges)
				{
					for (unsigned b=0; b<Body.Length(); b++)
					{
						GString::Array Fn = Body[b].Strip().Split(":", 1);
						VcFile *f = new VcFile(d, true);
						if (Fn.Length() > 1)
						{
							f->SetText(Fn[0], COL_STATE);
							f->SetText(Fn[1].Strip(), COL_FILENAME);
						}
						else
							f->SetText(Fn[0], COL_FILENAME);
						d->Files->Insert(f);
					}
				}
			}
			break;
		}
		case VcSvn:
		{
			GString::Array a = s.Split("\n");
			for (unsigned i=0; i<a.Length(); i++)
			{
				GString Ln = a[i].Strip();
				if (Ln.Length() == 0)
					continue;
				if (Ln(0) == '?')
					; // Ignore
				else
				{
					VcFile *li = new VcFile(d, true);
					li->SetText(Ln, COL_FILENAME);
					d->Files->Insert(li);
				}
			}
			break;
		}
	}

	IsWorkingFld = false;
	d->Files->ResizeColumnsToContent();

	return false;
}

bool VcFolder::ParseFiles(GString s)
{
	d->ClearFiles();

	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.Split("\n");
			GString Diff;
			VcFile *f = NULL;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "diff", 4))
				{
					if (f)
					{
						f->SetDiff(Diff);
						f->Select(false);
					}
					Diff.Empty();

					GString Fn = a[i].Split(" ").Last()(2, -1);
					f = new VcFile(d);
					f->SetText(Fn, COL_FILENAME);
					d->Files->Insert(f);
				}
				else if (!_strnicmp(Ln, "index", 5) ||
						 !_strnicmp(Ln, "commit", 6)   ||
						 !_strnicmp(Ln, "Author:", 7)   ||
						 !_strnicmp(Ln, "Date:", 5)   ||
						 !_strnicmp(Ln, "+++", 3)   ||
						 !_strnicmp(Ln, "---", 3))
				{
					// Ignore
				}
				else
				{
					if (Diff) Diff += "\n";
					Diff += a[i];
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
		case VcSvn:
		{
			GString::Array a = s.Replace("\r").Split("\n");
			GString Diff;
			VcFile *f = NULL;
			bool InPreamble = false;
			bool InDiff = false;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "Index:", 6))
				{
					if (f)
					{
						f->SetDiff(Diff);
						f->Select(false);
					}
					Diff.Empty();
					InDiff = false;
					InPreamble = false;

					GString Fn = a[i].Split(":", 1).Last().Strip();
					f = new VcFile(d);
					f->SetText(Fn, COL_FILENAME);
					d->Files->Insert(f);
				}
				else if (!_strnicmp(Ln, "------", 6))
				{
					InPreamble = !InPreamble;
				}
				else if (!_strnicmp(Ln, "======", 6))
				{
					InPreamble = false;
					InDiff = true;
				}
				else if (InDiff)
				{
					if (!strncmp(Ln, "--- ", 4) ||
						!strncmp(Ln, "+++ ", 4))
					{
					}
					else
					{
						if (Diff) Diff += "\n";
						Diff += a[i];
					}
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
	}

	IsFilesCmd = false;
	d->Files->ResizeColumnsToContent();

	return false;
}

void VcFolder::OnPulse()
{
	bool Reselect = false;
		
	for (unsigned i=0; i<Cmds.Length(); i++)
	{
		Cmd *c = Cmds[i];
		if (c && c->Rd->IsExited())
		{
			GString s = c->Buf.NewGStr();
			Reselect |= CALL_MEMBER_FN(*this, c->PostOp)(s);
			Cmds.DeleteAt(i--, true);
			delete c;
		}
	}

	if (Reselect)
	{
		Select(true);
		Update();
	}
}

void VcFolder::OnRemove()
{
	GXmlTag *t = d->Opts.LockTag(NULL, _FL);
	if (t)
	{
		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			if (c->IsTag(OPT_Folder) &&
				c->GetContent() &&
				!_stricmp(c->GetContent(), Path))
			{
				c->RemoveTag();
				delete c;
				break;
			}
		}
		d->Opts.Unlock();
	}
}

void VcFolder::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		GSubMenu s;
		s.AppendItem("Remove", IDM_REMOVE);
		int Cmd = s.Float(GetTree(), m);
		switch (Cmd)
		{
			case IDM_REMOVE:
			{
				OnRemove();
				delete this;
				break;
			}
			default:
				break;
		}
	}
}

void VcFolder::OnUpdate(const char *Rev)
{
	if (!Rev)
		return;
	
	if (!IsUpdate)
	{
		GString Args;

		NewRev = Rev;
		switch (GetType())
		{
			case VcGit:
				Args.Printf("checkout %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate);
				break;
			case VcSvn:
				Args.Printf("up -r %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate);
				break;
		}
	}
}

void VcFolder::ListCommit(const char *Rev)
{
	if (!IsFilesCmd)
	{
		GString Args;
		switch (GetType())
		{
			case VcGit:
				// Args.Printf("show --oneline --name-only %s", Rev);
				Args.Printf("show %s", Rev);
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles);
				break;
			case VcSvn:
				Args.Printf("log --verbose --diff -r %s", Rev);
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles);
				break;
		}

		if (IsFilesCmd)
			d->ClearFiles();
	}
}

void VcFolder::ListWorkingFolder()
{
	if (!IsWorkingFld)
	{
		d->ClearFiles();

		GString Args;
		switch (GetType())
		{
			case VcGit:
				IsWorkingFld = StartCmd("diff", &VcFolder::ParseFiles);
				break;
			case VcSvn:
				IsWorkingFld = StartCmd("status", &VcFolder::ParseWorking);
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
void VcFolder::UncommitedItem::Select(bool b)
{
	LListItem::Select(b);
	if (b)
	{
		GTreeItem *i = d->Tree->Selection();
		VcFolder *f = dynamic_cast<VcFolder*>(i);
		if (f)
			f->ListWorkingFolder();

		if (d->Msg)
		{
			d->Msg->Name(NULL);
			d->Msg->GetWindow()->SetCtrlEnabled(IDC_COMMIT, true);
		}
	}
}

void VcFolder::UncommitedItem::OnPaint(GItem::ItemPaintCtx &Ctx)
{
	GFont *f = GetList()->GetFont();
	f->Transparent(false);
	f->Colour(Ctx.Fore, Ctx.Back);
	
	GDisplayString ds(f, "(working folder)");
	ds.Draw(Ctx.pDC, Ctx.x1 + ((Ctx.X() - ds.X()) / 2), Ctx.y1 + ((Ctx.Y() - ds.Y()) / 2), &Ctx);
}
