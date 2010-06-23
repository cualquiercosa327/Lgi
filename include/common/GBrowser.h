
#ifndef __GBROWSER_H
#define __GBROWSER_H

#define CMD_BACK		1
#define CMD_STOP		2
#define CMD_HOME		3
#define CMD_FORWARD		4
#define CMD_SEARCH		5

class LgiClass GBrowserCtrl
{
public:
	virtual ~GBrowserCtrl() {}

	// Interface
	virtual char *GetDescription() = 0;

	// Window
	virtual char *Name() = 0;
	virtual void Name(char *s) = 0;
	virtual void Visible(bool i) = 0;
	virtual bool Visible() = 0;
	virtual void Sunken(bool i) = 0;
	virtual bool Sunken() = 0;
	virtual void SetPos(int x1, int y1, int x2, int y2) = 0;
	virtual void GetPos(int &x1, int &y1, int &x2, int &y2) = 0;
	virtual bool Attach(OsView Parent) = 0;
	virtual bool Detach() = 0;

	// Html
	virtual void Browse(char *s) = 0;
	virtual bool GetCurrentURL(char *Buffer) = 0;
	virtual int OnCommand(int Cmd, int a, int b) = 0;
};

typedef GBrowserCtrl *(*Proc_CreateBrowserCtrl)(char *Arg);

#endif
