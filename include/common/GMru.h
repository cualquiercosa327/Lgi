#ifndef __GMRU_H
#define __GMRU_H

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
	void DoFileDlg(bool Open);

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
	// bool Serialize(ObjProperties *Store, char *Prefix, bool Write);

	// Events
	virtual bool OpenFile(char *FileName, bool ReadOnly) = 0;
	virtual bool SaveFile(char *FileName) = 0;
};

#endif
