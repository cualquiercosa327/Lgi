/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief File association query and edit.

#ifndef _FILE_ASSOC_H_
#define _FILE_ASSOC_H_

/// A single file association action
class GFileAssocAction
{
public:
	char *App;
	char *Action;

	GFileAssocAction();
	~GFileAssocAction();
};

/// A file type, with a list of associated actions
class GFileAssoc
{
	class GFileAssocPrivate *d;

public:
	GFileAssoc(char *MimeType, char *Extension);
	~GFileAssoc();

	/// Returns the mimetype
	char *GetMimeType();

	/// Returns the extension
	char *GetExtension();

	/// Gets the extensions that map to this assocaition.
	bool GetExtensions(GArray<char*> &Ext);

	/// Gets the action for this association.
	bool GetActions(GArray<GFileAssocAction*> &Actions);

	/// Sets an action.
	bool SetAction(GFileAssocAction *Action);

	/// Sets the icon assocaited with the file type
	bool SetIcon(char *File, int Index);
};

#endif