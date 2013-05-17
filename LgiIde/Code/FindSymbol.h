#ifndef _FIND_SYMBOL_H_
#define _FIND_SYMBOL_H_

class FindSymbolDlg : public GDialog
{
	AppWnd *App;
	struct FindSymbolPriv *d;

public:
	GAutoString File;
	int Line;

	FindSymbolDlg(AppWnd *app);
	~FindSymbolDlg();
	
	int OnNotify(GViewI *v, int f);
	void OnPulse();
	void OnCreate();
	bool OnViewKey(GView *v, GKey &k);
};

#endif