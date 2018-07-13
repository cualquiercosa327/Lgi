#include "Lvc.h"
#include "GClipBoard.h"
#include "../Resources/resdefs.h"

VcCommit::VcCommit(AppPriv *priv)
{
	d = priv;
	Current = false;
}

char *VcCommit::GetRev()
{
	return Rev;
}

char *VcCommit::GetAuthor()
{
	return Author;
}

char *VcCommit::GetMsg()
{
	return Msg;
}

void VcCommit::SetCurrent(bool b)
{
	Current = b;
}

char *VcCommit::GetText(int Col)
{
	switch (Col)
	{
		case 0:
			return Current ? (char*)"***" : NULL;
		case 1:
			return Rev;
		case 2:
			return Author;
		case 3:
			Cache = Ts.Get();
			return Cache;
		case 4:
			if (!Msg)
				return NULL;
			Cache = Msg.Split("\n", 1)[0];
			return Cache;
	}

	return NULL;
}

bool VcCommit::GitParse(GString s)
{
	GString::Array lines = s.Split("\n");
	if (lines.Length() < 3)
		return false;

	for (unsigned ln = 0; ln < lines.Length(); ln++)
	{
		GString &l = lines[ln];
		if (ln == 0)
			Rev = l.SplitDelimit().Last();
		else if (l.Find("Author:") >= 0)
			Author = l.Split(":", 1)[1].Strip();
		else if (l.Find("Date:") >= 0)
			Ts.Parse(l.Split(":", 1)[1].Strip());
		else if (l.Strip().Length() > 0)
		{
			if (Msg)
				Msg += "\n";
			Msg += l.Strip();
		}
	}

	return Author && Rev;
}

bool VcCommit::CvsParse(LDateTime &Dt, GString Auth, GString Message)
{
	Ts = Dt;
	Ts.ToLocal();
	
	uint64 i;
	if (Ts.Get(i))
		Rev.Printf(LGI_PrintfInt64, i);
	Author = Auth;
	Msg = Message;
	return true;
}

bool VcCommit::HgParse(GString s)
{
	GString::Array Lines = s.SplitDelimit("\n");
	if (Lines.Length() < 1)
		return false;

	for (GString *Ln = NULL; Lines.Iterate(Ln); )
	{
		GString::Array f = Ln->Split(":", 1);
		if (f.Length() == 2)
		{
			if (f[0].Equals("changeset"))
				Rev = f[1].Strip();
			else if (f[0].Equals("user"))
				Author = f[1].Strip();
			else if (f[0].Equals("date"))
				Ts.Parse(f[1].Strip());
			else if (f[0].Equals("summary"))
				Msg = f[1].Strip();
		}
	}

	return Rev.Get() != NULL;
}

bool VcCommit::SvnParse(GString s)
{
	GString::Array lines = s.Split("\n");
	if (lines.Length() < 1)
		return false;

	for (unsigned ln = 0; ln < lines.Length(); ln++)
	{
		GString &l = lines[ln];
		if (ln == 0)
		{
			GString::Array a = l.Split("|");
			if (a.Length() > 3)
			{
				Rev = a[0].Strip(" \tr");
				Author = a[1].Strip();
				Ts.Parse(a[2]);
			}
		}
		else
		{
			if (Msg)
				Msg += "\n";
			Msg += l.Strip();
		}
	}

	Msg = Msg.Strip();

	return Author && Rev && Ts.IsValid();
}

VcFolder *VcCommit::GetFolder()
{
	for (GTreeItem *i = d->Tree->Selection(); i;
		i = i->GetParent())
	{
		auto f = dynamic_cast<VcFolder*>(i);
		if (f)
			return f;
	}
	return NULL;
}

void VcCommit::Select(bool b)
{
	LListItem::Select(b);
	if (Rev && b)
	{
		VcFolder *f = GetFolder();
		if (f)
			f->ListCommit(this);

		if (d->Msg)
		{
			d->Msg->Name(Msg);
			
			GWindow *w = d->Msg->GetWindow();
			if (w)
			{
				w->SetCtrlEnabled(IDC_COMMIT, false);
				w->SetCtrlEnabled(IDC_COMMIT_AND_PUSH, false);
			}
		}
	}
}

void VcCommit::OnMouseClick(GMouse &m)
{
	LListItem::OnMouseClick(m);

	if (m.IsContextMenu())
	{
		GSubMenu s;
		s.AppendItem("Update", IDM_UPDATE, !Current);
		s.AppendItem("Copy Revision", IDM_COPY_REV);
		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
			case IDM_UPDATE:
			{
				VcFolder *f = GetFolder();
				if (!f)
				{
					LgiAssert(!"No folder?");
					break;
				}

				f->OnUpdate(Rev);
				break;
			}
			case IDM_COPY_REV:
			{
				GClipBoard c(GetList());
				c.Text(Rev);
				break;
			}
		}
	}
}
