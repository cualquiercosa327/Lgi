#ifndef _ADD_FTP_FILE_H_
#define _ADD_FTP_FILE_H_

#include "FtpThread.h"

class AddFtpFile : public GDialog, public FtpCallback
{
	GUri *Base;
	GList *Files;
	GList *Log;
	FtpThread *Thread;

public:
	GArray<char*> Uris;

	AddFtpFile(GViewI *p, char *ftp);
	~AddFtpFile();

	void OnCmdComplete(FtpCmd *Cmd);
	int OnNotify(GViewI *c, int f);
};

#endif