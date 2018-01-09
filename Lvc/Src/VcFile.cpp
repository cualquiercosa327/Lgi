#include "Lvc.h"

VcFile::VcFile(AppPriv *priv)
{
	d = priv;
}

VcFile::~VcFile()
{
}

void VcFile::Select(bool b)
{
	LListItem::Select(b);
	if (b)
		d->Txt->Name(Diff);

}

