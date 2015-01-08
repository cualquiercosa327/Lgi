#ifndef _FTP_THREAD_H_
#define _FTP_THREAD_H_

#include "GList.h"
#include "IFtp.h"

enum FtpCommand
{
	FtpNone,
	FtpList,
	FtpRead,
	FtpWrite
};

class FtpCmd;
class FtpCallback
{
public:
	virtual ~FtpCallback() {}
	virtual void OnCmdComplete(FtpCmd *Cmd) = 0;
};

class FtpCmd
{
public:
	// Input
	FtpCommand Cmd;
	char *Uri;
	GList *Watch;

	// Output
	bool Status;
	char *File;
	List<IFtpEntry> Dir;

	// Callback stuff
	FtpCallback *Callback;
	class NodeView *View;

	FtpCmd(FtpCommand c, FtpCallback *cb);
	~FtpCmd();
	void Error(const char *e);
};

class FtpThread : public GThread
{
	struct FtpThreadPriv *d;

public:
	FtpThread();
	~FtpThread();

	void Post(FtpCmd *Cmd);
	int Main();
};

extern FtpThread *GetFtpThread();
extern void ShutdownFtpThread();

#endif