
#ifndef __RES_H
#define __RES_H

// Includes
#include "GXmlTree.h"
// #include "GProperties.h"

/////////////////////////////////////////////////////////////////////
// Language stuff
enum ResFileFormat
{
	CodepageFile,		// The original format based on different codepages for each translations.
	Lr8File,			// The second format using utf-8 throughout instead.
	XmlFile,			// Straight XML.
};

typedef char *GLanguageId;

struct GLanguage
{
	char *Name;
	GLanguageId Id;
	int Win32Id;
	int OldId;
	char *CodePage;

	bool IsEnglish()
	{
		return	Id[0] == 'e' AND
				Id[1] == 'n' AND
				!Id[2];
	}

	bool operator ==(GLanguageId i)
	{
		return i AND stricmp(Id, i) == 0;
	}

	bool operator !=(GLanguageId i)
	{
		return !(*this == i);
	}
};

/*
class LangInfo
{
public:
	int Id;
	char *Name;
	char *CodePage;
	int Win32Id;
	char *Win32Define;
};
*/

LgiExtern GLanguage LgiLanguageTable[];
LgiExtern GLanguage *GFindLang(GLanguageId Id, char *Name = 0);
LgiExtern GLanguage *GFindOldLang(int OldId);

////////////////////////////////////////////////////////////////////
// All the names of the controls as strings
LgiExtern char Res_Dialog[];
LgiExtern char Res_Table[];
LgiExtern char Res_ControlTree[];
LgiExtern char Res_StaticText[];
LgiExtern char Res_EditBox[];
LgiExtern char Res_CheckBox[];
LgiExtern char Res_Button[];
LgiExtern char Res_Group[];
LgiExtern char Res_RadioBox[];
LgiExtern char Res_Tab[];
LgiExtern char Res_TabView[];
LgiExtern char Res_ListView[];
LgiExtern char Res_Column[];
LgiExtern char Res_TreeView[];
LgiExtern char Res_Bitmap[];
LgiExtern char Res_Progress[];
LgiExtern char Res_Slider[];
LgiExtern char Res_ComboBox[];
LgiExtern char Res_ScrollBar[];
LgiExtern char Res_Custom[];

// Classes
class ResObjectImpl;
class ResFactory;

class LgiClass ResParent
{
public:
	ResParent() {}
	virtual ~ResParent() {}

	virtual void *GetRealPtr() { return 0; }
};

class LgiClass ResObject : public ResParent
{
private:
	ResObjectImpl *_ObjImpl;

protected:
	char *_ObjName;

public:
	ResObject(char *Name);
	~ResObject();

	char *GetObjectName() { return _ObjName; }
	virtual ResObjectImpl *GetObjectImpl(ResFactory *f);
};

// Inherit from and implement to create objects for your system
class LgiClass ResFactory
{
public:
	// Use to read and write the objects
	bool Res_Read(ResObject *Obj, GXmlTag *Tag);
	bool Res_Write(ResObject *Obj, GXmlTag *Tag);

	// Overide
	virtual char *StringFromRef(int Ref) = 0;
	virtual ResObject *CreateObject(GXmlTag *Tag, ResObject *Parent) = 0;

	// Overide these to operate on the objects
	virtual void		Res_SetPos		(ResObject *Obj, int x1, int y1, int x2, int y2) = 0;
	virtual void		Res_SetPos		(ResObject *Obj, char *s) = 0;
	virtual GRect		Res_GetPos		(ResObject *Obj) = 0;
	virtual int			Res_GetStrRef	(ResObject *Obj) = 0;
	virtual void		Res_SetStrRef	(ResObject *Obj, int Ref) = 0;
	virtual void		Res_Attach		(ResObject *Obj, ResObject *Parent) = 0;
	virtual bool		Res_GetChildren	(ResObject *Obj, List<ResObject> *l, bool Deep) = 0;
	virtual void		Res_Append		(ResObject *Obj, ResObject *Parent) = 0;
	virtual bool		Res_GetItems	(ResObject *Obj, List<ResObject> *l) = 0;
	virtual GDom*		Res_GetDom		(ResObject *Obj) = 0;
	
	// Property access
	virtual bool		Res_GetProperties(ResObject *Obj, GDom *Props) = 0;
	virtual bool		Res_SetProperties(ResObject *Obj, GDom *Props) = 0;
};

#endif
