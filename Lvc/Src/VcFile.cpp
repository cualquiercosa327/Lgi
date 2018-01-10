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

void VcFile::Select(bool b)
{
	LListItem::Select(b);
	if (b)
		d->Txt->Name(Diff);

}

