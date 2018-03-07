#ifndef _VcFile_h_
#define _VcFile_h_

#include "LListItemCheckBox.h"

class VcFile : public LListItem
{
	AppPriv *d;
	VcFolder *Owner;
	GString Diff;
	LListItemCheckBox *Chk;

public:
	VcFile(AppPriv *priv, VcFolder *owner, bool working = false);
	~VcFile();

	int Checked(int Set = -1);
	const char *GetDiff() { return Diff; }
	const char *GetFileName() { return GetText(COL_FILENAME); }
	
	void SetDiff(GString d);
	void Select(bool b);
	void OnMouseClick(GMouse &m);
};

#endif