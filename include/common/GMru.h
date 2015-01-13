#ifndef __GMRU_H
#define __GMRU_H

#ifdef HAS_PROPERTIES
#include "GProperties.h"
#endif

// Message defines
#define	IDM_OPEN			15000
#define	IDM_SAVEAS			15001

// Classes
class LgiClass GMru
{
private:
	class GMruPrivate *d;
	void _Update();

protected:
	virtual bool _OpenFile(char *File, bool ReadOnly);
	virtual bool _SaveFile(char *File);

	virtual char *_GetCurFile();
	virtual void GetFileTypes(GFileSelect *Dlg, bool Write);
	virtual GFileType *GetSelectedType();
	void DoFileDlg(GFileSelect &Select, bool Open);

	/// This method converts the storage reference (which can contain user/pass credentials) 
	/// between the display, raw and stored forms. Display and Raw are used at runtime and
	/// the stored form is written to disk.
	///
	/// The implementor should fill in any NULL strings by converting from the supplied forms.
	/// The caller must supply either the Raw or Stored form.
	virtual bool SerializeEntry
	(
		/// The displayable version of the reference (this should have any passwords blanked out)
		GAutoString *Display,
		/// The form passed to the client software to open/save. (passwords NOT blanked)
		GAutoString *Raw,
		/// The form safe to write to disk, if a password is present it must be encrypted.
		GAutoString *Stored
	);


public:
	GMru();
	virtual ~GMru();

	// Impl
	bool Set(GSubMenu *parent, int size = -1);
	char *AddFile(char *FileName, bool Update = true);
	void RemoveFile(char *FileName, bool Update = true);
	GMessage::Result OnEvent(GMessage *Msg);
	void OnCommand(int Cmd);

	// Serialization
	bool Serialize(GDom *Store, const char *Prefix, bool Write);
	#ifdef HAS_PROPERTIES
	bool Serialize(ObjProperties *Store, char *Prefix, bool Write);
	#endif

	// Events
	virtual bool OpenFile(char *FileName, bool ReadOnly) = 0;
	virtual bool SaveFile(char *FileName) = 0;
};

#endif
