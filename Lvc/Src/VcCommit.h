#ifndef _VcCommit_h_
#define _VcCommit_h_

class VcCommit : public LListItem
{
	AppPriv *d;
	bool Current;
	GString Rev;
	GString Author;
	LDateTime Ts;
	GString TsCache;
	GString Msg;

public:
	VcCommit(AppPriv *priv);

	char *GetRev();
	void SetCurrent(bool b);
	char *GetText(int Col);
	bool GitParse(GString s);
	bool SvnParse(GString s);
};

#endif