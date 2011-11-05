/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A progress window

#ifndef __GPROGRESSDLG_H
#define __GPROGRESSDLG_H

class LgiClass ProgressList : public List<Progress>
{
	bool InUse;

public:
	ProgressList();

	bool Lock();
	void Unlock();
};

/// Progress window pane, tracks one task.
class LgiClass GProgressPane : public Progress, public GLayout
{
	friend class GProgressDlg;

	Progress *Ref;

protected:
	bool Wait;

	GText *Desc;
	GText *ValText;
	GText *Rate;
	GProgress *Bar;
	GButton *But;

public:
	GProgressPane();
	~GProgressPane();

	char *GetDescription();
	void SetDescription(const char *d);
	void GetLimits(int64 *l, int64 *h) { Progress::GetLimits(l, h); }
	void SetLimits(int64 l, int64 h);
	int64 Value() { return Progress::Value(); }
	void Value(int64 v);
	GFont *GetFont();

	void OnCreate();
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPaint(GSurface *pDC);
	bool Pour(GRegion &r);
};

/// Progress dialog
class LgiClass GProgressDlg : public GDialog
{
protected:
	bool Wait;
	List<GProgressPane> Progri;

	void Resize();

public:
	/// Constructor
	GProgressDlg
	(
		/// The parent window
		GView *parent = 0,
		/// true if the window should not destroy itself when the user
		/// cancels the operation.
		bool wait = false
	);
	~GProgressDlg();

	/// Adds a pane. By default there is one pane.
	GProgressPane *Push();
	/// Pops off a pane
	void Pop(GProgressPane *p = 0);
	/// Gets the pane at index 'i'
	GProgressPane *ItemAt(int i);

	/// Returns the description of the first pane
	char *GetDescription();
	/// Sets the description of the first pane
	void SetDescription(const char *d);
	/// Returns the upper and lower limits of the first pane
	void GetLimits(int64 *l, int64 *h);
	/// Sets the upper and lower limits of the first pane
	void SetLimits(int64 l, int64 h);
	/// Returns the scaling factor of the first pane
	double GetScale();
	/// Sets the scaling factor of the first pane
	void SetScale(double s);
	/// Returns the current value of the first pane
	int64 Value();
	/// Sets the current value of the first pane
	void Value(int64 v);
	/// Returns the type description of the first pane
	const char *GetType();
	/// Sets the type description of the first pane
	void SetType(const char *t);
	/// Returns whether the user has cancelled the operation
	bool Cancel();
	/// Syncs an external list of progress objects with the dialog
	void OnSync(ProgressList *Prg);

	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	void OnCreate();
};

#endif
