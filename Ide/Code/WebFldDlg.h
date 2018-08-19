#ifndef _WEB_FLD_DLG_H_
#define _WEB_FLD_DLG_H_

//////////////////////////////////////////////////////////////////////////////////
class WebFldDlg : public GDialog
{
public:
	char *Name;
	GAutoString Ftp;
	char *Www;

	WebFldDlg(GViewI *p, char *name, char *ftp, char *www);
	~WebFldDlg();
	int OnNotify(GViewI *v, int f);
};

#endif