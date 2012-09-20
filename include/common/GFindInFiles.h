#ifndef _GFINDINFILES_H_
#define _GFINDINFILES_H_

#define OPT_SearchTerm		"FIF_Search"
#define OPT_SearchFolder	"FIF_Folder"
#define OPT_SearchPattern	"FIF_Pattern"

class GFindInFiles : public GDialog
{
	struct GFindInFilesPriv *d;
	
public:
	GFindInFiles(GViewI *Parent, GAutoString Search, GDom *Store);
	~GFindInFiles();
	
	int OnNotify(GViewI *Ctrl, int Flags);
};

#endif
