#ifndef _VcFile_h_
#define _VcFile_h_

class VcFile : public LListItem
{
	AppPriv *d;
	GString Diff;

public:
	VcFile(AppPriv *priv);
	~VcFile();

	const char *GetDiff() { return Diff; }
	void SetDiff(GString d) { Diff = d; }

	void Select(bool b);
};

#endif