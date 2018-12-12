#ifndef _VcFile_h_
#define _VcFile_h_

#include "LListItemCheckBox.h"

class VcFile : public LListItem
{
public:
	enum FileStatus
	{
		SUnknown,
		SUntracked,			// Not in version control
		SClean,	
		SModified,
		SReplaced,
		SDeleted,
		SAdded,
		SMissing,
		SConflicted,
	};

private:
	AppPriv *d;
	VcFolder *Owner;
	bool LoadDiff;
	GString Diff;
	GString Revision;
	LListItemCheckBox *Chk;
	FileStatus Status;

public:
	VcFile(AppPriv *priv, VcFolder *owner, GString revision, bool working = false);
	~VcFile();

	int Checked(int Set = -1);
	const char *GetDiff() { return Diff; }
	const char *GetFileName() { return GetText(COL_FILENAME); }
	FileStatus GetStatus();
	
	void SetDiff(GString d);
	void Select(bool b);
	void OnMouseClick(GMouse &m);
};

#endif