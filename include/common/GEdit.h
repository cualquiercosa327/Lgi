/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief An edit box control

#ifndef _GEDIT_H_
#define _GEDIT_H_

#if !WIN32NATIVE
#include "GTextView3.h"
#endif

/// An edit box allowing the user to enter text
class LgiClass GEdit :
	#if WIN32NATIVE
	public GControl,
	public ResObject
	#else
	public GTextView3
	#endif
{
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
		const char *name
	);
	~GEdit();

	const char *GetClass() { return "GEdit"; }


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
	/// Get the current selection in characters
	bool GetSelection(int &Start, int &Len);
	/// Gets the Caret position in characters
	int GetCaret();
	/// Sets the Caret position in characters
	void SetCaret(int Pos);

	bool OnKey(GKey &k);
	
	#if WIN32NATIVE
	int OnEvent(GMessage *Msg);
	void OnAttach();
	char *Name();
	bool Name(const char *s);
	char16 *NameW();
	bool NameW(const char16 *s);
	#else
	void OnEnter(GKey &k);
	void SendNotify(int Data);
	bool OnLayout(GViewLayoutInfo &Inf) { return false; }
	#endif
};

#endif