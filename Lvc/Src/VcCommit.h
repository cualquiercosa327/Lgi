#ifndef _VcCommit_h_
#define _VcCommit_h_

class VcFolder;

class VcCommit : public LListItem
{
	AppPriv *d;
	bool Current;
	GString Rev;
	GString Author;
	LDateTime Ts;
	GString Cache;
	GString Msg;

public:
	GString::Array Files;

	VcCommit(AppPriv *priv);

	char *GetRev();
	char *GetAuthor();
	char *GetMsg();
	LDateTime &GetTs() { return Ts; }

	void SetCurrent(bool b);
	char *GetText(int Col);
	bool GitParse(GString s);
	bool SvnParse(GString s);
	bool HgParse(GString s);
	bool CvsParse(LDateTime &Dt, GString Auth, GString Msg);
	VcFolder *GetFolder();

	// Events
	void OnMouseClick(GMouse &m);
	void Select(bool b);
};

#endif