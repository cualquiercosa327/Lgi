#ifndef _OPTION_FILE_H_
#define _OPTION_FILE_H_

#include "GVariant.h"
#include "GXmlTree.h"
#include "GMutex.h"

class LgiClass GOptionsFile : public GXmlTag, public GMutex
{
public:
	enum PortableType
	{
		UnknownMode,
		PortableMode,
		DesktopMode,
	};

private:
	GAutoString File;
	GAutoString Error;
	bool Dirty;
	
	char *LockFile;
	int LockLine;

	void _Init();
	bool _OnAccess(bool Start);

	// Don't allow non-threadsafe access to these
	char *GetAttr(const char *Name) { return GXmlTag::GetAttr(Name); }
	int GetAsInt(const char *Name) { return GXmlTag::GetAsInt(Name); }
	bool SetAttr(const char *Name, char *Value) { return GXmlTag::SetAttr(Name, Value); }
	bool SetAttr(const char *Name, int Value) { return GXmlTag::SetAttr(Name, Value); }
	bool DelAttr(const char *Name) { return GXmlTag::DelAttr(Name); }
	void InsertTag(GXmlTag *t) { GXmlTag::InsertTag(t); }
	void RemoveTag() { GXmlTag::RemoveTag(); }
	GXmlTag *GetTag(const char *Name, bool Create = false) { return GXmlTag::GetTag(Name, Create); }

protected:
	virtual void _Defaults() {}

public:
	GOptionsFile(const char *FileName = 0);
	GOptionsFile(PortableType Mode, const char *BaseName = 0);
	~GOptionsFile();

	void SetFile(const char *f);
	bool SetMode(PortableType Mode, const char *BaseName = 0);

	bool IsValid();
	char *GetFile() { return File; }
	char *GetError() { return Error; }
	bool Serialize(bool Write);
	bool DeleteValue(const char *Name);

	bool CreateTag(const char *Name);
	bool DeleteTag(const char *Name);
	GXmlTag *LockTag(const char *Name, const char *File, int Line);
};

#endif