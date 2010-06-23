/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief An edit box control

#ifndef _GEDIT_H_
#define _GEDIT_H_

/// An edit box allowing the user to enter text
class LgiClass GEdit :
	public GControl,
	public ResObject
{
	/*
	#if defined BEOS
	class _OsEditFrame *Edit;
	#elif defined LINUX
	class _OsTextView *Edit;
	#endif
	*/

protected:
	class GEditPrivate *d;
	#if WIN32NATIVE
	int SysOnNotify(int Code);
	#endif

public:
	/// Constructor
	GEdit
	(
		/// Ctrl's ID
		int id,
		/// x coord
		int x,
		/// y coord
		int y,
		/// width
		int cx,
		/// height
		int cy,
		/// Initial text
		char *name
	);
	~GEdit();

	char *GetClass() { return "GEdit"; }

	/// Gets "Allow multiple lines"
	bool MultiLine();
	/// Sets "Allow multiple lines"
	void MultiLine(bool m);
	/// Is the text obsured by hashes?
	bool Password();
	/// Sets the text to be obsured by hashes.
	void Password(bool m);
	/// Interprets the text in the control as an integer
	void Value(int64 i);
	/// Sets the text in the control to an integer
	int64 Value();
	/// Selects a region of text
	void Select(int Start = 0, int Len = -1);
	/// Gets the Caret position in characters
	int GetCaret();
	/// Sets the Caret position in characters
	void SetCaret(int Pos);

	int OnEvent(GMessage *Msg);
	bool OnKey(GKey &k);
	char *Name();
	bool Name(char *s);
	char16 *NameW();
	bool NameW(char16 *s);
	
	#if WIN32NATIVE
	void OnAttach();
	#else
	void Enabled(bool e);
	bool Enabled();
	void Focus(bool f);
	bool Focus();
	bool SetPos(GRect &p, bool Repaint = false);
	int OnNotify(GViewI *c, int f);
	void OnCreate();
	#endif
};

#endif