
#ifndef __QWindowMain_h
#define __QWindowMain_h

#include "LgiLinux.h"
#include "xwidget.h"

class XMainWindow : public XWidget
{
	friend class XApplication;
	class XMainWindowPrivate *d;
	
public:
	XMainWindow();
	~XMainWindow();

	// QMenuBar *menuBar();
	bool GetIgnoreInput();
	void SetIgnoreInput(bool i);
	bool Modal();
	void Modal(XMainWindow *Owner);
	XWidget *GetLastFocus();
	void SetLastFocus(XWidget *w);
	void DefaultFocus();
};

#endif
