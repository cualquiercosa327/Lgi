/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef __GFINDREPLACEDLG_H
#define __GFINDREPLACEDLG_H

/// Common find/replace window parameters
class LgiClass GFindReplaceCommon : public GDialog
{
	bool OnViewKey(GView *v, GKey &k);

public:
	/// The string to find
	GString Find;
	/// Whether to match a whole word
	bool MatchWord;
	/// Whether to match the case
	bool MatchCase;
	/// Whether to search only in the selection
	bool SelectionOnly;
	/// Whether to search only in the selection
	bool SearchUpwards;
	
	GFindReplaceCommon();
};

typedef bool (*GFrCallback)(GFindReplaceCommon *Dlg, bool Replace, void *User);

/// Find dialog
class LgiClass GFindDlg : public GFindReplaceCommon
{
	class GFindDlgPrivate *d;

public:
	/// Constructor
	GFindDlg
	(
		/// The parent window
		GView *Parent,
		/// The initial string for the find argument
		char *Init = 0,
		/// Callback
		GFrCallback Callback = 0,
		/// User defined data for callback
		void *UserData = 0
	);
	~GFindDlg();

	void OnCreate();
	void OnPosChange();
	int OnNotify(GViewI *Ctrl, int Flags);
};

/// The find command on the Replace dialog
#define IDC_FR_FIND					21002
/// The replace command on the Replace dialog
#define IDC_FR_REPLACE				21007

/// Replace dialog
/// 
/// \returns DoModal will return one of #IDC_FR_FIND, 
/// #IDC_FR_REPLACE, #IDCANCEL or #IDOK (which means 'Replace All', the default action)
class LgiClass GReplaceDlg : public GFindReplaceCommon
{
	class GReplaceDlgPrivate *d;

public:
	GString Replace;

	/// Constructor
	GReplaceDlg
	(
		/// The parent window
		GView *Parent,
		/// The initial value to find
		char *InitFind = 0,
		/// The initial value to replace with
		char *InitReplace = 0,
		/// Callback
		GFrCallback Callback = 0,
		/// User defined data for callback
		void *UserData = 0
	);
	~GReplaceDlg();

	void OnCreate();
	void OnPosChange();
	int OnNotify(GViewI *Ctrl, int Flags);
};


#endif

