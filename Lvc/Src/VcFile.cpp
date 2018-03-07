#include "Lvc.h"

VcFile::VcFile(AppPriv *priv, VcFolder *owner, bool working)
{
	d = priv;
	Owner = owner;
	if (working)
		Chk = new LListItemCheckBox(this, 0, false);
	else
		Chk = NULL;
}

VcFile::~VcFile()
{
}

int VcFile::Checked(int Set)
{
	if (!Chk)
		return -1;

	if (Set >= 0)
		Chk->Value(Set);

	return (int)Chk->Value();
}

void VcFile::SetDiff(GString diff)
{
	Diff = diff;
	if (LListItem::Select())
		d->Diff->Name(Diff);
}

void VcFile::Select(bool b)
{
	LListItem::Select(b);
	if (b)
		d->Diff->Name(Diff);
}

void VcFile::OnMouseClick(GMouse &m)
{
	LListItem::OnMouseClick(m);

	if (m.IsContextMenu())
	{
		GSubMenu s;
		char *File = GetText(COL_FILENAME);
		GFile::Path p = Owner->GetPath();
		p += File;

		if (Chk)
		{
			// Uncommitted changes
			s.AppendItem("Revert Changes", IDM_REVERT);
			s.AppendItem("Browse To", IDM_BROWSE);
		}
		else
		{
			// Committed changes
			s.AppendItem("Revert To This Revision", IDM_REVERT_TO_REV);
			s.AppendItem("Blame", IDM_BLAME);
			s.AppendItem("Save As", IDM_SAVE_AS);
		}

		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
			case IDM_REVERT:
			{
				Owner->Revert(p);
				break;
			}
			case IDM_BROWSE:
			{
				if (p.Exists())
					LgiBrowseToFile(p);
				else
					LgiMsg(GetList(), "Can't find file...", AppName);
				break;
			}
			case IDM_REVERT_TO_REV:
			{
				break;
			}
			case IDM_BLAME:
			{
				break;
			}
			case IDM_SAVE_AS:
			{
				break;
			}
		}
	}
	else if (m.Left() && m.Down() && d->Tabs)
	{
		d->Tabs->Value(0);
	}
}
