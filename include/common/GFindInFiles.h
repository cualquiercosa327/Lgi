#ifndef _GFINDINFILES_H_
#define _GFINDINFILES_H_

class GFindInFiles : public GDialog
{
	struct GFindInFilesPriv *d;
	
public:
	GFindInFiles(GViewI *Parent);
	~GFindInFiles();
	
	int OnNotify(GViewI *Ctrl, int Flags);
};

#endif
