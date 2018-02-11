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
	VcCommit(AppPriv *priv);

	char *GetRev();
	char *GetAuthor();
	char *GetMsg();

	void SetCurrent(bool b);
	char *GetText(int Col);
	bool GitParse(GString s);
	bool SvnParse(GString s);
	VcFolder *GetFolder();

	// Events
	void OnMouseClick(GMouse &m);
	void Select(bool b);
};

#endif