#include "Lvc.h"

VcFile::VcFile(AppPriv *priv, bool working)
{
	d = priv;
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
	if (m.Left() && m.Down() && d->Tabs)
		d->Tabs->Value(0);
}
