#ifndef __GFILE_SELECT_H
#define __GFILE_SELECT_H

LgiFunc bool LgiGetUsersLinks(GArray<GString> &Links);

///////////////////////////////////////////////////////////////////////////////////////////////////
// File select dialog
class LgiClass GFileType : public GBase
{
	char *Ext;
	int _Data;

public:
	GFileType()
	{
		Ext = 0;
		_Data = 0;
	}

	~GFileType()
	{
		DeleteArray(Ext);
	}

	char *Extension() { return Ext; }
	bool Extension(const char *e) { return (Ext = NewStr(e)) != 0; }
	char *Description() { return Name(); }
	bool Description(const char *d) { return Name(d); }
	int Data() { return _Data; }
	void Data(int i) { _Data = i; }

	char *DefaultExtension();
};

/// \brief File selector dialog
///
/// Handles the UI for selecting files for opening and saving and selecting directories.
/// Uses the Win32 system dialogs on Windows and has it's own native window on Linux.
/// 
/// A simple example of this class in action:
/// \code
/// GFileSelect s;
/// s.Parent(MyWindow);
/// s.Type("PNG Files", "*.png");
/// if (s.Open())
/// {
/// 	LgiMsg(MyWindow, "The file selected is '%s'", "Example", MB_OK, s.Name());
/// }
/// \endcode
class LgiClass GFileSelect :
	public GBase
{
	class GFileSelectPrivate *d;

public:
	GFileSelect();
	~GFileSelect();

	// Properties
	
	/// Returns the first file name selected.
	char *Name();
	/// Sets the file name
	bool Name(const char *n);
	/// Returns the n'th file name selected
	char *operator [](size_t i);
	/// Returns the number of file names selected
	size_t Length();
	/// Returns the parent window
	GViewI *Parent();
	/// Sets the parent window
	void Parent(GViewI *Window);
	/// Returns whether the use can select multiple files
	bool MultiSelect();
	/// Sets whether the use can select multiple files
	void MultiSelect(bool Multi);
	/// Returns whether read only was selected
	bool ReadOnly();
	/// Sets whether the user sees a read only option
	void ShowReadOnly(bool ro);
	/// Gets the initial directory to open in
	char *InitialDir();
	/// Sets the initial directory to open in
	void InitialDir(const char *InitDir);
	/// Gets the title of the dialog box
	char *Title();
	/// Sets the title of the dialog box
	void Title(const char *Title);
	/// Gets the default extension to append to files selected without an extension
	char *DefaultExtension();
	/// Sets the default extension to append to files selected without an extension
	void DefaultExtension(const char *DefExt);

	// File types
	
	/// Returns the number of types in the type list
	size_t Types();
	/// Returns the 0 based index of the type selected in the type list
	ssize_t SelectedType();
	/// Returns the type into at a given index
	GFileType *TypeAt(ssize_t n);
	/// Adds a file type to the type filters list.
	bool Type
	(
		/// This full description of the file type
		const char *Description,
		/// The extension(s) of the file type: e.g: '*.png;*.gif;*.jpg'
		const char *Extension,
		/// Application defined 32-bit value.
		int Data = 0
	);
	/// Empties the type list
	void ClearTypes();

	// Methods
	
	/// Shows the open file dialog
	/// \returns true if the user selected a file, otherwise false
	bool Open();
	/// Shows the save file dialog
	/// \returns true if the user selected a file, otherwise false
	bool Save();
	/// Shows the open folder dialog
	/// \returns true if the user selected a folder, otherwise false
	bool OpenFolder();
};

#endif
