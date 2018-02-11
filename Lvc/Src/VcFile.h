#ifndef _VcFile_h_
#define _VcFile_h_

#include "LListItemCheckBox.h"

class VcFile : public LListItem
{
	AppPriv *d;
	GString Diff;
	LListItemCheckBox *Chk;

public:
	VcFile(AppPriv *priv, bool working = false);
	~VcFile();

	int Checked(int Set = -1);
	const char *GetDiff() { return Diff; }
	
	void SetDiff(GString d);
	void Select(bool b);
};

#endif