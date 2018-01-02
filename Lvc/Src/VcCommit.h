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
	int IsWeekDay(const char *s);
	int IsMonth(const char *s);
	LDateTime ParseDate(GString s);
	bool GitParse(GString s);
	bool SvnParse(GString s);
};

#endif